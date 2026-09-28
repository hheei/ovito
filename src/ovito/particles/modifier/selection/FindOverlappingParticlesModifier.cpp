// SPDX-FileCopyrightText: 2026 OVITO GmbH, Germany
// SPDX-License-Identifier: GPL-3.0-only OR MIT

#include "FindOverlappingParticlesModifier.h"
#include <ovito/core/dataset/pipeline/ModificationNode.h>
#include <ovito/particles/util/CutoffNeighborFinder.h>

namespace Ovito {

IMPLEMENT_CREATABLE_OVITO_CLASS(FindOverlappingParticlesModifier);
OVITO_CLASSINFO(FindOverlappingParticlesModifier, "DisplayName", "Find overlapping particles");
OVITO_CLASSINFO(FindOverlappingParticlesModifier, "ClassNameAlias", "SelectOverlappingParticlesModifier");

OVITO_CLASSINFO(FindOverlappingParticlesModifier, "Description", "Finds particles that are very close to each other.");
OVITO_CLASSINFO(FindOverlappingParticlesModifier, "ModifierCategory", "Selection");

DEFINE_PROPERTY_FIELD(FindOverlappingParticlesModifier, overlapDistance);
DEFINE_PROPERTY_FIELD(FindOverlappingParticlesModifier, applyToSelection);
DEFINE_PROPERTY_FIELD(FindOverlappingParticlesModifier, keepOne);
DEFINE_PROPERTY_FIELD(FindOverlappingParticlesModifier, moveToMeanPosition);

SET_PROPERTY_FIELD_LABEL(FindOverlappingParticlesModifier, overlapDistance, "Overlap distance");
SET_PROPERTY_FIELD_LABEL(FindOverlappingParticlesModifier, applyToSelection, "Use only selected particles");
SET_PROPERTY_FIELD_LABEL(FindOverlappingParticlesModifier, keepOne, "Keep one particle unselected on overlap");
SET_PROPERTY_FIELD_LABEL(FindOverlappingParticlesModifier, moveToMeanPosition, "Move overlapping particles to mean position");

SET_PROPERTY_FIELD_UNITS_AND_MINIMUM(FindOverlappingParticlesModifier, overlapDistance, WorldParameterUnit, 0);

/******************************************************************************
 * Asks the modifier whether it can be applied to the given input data.
 ******************************************************************************/
bool FindOverlappingParticlesModifier::OOMetaClass::isApplicableTo(const DataCollection& input) const
{
    return input.containsObject<Particles>();
}

/******************************************************************************
 * Modifies the input data.
 ******************************************************************************/
Future<PipelineFlowState> FindOverlappingParticlesModifier::evaluateModifier(const ModifierEvaluationRequest& request,
                                                                             PipelineFlowState&& state)
{
    // Early exit
    if(overlapDistance() == 0) {
        co_return std::move(state);
    }

    // Create local copies of co-routine input objects
    const ModifierEvaluationRequest request_l = request;
    PipelineFlowState state_l = std::move(state);

    const FloatType overlapDistance_l = overlapDistance();
    const bool applyToSelection_l = applyToSelection();
    const bool moveToMeanPosition_l = moveToMeanPosition();
    const bool keepOne_l = keepOne();

    // Get the input particles.
    const Particles* particles = state_l.expectObject<Particles>();
    particles->verifyIntegrity();

    // Get the required input properties.
    ConstPropertyPtr selProperty = applyToSelection_l ? particles->expectProperty(Particles::SelectionProperty) : nullptr;

    // Perform the following in a worker thread.
    co_await ExecutorAwaiter(ThreadPoolExecutor());

    TaskProgress progress(this_task::ui());
    progress.setText(tr("Selecting overlapping particles"));

    // Validate input simulation cell.
    // Warn if overlap distance exceeds periodicity lengths.
    const SimulationCell* cell = state_l.getObject<SimulationCell>();
    if(cell && keepOne_l) {
        for(size_t dim = 0; dim < 3; dim++) {
            if(cell->hasPbcCorrected(dim) && overlapDistance_l >= cell->cellMatrix().column(dim).length()) {
                state_l.setStatus(PipelineStatus(
                    PipelineStatus::Warning,
                    tr("Overlap distance exceeds simulation cell periodicity length. This likely leads to incorrect results.")));
                break;
            }
        }
    }

    // Output mask / selection property
    Particles* mutableParticles = state_l.expectMutableObject<Particles>();
    Property* outputSelectionProperty = mutableParticles->createProperty(Particles::SelectionProperty);
    BufferWriteAccess<SelectionIntType, access_mode::discard_write> outputSelection(outputSelectionProperty);
    std::ranges::fill(outputSelection, 0);

    // Modify the position particle property (only if moveToMeanPosition is enabled).
    BufferWriteAccess<Point3, access_mode::read_write> positionsAccess(
        moveToMeanPosition_l ?
        mutableParticles->expectMutableProperty(Particles::PositionProperty) : nullptr);

    // Get selection particle property.
    BufferReadAccess<SelectionIntType> selectionAcc(selProperty);

    CutoffNeighborFinder neighborFinder(overlapDistance_l, particles->expectProperty(Particles::PositionProperty), cell, selProperty);

    std::vector<size_t> neighs;

    // Note: Use std::mt19937 (not std::minstd_rand), whose recurrence is purely bitwise and thus
    // portable, unlike a linear congruential engine templated on the implementation-defined-width
    // std::uint_fast32_t. Combined with boost::random::uniform_int_distribution (whose mapping
    // algorithm is fixed by Boost rather than left implementation-defined like std::uniform_int_distribution),
    // this guarantees the same "particle to keep" is picked on every platform for a given seed.
    std::optional<std::mt19937> rng;
    if(keepOne_l) {
        rng.emplace(1323);
    }

    progress.setMaximum(particles->elementCount());
    const int progressInterval = std::max(int(particles->elementCount() / 100), 1);
    for(size_t particleIndex = 0; particleIndex < particles->elementCount(); particleIndex++) {
        progress.setValueIntermittent(particleIndex, progressInterval);

        // Skip particles that are not included in the analysis.
        if(selectionAcc && selectionAcc[particleIndex] == 0) continue;
        // Skip particles that have already been marked / processed.
        if(outputSelection[particleIndex] != 0) continue;

        Vector3 shift = Vector3::Zero();

        neighs.clear();
        neighs.emplace_back(particleIndex);
        for(CutoffNeighborFinder::Query neighQuery(neighborFinder, particleIndex); !neighQuery.atEnd(); neighQuery.next()) {
            const size_t neighIndex = neighQuery.current();
            neighs.emplace_back(neighIndex);
            if(moveToMeanPosition_l) {
                shift += neighQuery.delta();
            }
        }

        if(neighs.size() > 1) {
            if(keepOne_l) {
                OVITO_ASSERT(rng.has_value());
                const size_t keep = neighs[boost::random::uniform_int_distribution<size_t>(0, neighs.size() - 1)(*rng)];
                for(const size_t idx : neighs) {
                    // 2 is the marker for particles to keep
                    outputSelection[idx] = (idx == keep) ? 2 : 1;
                }
            }
            else {
                for(size_t neigh : neighs) {
                    outputSelection[neigh] = 1;
                }
            }
            if(moveToMeanPosition_l) {
                const Point3 meanPos = positionsAccess[particleIndex] + (shift / FloatType(neighs.size()));
                for(size_t neigh : neighs) {
                    positionsAccess[neigh] = meanPos;
                }
            }
        }
    }

    // Remove marker that a particle was designated as particle to keep
    std::ranges::replace(outputSelection, 2, 0);

    positionsAccess.reset();
    outputSelection.reset();

    const size_t selectedCount = outputSelectionProperty->nonzeroCount();
    state_l.addAttribute(QStringLiteral("FindOverlappingParticles.count"), QVariant::fromValue(selectedCount), request_l.modificationNode());
    state_l.combineStatus(PipelineStatus(tr("%1 out of %2 particles selected (%3%)")
                                             .arg(selectedCount)
                                             .arg(particles->elementCount())
                                             .arg(particles->elementCount() > 0 ? selectedCount * 100 / particles->elementCount() : 0)));

    co_return std::move(state_l);
}

}  // namespace Ovito