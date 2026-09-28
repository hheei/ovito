// SPDX-FileCopyrightText: 2026 OVITO GmbH, Germany
// SPDX-License-Identifier: GPL-3.0-only OR MIT

#include <ovito/particles/Particles.h>
#include <ovito/particles/util/NearestNeighborFinder.h>
#include <ovito/stdobj/properties/Property.h>
#include <ovito/stdobj/table/DataTable.h>
#include <ovito/stdobj/simcell/SimulationCell.h>
#include <ovito/core/utilities/concurrent/ParallelFor.h>
#include <ovito/core/utilities/concurrent/EnumerableThreadSpecific.h>
#include <ovito/core/utilities/units/UnitsManager.h>
#include <ovito/core/dataset/pipeline/ModificationNode.h>
#include <ovito/core/dataset/DataSet.h>
#include "PolyhedralTemplateMatchingModifier.h"

namespace Ovito {

IMPLEMENT_CREATABLE_OVITO_CLASS(PolyhedralTemplateMatchingModifier);
OVITO_CLASSINFO(PolyhedralTemplateMatchingModifier, "DisplayName", "Polyhedral template matching");
OVITO_CLASSINFO(PolyhedralTemplateMatchingModifier, "Description", "Identify structures using the PTM method and local crystal orientations.");
OVITO_CLASSINFO(PolyhedralTemplateMatchingModifier, "ModifierCategory", "Structure identification");
DEFINE_PROPERTY_FIELD(PolyhedralTemplateMatchingModifier, rmsdCutoff);
DEFINE_PROPERTY_FIELD(PolyhedralTemplateMatchingModifier, outputRmsd);
DEFINE_PROPERTY_FIELD(PolyhedralTemplateMatchingModifier, outputInteratomicDistance);
DEFINE_PROPERTY_FIELD(PolyhedralTemplateMatchingModifier, outputOrientation);
DEFINE_PROPERTY_FIELD(PolyhedralTemplateMatchingModifier, outputDeformationGradient);
DEFINE_PROPERTY_FIELD(PolyhedralTemplateMatchingModifier, outputOrderingTypes);
DEFINE_VECTOR_REFERENCE_FIELD(PolyhedralTemplateMatchingModifier, orderingTypes);
SET_PROPERTY_FIELD_LABEL(PolyhedralTemplateMatchingModifier, rmsdCutoff, "RMSD cutoff");
SET_PROPERTY_FIELD_LABEL(PolyhedralTemplateMatchingModifier, outputRmsd, "Output RMSD values");
SET_PROPERTY_FIELD_LABEL(PolyhedralTemplateMatchingModifier, outputInteratomicDistance, "Output interatomic distance");
SET_PROPERTY_FIELD_LABEL(PolyhedralTemplateMatchingModifier, outputOrientation, "Output lattice orientations");
SET_PROPERTY_FIELD_LABEL(PolyhedralTemplateMatchingModifier, outputDeformationGradient, "Output deformation gradients");
SET_PROPERTY_FIELD_LABEL(PolyhedralTemplateMatchingModifier, outputOrderingTypes, "Output ordering types");
SET_PROPERTY_FIELD_LABEL(PolyhedralTemplateMatchingModifier, orderingTypes, "Ordering types");
SET_PROPERTY_FIELD_UNITS_AND_MINIMUM(PolyhedralTemplateMatchingModifier, rmsdCutoff, FloatParameterUnit, 0);

/******************************************************************************
* Constructor.
******************************************************************************/
void PolyhedralTemplateMatchingModifier::initializeObject(ObjectInitializationFlags flags)
{
    StructureIdentificationModifier::initializeObject(flags);

    if(!flags.testFlag(ObjectInitializationFlag::DontInitializeObject)) {
        // Define the structure types.
        createStructureType(PTMAlgorithm::OTHER, ParticleType::PredefinedStructureType::OTHER);
        createStructureType(PTMAlgorithm::FCC, ParticleType::PredefinedStructureType::FCC);
        createStructureType(PTMAlgorithm::HCP, ParticleType::PredefinedStructureType::HCP);
        createStructureType(PTMAlgorithm::BCC, ParticleType::PredefinedStructureType::BCC);
        createStructureType(PTMAlgorithm::ICO, ParticleType::PredefinedStructureType::ICO)->setEnabled(false);
        createStructureType(PTMAlgorithm::SC, ParticleType::PredefinedStructureType::SC)->setEnabled(false);
        createStructureType(PTMAlgorithm::CUBIC_DIAMOND, ParticleType::PredefinedStructureType::CUBIC_DIAMOND)->setEnabled(false);
        createStructureType(PTMAlgorithm::HEX_DIAMOND, ParticleType::PredefinedStructureType::HEX_DIAMOND)->setEnabled(false);
        createStructureType(PTMAlgorithm::GRAPHENE, ParticleType::PredefinedStructureType::GRAPHENE)->setEnabled(false);

        // Define the ordering types.
        for(int id = 0; id < PTMAlgorithm::NUM_ORDERING_TYPES; id++) {
            OORef<ParticleType> otype = OORef<ParticleType>::create(flags);
            otype->initializeType([&]() {
                otype->setNumericId(id);
            }, OwnerPropertyRef(&Particles::OOClass(), QStringLiteral("Ordering Type")));
            _orderingTypes.push_back(this, PROPERTY_FIELD(orderingTypes), std::move(otype));
        }
        orderingTypes()[PTMAlgorithm::ORDERING_NONE]->setName(tr("Other"));
        orderingTypes()[PTMAlgorithm::ORDERING_PURE]->setName(tr("Pure"));
        orderingTypes()[PTMAlgorithm::ORDERING_L10]->setName(tr("L10"));
        orderingTypes()[PTMAlgorithm::ORDERING_L12_A]->setName(tr("L12 (A-site)"));
        orderingTypes()[PTMAlgorithm::ORDERING_L12_B]->setName(tr("L12 (B-site)"));
        orderingTypes()[PTMAlgorithm::ORDERING_B2]->setName(tr("B2"));
        orderingTypes()[PTMAlgorithm::ORDERING_ZINCBLENDE_WURTZITE]->setName(tr("Zincblende/Wurtzite"));
        orderingTypes()[PTMAlgorithm::ORDERING_BORON_NITRIDE]->setName(tr("Boron/Nitride"));

        orderingTypes()[PTMAlgorithm::ORDERING_NONE]->setColor({0.95f, 0.95f, 0.95f});
        orderingTypes()[PTMAlgorithm::ORDERING_PURE]->setColor({0.40f, 0.62f, 1.00f});
        orderingTypes()[PTMAlgorithm::ORDERING_L10]->setColor({1.00f, 0.60f, 0.20f});
        orderingTypes()[PTMAlgorithm::ORDERING_L12_A]->setColor({0.35f, 0.85f, 0.35f});
        orderingTypes()[PTMAlgorithm::ORDERING_L12_B]->setColor({1.00f, 0.35f, 0.35f});
        orderingTypes()[PTMAlgorithm::ORDERING_B2]->setColor({0.75f, 0.40f, 1.00f});
        orderingTypes()[PTMAlgorithm::ORDERING_ZINCBLENDE_WURTZITE]->setColor({0.20f, 0.88f, 0.90f});
        orderingTypes()[PTMAlgorithm::ORDERING_BORON_NITRIDE]->setColor({1.00f, 0.90f, 0.20f});
    }
}

/******************************************************************************
* Creates the engine that will perform the structure identification.
******************************************************************************/
std::shared_ptr<StructureIdentificationModifier::Algorithm> PolyhedralTemplateMatchingModifier::createAlgorithm(const ModifierEvaluationRequest& request, const PipelineFlowState& input)
{
    const Particles* particles = input.expectObject<Particles>();

    // Get input particle types if needed.
    const Property* typeProperty = outputOrderingTypes() ? particles->expectProperty(Particles::TypeProperty) : nullptr;

    return std::make_shared<PTMEngine>(*this, input, typeProperty,
            orderingTypes(), outputInteratomicDistance(), outputOrientation(), outputDeformationGradient());
}

/******************************************************************************
* Compute engine constructor.
******************************************************************************/
PolyhedralTemplateMatchingModifier::PTMEngine::PTMEngine(const StructureIdentificationModifier& modifier, const PipelineFlowState& input, ConstPropertyPtr particleTypes,
        const OORefVector<ElementType>& orderingTypes, bool outputInteratomicDistance, bool outputOrientation, bool outputDeformationGradient) :
    Algorithm(modifier, input),
    _rmsd(Particles::OOClass().createUserProperty(DataBuffer::Uninitialized, particles()->elementCount(), Property::FloatDefault, 1, QStringLiteral("RMSD"))),
    _interatomicDistances(outputInteratomicDistance ? Particles::OOClass().createUserProperty(DataBuffer::Initialized, particles()->elementCount(), Property::FloatDefault, 1, QStringLiteral("Interatomic Distance")) : nullptr),
    _orientations(outputOrientation ? Particles::OOClass().createStandardProperty(DataBuffer::Initialized, particles()->elementCount(), Particles::OrientationProperty) : nullptr),
    _deformationGradients(outputDeformationGradient ? Particles::OOClass().createStandardProperty(DataBuffer::Initialized, particles()->elementCount(), Particles::ElasticDeformationGradientProperty) : nullptr),
    _orderingTypes(particleTypes ? Particles::OOClass().createUserProperty(DataBuffer::Initialized, particles()->elementCount(), Property::Int32, 1, QStringLiteral("Ordering Type")) : nullptr),
    _correspondences(outputOrientation ? Particles::OOClass().createUserProperty(DataBuffer::Initialized, particles()->elementCount(), Property::Int64, 1, QStringLiteral("Correspondences")) : nullptr),    // only output correspondences if orientations are selected
    _rmsdHistogram(DataTable::OOClass().createUserProperty(DataBuffer::Initialized, 100, Property::Int64, 1, tr("Count"))),
    _outputDeformationGradient(outputDeformationGradient),
    _particleTypes(particleTypes)
{
    // Attach ordering types to output particle property.
    if(_orderingTypes) {
        // Create deep copies of the elements types, because data objects owned by the modifier should
        // not be passed to the data pipeline.
        for(const ElementType* type : orderingTypes) {
            // Attach element type to output particle property.
            _orderingTypes->addElementType(DataOORef<ElementType>::makeDeepCopy(type));
        }
    }
}

/******************************************************************************
* Performs the actual analysis.
******************************************************************************/
void PolyhedralTemplateMatchingModifier::PTMEngine::identifyStructures()
{
    if(simulationCell().is2D())
        throw Exception(tr("The PTM algorithm does not support 2d simulation cells."));

    // Initialize the PTM algorithm object.
    _algorithm.emplace(particles()->expectProperty(Particles::PositionProperty), simulationCell(), particleSelection());

    _algorithm->setCalculateDefGradient(_outputDeformationGradient);
    _algorithm->setIdentifyOrdering(std::move(_particleTypes));
    _algorithm->setRmsdCutoff(0.0); // Note: We do our own RMSD threshold filtering in postProcessStructureTypes().

    // Specify the structure types the PTM should look for.
    for(int i = 0; i < PTMAlgorithm::NUM_STRUCTURE_TYPES; i++) {
        _algorithm->setStructureTypeIdentification(static_cast<PTMAlgorithm::StructureType>(i), typeIdentificationEnabled(i));
    }

    // Get access to the particle selection flags.
    BufferReadAccess<SelectionIntType> selectionAcc(particleSelection());

    TaskProgress progress(this_task::ui());
    progress.setText(tr("Pre-calculating neighbor ordering"));

    // Pre-order neighbors of each particle.
    std::vector<uint64_t> cachedNeighbors(particles()->elementCount());

    EnumerableThreadSpecific<PTMAlgorithm::Kernel> ptmKernels;
    parallelForInnerOuter(particles()->elementCount(), 1024, progress, [&](auto&& iterate) {
        // Create a thread-local kernel for the PTM algorithm.
        PTMAlgorithm::Kernel& kernel = ptmKernels.create(*_algorithm);
        iterate([&](size_t index) {
            // Skip particles that are not included in the analysis.
            if(selectionAcc && !selectionAcc[index])
                return;

            // Calculate ordering of neighbors
            kernel.cacheNeighbors(index, &cachedNeighbors[index]);
        });
    });

    progress.setText(tr("Performing polyhedral template matching"));

    // Get access to the output buffers that will receive the identified particle types and other data.
    BufferWriteAccess<int32_t, access_mode::discard_read_write> outputStructureArray(structures());
    BufferWriteAccess<FloatType, access_mode::discard_read_write> rmsdArray(rmsd());
    BufferWriteAccess<FloatType, access_mode::write> interatomicDistancesArray(interatomicDistances());
    BufferWriteAccess<QuaternionG, access_mode::write> orientationsArray(orientations());
    BufferWriteAccess<Matrix3, access_mode::write> deformationGradientsArray(deformationGradients());
    BufferWriteAccess<int32_t, access_mode::write> orderingTypesArray(orderingTypes());
    BufferWriteAccess<int64_t, access_mode::write> correspondencesArray(correspondences());

    // Perform analysis on each particle.
    parallelForInnerOuter(particles()->elementCount(), 1024, progress, [&](auto&& iterate) {
        PTMAlgorithm::Kernel& kernel = ptmKernels.create(*_algorithm);
        iterate([&](size_t index) {
            // Skip particles that are not included in the analysis.
            if(selectionAcc && !selectionAcc[index]) {
                outputStructureArray[index] = PTMAlgorithm::OTHER;
                rmsdArray[index] = 0;
                return;
            }

            // Perform the PTM analysis for the current particle.
            PTMAlgorithm::StructureType type = kernel.identifyStructure(index, cachedNeighbors);

            // Store results in the output arrays.
            outputStructureArray[index] = type;
            rmsdArray[index] = kernel.rmsd();
            if(type != PTMAlgorithm::OTHER) {
                if(interatomicDistancesArray) interatomicDistancesArray[index] = kernel.interatomicDistance();
                if(deformationGradientsArray) deformationGradientsArray[index] = kernel.deformationGradient();
                if(orientationsArray) orientationsArray[index] = kernel.orientation().toDataType<GraphicsFloatType>();
                if(orderingTypesArray) orderingTypesArray[index] = kernel.orderingType();
                if(correspondencesArray) correspondencesArray[index] = kernel.correspondence();
            }
        });
    });

    // Determine histogram bin size based on maximum RMSD value.
    const size_t numHistogramBins = _rmsdHistogram->size();
    FloatType rmsdHistogramBinSize = (rmsdArray.size() != 0) ? (FloatType(1.01) * *std::ranges::max_element(rmsdArray) / numHistogramBins) : 0;
    if(rmsdHistogramBinSize <= 0) rmsdHistogramBinSize = 1;
    _rmsdHistogramRange = rmsdHistogramBinSize * numHistogramBins;

    // Perform binning of RMSD values.
    if(outputStructureArray.size() != 0) {
        BufferWriteAccess<int64_t, access_mode::read_write> histogramCounts(_rmsdHistogram);
        const int32_t* structureType = outputStructureArray.cbegin();
        for(FloatType rmsdValue : rmsdArray) {
            if(*structureType++ != PTMAlgorithm::OTHER) {
                OVITO_ASSERT(rmsdValue >= 0);
                int binIndex = rmsdValue / rmsdHistogramBinSize;
                if(binIndex < numHistogramBins)
                    histogramCounts[binIndex]++;
            }
        }
    }

    // Release data that is no longer needed.
    _algorithm.reset();
}

/******************************************************************************
* Injects the computed results of the engine into the data pipeline.
******************************************************************************/
PropertyPtr PolyhedralTemplateMatchingModifier::PTMEngine::postProcessStructureTypes(const PropertyPtr& structures, const std::any& modifierParameters) const
{
    FloatType rmsdCutoff = std::any_cast<std::pair<FloatType, bool>>(modifierParameters).first;

    // Enforce RMSD cutoff.
    if(rmsdCutoff > 0 && rmsd()) {

        // Start off with the original particle classifications and make a copy.
        PropertyPtr finalStructureTypes = structures.makeCopy();

        // Mark those particles whose RMSD exceeds the cutoff as 'OTHER'.
        BufferReadAccess<FloatType> rmdsArray(rmsd());
        BufferWriteAccess<int32_t, access_mode::write> structureTypesArray(finalStructureTypes);
        const FloatType* rmsdValue = rmdsArray.cbegin();
        for(int32_t& type : structureTypesArray) {
            if(*rmsdValue++ > rmsdCutoff)
                type = PTMAlgorithm::OTHER;
        }

        // Replace old classifications with updated ones.
        return finalStructureTypes;
    }
    else {
        return structures;
    }
}

/******************************************************************************
* Computes the structure identification statistics.
******************************************************************************/
std::vector<int64_t> PolyhedralTemplateMatchingModifier::PTMEngine::computeStructureStatistics(const Property* structures, PipelineFlowState& state, const OOWeakRef<const PipelineNode>& createdByNode, const std::any& modifierParameters) const
{
    std::vector<int64_t> structureTypeCounts = StructureIdentificationModifier::Algorithm::computeStructureStatistics(structures, state, createdByNode, modifierParameters);

    // Also output structure type counts, which have been computed by the base class.
    state.addAttribute(QStringLiteral("PolyhedralTemplateMatching.counts.OTHER"), QVariant::fromValue(structureTypeCounts.at(PTMAlgorithm::OTHER)), createdByNode);
    state.addAttribute(QStringLiteral("PolyhedralTemplateMatching.counts.FCC"), QVariant::fromValue(structureTypeCounts.at(PTMAlgorithm::FCC)), createdByNode);
    state.addAttribute(QStringLiteral("PolyhedralTemplateMatching.counts.HCP"), QVariant::fromValue(structureTypeCounts.at(PTMAlgorithm::HCP)), createdByNode);
    state.addAttribute(QStringLiteral("PolyhedralTemplateMatching.counts.BCC"), QVariant::fromValue(structureTypeCounts.at(PTMAlgorithm::BCC)), createdByNode);
    state.addAttribute(QStringLiteral("PolyhedralTemplateMatching.counts.ICO"), QVariant::fromValue(structureTypeCounts.at(PTMAlgorithm::ICO)), createdByNode);
    state.addAttribute(QStringLiteral("PolyhedralTemplateMatching.counts.SC"), QVariant::fromValue(structureTypeCounts.at(PTMAlgorithm::SC)), createdByNode);
    state.addAttribute(QStringLiteral("PolyhedralTemplateMatching.counts.CUBIC_DIAMOND"), QVariant::fromValue(structureTypeCounts.at(PTMAlgorithm::CUBIC_DIAMOND)), createdByNode);
    state.addAttribute(QStringLiteral("PolyhedralTemplateMatching.counts.HEX_DIAMOND"), QVariant::fromValue(structureTypeCounts.at(PTMAlgorithm::HEX_DIAMOND)), createdByNode);
    state.addAttribute(QStringLiteral("PolyhedralTemplateMatching.counts.GRAPHENE"), QVariant::fromValue(structureTypeCounts.at(PTMAlgorithm::GRAPHENE)), createdByNode);
    this_task::throwIfCanceled();

    if(orderingTypes()) {
        // Count the number of particles of each identified chemical ordering type.
        int maxTypeId = 0;
        for(const ElementType* ctype : orderingTypes()->elementTypes()) {
            OVITO_ASSERT(ctype->numericId() >= 0);
            maxTypeId = std::max(maxTypeId, ctype->numericId());
        }
        std::vector<int64_t> chemicalTypeCounts(maxTypeId + 1, 0);
        for(auto t : BufferReadAccess<int32_t>(orderingTypes())) {
            if(t >= 0 && t <= maxTypeId)
                chemicalTypeCounts[t]++;
        }
        this_task::throwIfCanceled();

        // Create the property arrays for the bar chart.
        PropertyPtr typeCounts = DataTable::OOClass().createUserProperty(DataBuffer::Uninitialized, maxTypeId + 1, Property::Int64, 1, tr("Count"));
        std::ranges::copy(chemicalTypeCounts, BufferWriteAccess<int64_t, access_mode::discard_write>(typeCounts).begin());
        PropertyPtr typeIds = DataTable::OOClass().createUserProperty(DataBuffer::Uninitialized, maxTypeId + 1, Property::Int32, 1, tr("Ordering Type"));
        boost::algorithm::iota_n(BufferWriteAccess<int32_t, access_mode::discard_write>(typeIds).begin(), 0, typeIds->size());

        // Use the chemical ordering types as labels for the output bar chart.
        for(const ElementType* type : orderingTypes()->elementTypes()) {
            if(type->enabled())
                typeIds->addElementType(type);
        }

        // Output a bar chart with the type counts.
        state.createObject<DataTable>(QStringLiteral("chemical-ordering"), createdByNode, DataTable::BarChart, tr("Chemical ordering counts"), std::move(typeCounts), std::move(typeIds));
        this_task::throwIfCanceled();

        // Output type counts as global attributes.
        state.addAttribute(QStringLiteral("PolyhedralTemplateMatching.ordering_counts.NONE"), QVariant::fromValue(chemicalTypeCounts.at(PTMAlgorithm::ORDERING_NONE)), createdByNode);
        state.addAttribute(QStringLiteral("PolyhedralTemplateMatching.ordering_counts.PURE"), QVariant::fromValue(chemicalTypeCounts.at(PTMAlgorithm::ORDERING_PURE)), createdByNode);
        state.addAttribute(QStringLiteral("PolyhedralTemplateMatching.ordering_counts.L10"), QVariant::fromValue(chemicalTypeCounts.at(PTMAlgorithm::ORDERING_L10)), createdByNode);
        state.addAttribute(QStringLiteral("PolyhedralTemplateMatching.ordering_counts.L12_A"), QVariant::fromValue(chemicalTypeCounts.at(PTMAlgorithm::ORDERING_L12_A)), createdByNode);
        state.addAttribute(QStringLiteral("PolyhedralTemplateMatching.ordering_counts.L12_B"), QVariant::fromValue(chemicalTypeCounts.at(PTMAlgorithm::ORDERING_L12_B)), createdByNode);
        state.addAttribute(QStringLiteral("PolyhedralTemplateMatching.ordering_counts.B2"), QVariant::fromValue(chemicalTypeCounts.at(PTMAlgorithm::ORDERING_B2)), createdByNode);
        state.addAttribute(QStringLiteral("PolyhedralTemplateMatching.ordering_counts.ZINCBLENDE_WURTZITE"), QVariant::fromValue(chemicalTypeCounts.at(PTMAlgorithm::ORDERING_ZINCBLENDE_WURTZITE)), createdByNode);
        state.addAttribute(QStringLiteral("PolyhedralTemplateMatching.ordering_counts.BORON_NITRIDE"), QVariant::fromValue(chemicalTypeCounts.at(PTMAlgorithm::ORDERING_BORON_NITRIDE)), createdByNode);
    }

    Particles* particles = state.expectMutableObject<Particles>();

    // Output per-particle properties.
    bool outputRmsd = std::any_cast<std::pair<FloatType, bool>>(modifierParameters).second;
    if(rmsd() && outputRmsd) {
        particles->createProperty(rmsd());
    }
    if(interatomicDistances()) {
        particles->createProperty(interatomicDistances());
    }
    if(orientations()) {
        particles->createProperty(orientations());
    }
    if(correspondences()) {
        particles->createProperty(correspondences());
    }
    if(deformationGradients()) {
        particles->createProperty(deformationGradients());
    }
    if(orderingTypes()) {
        particles->createProperty(orderingTypes());
    }

    // Output RMSD histogram.
    DataTable* table = state.createObject<DataTable>(QStringLiteral("ptm-rmsd"), createdByNode, DataTable::Line, tr("RMSD distribution"), rmsdHistogram());
    table->setAxisLabelX(tr("RMSD"));
    table->setIntervalStart(0);
    table->setIntervalEnd(rmsdHistogramRange());

    return structureTypeCounts;
}

}   // End of namespace
