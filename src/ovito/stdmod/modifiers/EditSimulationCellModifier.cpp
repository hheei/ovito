// SPDX-FileCopyrightText: 2026 OVITO GmbH, Germany
// SPDX-License-Identifier: GPL-3.0-only OR MIT

#include <ovito/stdmod/StdMod.h>
#include <ovito/core/dataset/DataSet.h>
#include <ovito/core/dataset/pipeline/ModificationNode.h>
#include <ovito/stdobj/simcell/SimulationCell.h>
#include "EditSimulationCellModifier.h"

namespace Ovito {

IMPLEMENT_CREATABLE_OVITO_CLASS(EditSimulationCellModifier);
OVITO_CLASSINFO(EditSimulationCellModifier, "DisplayName", "Edit simulation cell");
OVITO_CLASSINFO(EditSimulationCellModifier, "Description", "Edit the simulation cell parameters and boundary conditions.");
OVITO_CLASSINFO(EditSimulationCellModifier, "ModifierCategory", "Modification");
DEFINE_PROPERTY_FIELD(EditSimulationCellModifier, cellMatrix);
DEFINE_PROPERTY_FIELD(EditSimulationCellModifier, replaceCell);
DEFINE_PROPERTY_FIELD(EditSimulationCellModifier, pbcX);
DEFINE_PROPERTY_FIELD(EditSimulationCellModifier, pbcY);
DEFINE_PROPERTY_FIELD(EditSimulationCellModifier, pbcZ);
DEFINE_PROPERTY_FIELD(EditSimulationCellModifier, is2D);
SET_PROPERTY_FIELD_LABEL(EditSimulationCellModifier, cellMatrix, "Cell matrix");
SET_PROPERTY_FIELD_LABEL(EditSimulationCellModifier, replaceCell, "Override cell");
SET_PROPERTY_FIELD_LABEL(EditSimulationCellModifier, pbcX, "Periodic boundary conditions (X)");
SET_PROPERTY_FIELD_LABEL(EditSimulationCellModifier, pbcY, "Periodic boundary conditions (Y)");
SET_PROPERTY_FIELD_LABEL(EditSimulationCellModifier, pbcZ, "Periodic boundary conditions (Z)");
SET_PROPERTY_FIELD_LABEL(EditSimulationCellModifier, is2D, "2D");
SET_PROPERTY_FIELD_UNITS(EditSimulationCellModifier, cellMatrix, WorldParameterUnit);

/******************************************************************************
* This method is called by the system when the modifier has been inserted
* into a pipeline.
******************************************************************************/
void EditSimulationCellModifier::initializeModifier(const ModifierInitializationRequest& request)
{
    Modifier::initializeModifier(request);

    // When the modifier is first inserted, automatically adopt those parameters from the input cell which
    // have not been set explicitly by the user or a script yet. Each parameter is considered on its own, so that
    // setting one of them (e.g. 'pbc_z' from Python) does not silently reset the others to their default values.
    if(cellMatrix() == AffineTransformation::Zero() || _uninitializedPbcX || _uninitializedPbcY || _uninitializedPbcZ || _uninitializedDimensionality) {
        const PipelineFlowState& input = request.modificationNode()->evaluateInput(request).blockForResult();
        if(const SimulationCell* inputCell = input.getObject<SimulationCell>()) {
            if(cellMatrix() == AffineTransformation::Zero())
                setCellMatrix(inputCell->cellMatrix());
            if(_uninitializedPbcX)
                setPbcX(inputCell->pbcX());
            if(_uninitializedPbcY)
                setPbcY(inputCell->pbcY());
            if(_uninitializedPbcZ)
                setPbcZ(inputCell->pbcZ());
            if(_uninitializedDimensionality)
                setIs2D(inputCell->is2D());
        }
    }
}

/******************************************************************************
* Is called when the value of a property of this object has changed.
******************************************************************************/
void EditSimulationCellModifier::propertyChanged(const PropertyFieldDescriptor* field)
{
    // User has explicitly set one of the PBC parameters. Note that each direction is tracked separately, so that
    // the remaining ones can still be inherited from the input cell in initializeModifier().
    if(field == PROPERTY_FIELD(pbcX)) {
        _uninitializedPbcX = false;
    }
    if(field == PROPERTY_FIELD(pbcY)) {
        _uninitializedPbcY = false;
    }
    if(field == PROPERTY_FIELD(pbcZ)) {
        _uninitializedPbcZ = false;
    }
    if(field == PROPERTY_FIELD(is2D)) {
        _uninitializedDimensionality = false; // User has explicitly set dimensionality parameter.
    }

    Modifier::propertyChanged(field);
}

/******************************************************************************
* Modifies the input data.
******************************************************************************/
Future<PipelineFlowState> EditSimulationCellModifier::evaluateModifier(const ModifierEvaluationRequest& request, PipelineFlowState&& state)
{
    SimulationCell* cell = state.expectMutableObject<SimulationCell>();

    cell->setIs2D(is2D());
    cell->setPbcFlags(pbcX(), pbcY(), !is2D() && pbcZ());
    if(replaceCell())
        cell->setCellMatrix(cellMatrix());

    return std::move(state);
}

}   // End of namespace
