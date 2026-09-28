// SPDX-FileCopyrightText: 2026 OVITO GmbH, Germany
// SPDX-License-Identifier: GPL-3.0-only OR MIT

#include <ovito/grid/Grid.h>
#include <ovito/grid/objects/VoxelGrid.h>
#include <ovito/stdobj/simcell/SimulationCell.h>
#include <ovito/core/dataset/pipeline/ModificationNode.h>
#include "VoxelGridAffineTransformationModifierDelegate.h"

namespace Ovito {

IMPLEMENT_CREATABLE_OVITO_CLASS(VoxelGridAffineTransformationModifierDelegate);
OVITO_CLASSINFO(VoxelGridAffineTransformationModifierDelegate, "DisplayName", "Voxel grids");

/******************************************************************************
* Indicates which data objects in the given input data collection the modifier
* delegate is able to operate on.
******************************************************************************/
QVector<DataObjectReference> VoxelGridAffineTransformationModifierDelegate::OOMetaClass::getApplicableObjects(const DataCollection& input) const
{
    // Gather list of all voxel grids in the input data collection.
    QVector<DataObjectReference> objects;
    for(const ConstDataObjectPath& path : input.getObjectsRecursive(VoxelGrid::OOClass())) {
        objects.push_back(path);
    }
    return objects;
}

/******************************************************************************
* Applies the modifier operation to the data in a pipeline flow state.
******************************************************************************/
Future<PipelineFlowState> VoxelGridAffineTransformationModifierDelegate::apply(const ModifierEvaluationRequest& request, PipelineFlowState&& state, const PipelineFlowState& originalState, const std::vector<PipelineFlowState>& additionalInputs)
{
    AffineTransformationModifier* modifier = static_object_cast<AffineTransformationModifier>(request.modifier());

    // Transform the spatial domains of all VoxelGrid objects.
    visitObjectsToBeProcessed<VoxelGrid>(state, inputDataObject(), request.modificationNodeWeak(), [&](const VoxelGrid* existingObject) {
        if(existingObject->domain()) {

            // Determine transformation matrix.
            const AffineTransformation tm = modifier->effectiveAffineTransformation(originalState);

            VoxelGrid* newObject = state.makeMutable(existingObject);
            newObject->mutableDomain()->setCellMatrix(tm * existingObject->domain()->cellMatrix());
        }
    });

    return std::move(state);
}

}   // End of namespace
