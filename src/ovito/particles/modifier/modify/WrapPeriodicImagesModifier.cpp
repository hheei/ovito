// SPDX-FileCopyrightText: 2026 OVITO GmbH, Germany
// SPDX-License-Identifier: GPL-3.0-only OR MIT

#include <ovito/particles/Particles.h>
#include <ovito/particles/objects/Particles.h>
#include <ovito/particles/objects/Bonds.h>
#include <ovito/stdobj/simcell/SimulationCell.h>
#include <ovito/core/dataset/pipeline/ModificationNode.h>
#include "WrapPeriodicImagesModifier.h"

namespace Ovito {

IMPLEMENT_CREATABLE_OVITO_CLASS(WrapPeriodicImagesModifier);
OVITO_CLASSINFO(WrapPeriodicImagesModifier, "DisplayName", "Wrap at periodic boundaries");
OVITO_CLASSINFO(WrapPeriodicImagesModifier, "Description", "Fold particle coordinates back into the periodic simulation box.");
OVITO_CLASSINFO(WrapPeriodicImagesModifier, "ModifierCategory", "Modification");

/******************************************************************************
* Asks the modifier whether it can be applied to the given input data.
******************************************************************************/
bool WrapPeriodicImagesModifier::OOMetaClass::isApplicableTo(const DataCollection& input) const
{
    return input.containsObject<Particles>() && input.containsObject<SimulationCell>();
}

/******************************************************************************
* Modifies the input data.
******************************************************************************/
Future<PipelineFlowState> WrapPeriodicImagesModifier::evaluateModifier(const ModifierEvaluationRequest& request, PipelineFlowState&& state)
{
    // Get the periodic simulation cell.
    const SimulationCell* simCell = state.expectObject<SimulationCell>();
    if(!simCell->hasPbcCorrected()) {
        state.setStatus(PipelineStatus(PipelineStatus::Warning, tr("No periodic boundary conditions are enabled for the simulation cell.")));
        return std::move(state);
    }

    // The actual work can be performed in a separate thread.
    return asyncLaunch([state = std::move(state), simCell]() mutable
    {
        // Make a modifiable copy of the particles object.
        Particles* outputParticles = state.expectMutableObject<Particles>();
        outputParticles->verifyIntegrity();

        // Perform the actual coordinate wrapping.
        outputParticles->wrapCoordinates(*simCell);

        // Return the modified data.
        return std::move(state);
    });
}

}   // End of namespace
