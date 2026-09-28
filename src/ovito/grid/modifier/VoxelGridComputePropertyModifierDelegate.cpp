// SPDX-FileCopyrightText: 2026 OVITO GmbH, Germany
// SPDX-License-Identifier: GPL-3.0-only OR MIT

#include <ovito/grid/Grid.h>
#include "VoxelGridComputePropertyModifierDelegate.h"

namespace Ovito {

IMPLEMENT_CREATABLE_OVITO_CLASS(VoxelGridComputePropertyModifierDelegate);
OVITO_CLASSINFO(VoxelGridComputePropertyModifierDelegate, "DisplayName", "Voxel grids");

/******************************************************************************
* Indicates which data objects in the given input data collection the modifier
* delegate is able to operate on.
******************************************************************************/
QVector<DataObjectReference> VoxelGridComputePropertyModifierDelegate::OOMetaClass::getApplicableObjects(const DataCollection& input) const
{
    // Gather list of all VoxelGrid objects in the input data collection.
    QVector<DataObjectReference> objects;
    for(const ConstDataObjectPath& path : input.getObjectsRecursive(VoxelGrid::OOClass())) {
        objects.push_back(path);
    }
    return objects;
}

/******************************************************************************
* Initializes an expression evaluator.
******************************************************************************/
std::unique_ptr<PropertyExpressionEvaluator> VoxelGridComputePropertyModifierDelegate::initializeExpressionEvaluator(const ComputePropertyModifier* modifier, const PipelineFlowState& originalState, int frame) const
{
    const ConstDataObjectPath containerPath = originalState.expectObject(inputContainerRef());
    const VoxelGrid* voxelGrid = containerPath.lastAs<VoxelGrid>();

    auto evaluator = std::make_unique<PropertyExpressionEvaluator>();
    evaluator->initializeInputs(originalState, containerPath, frame);

    evaluator->registerComputedVariable(
        "SpatialPosition.X",
        [helper = VoxelGrid::GridPositionHelper(voxelGrid)](size_t voxelIndex) -> double { return helper(voxelIndex).x(); },
        tr("Cartesian voxel coord"));

    evaluator->registerComputedVariable(
        "SpatialPosition.Y",
        [helper = VoxelGrid::GridPositionHelper(voxelGrid)](size_t voxelIndex) -> double { return helper(voxelIndex).y(); },
        tr("Cartesian voxel coord"));

    evaluator->registerComputedVariable(
        "SpatialPosition.Z",
        [helper = VoxelGrid::GridPositionHelper(voxelGrid)](size_t voxelIndex) -> double { return helper(voxelIndex).z(); },
        tr("Cartesian voxel coord"));

    evaluator->registerComputedVariable("VoxelCoordinate.X", [shape=voxelGrid->shape()](size_t voxelIndex) -> double {
        return voxelIndex % shape[0];
    },
    tr("Logical coordinate"));

    evaluator->registerComputedVariable("VoxelCoordinate.Y", [shape=voxelGrid->shape()](size_t voxelIndex) -> double {
        return (voxelIndex / shape[0]) % shape[1];
    },
    tr("Logical coordinate"));

    evaluator->registerComputedVariable("VoxelCoordinate.Z", [shape=voxelGrid->shape()](size_t voxelIndex) -> double {
        return voxelIndex / (shape[0] * shape[1]);
    },
    tr("Logical coordinate"));

    evaluator->initializeExpressions(modifier->expressions());

    return evaluator;
}

}   // End of namespace
