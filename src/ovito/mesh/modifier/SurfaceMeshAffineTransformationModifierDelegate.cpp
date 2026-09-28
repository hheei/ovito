// SPDX-FileCopyrightText: 2026 OVITO GmbH, Germany
// SPDX-License-Identifier: GPL-3.0-only OR MIT

#include <ovito/mesh/Mesh.h>
#include <ovito/stdobj/simcell/SimulationCell.h>
#include <ovito/core/dataset/DataSet.h>
#include <ovito/core/dataset/pipeline/ModificationNode.h>
#include "SurfaceMeshAffineTransformationModifierDelegate.h"

namespace Ovito {

IMPLEMENT_CREATABLE_OVITO_CLASS(SurfaceMeshAffineTransformationModifierDelegate);
OVITO_CLASSINFO(SurfaceMeshAffineTransformationModifierDelegate, "DisplayName", "Surfaces");

/******************************************************************************
* Indicates which data objects in the given input data collection the modifier
* delegate is able to operate on.
******************************************************************************/
QVector<DataObjectReference> SurfaceMeshAffineTransformationModifierDelegate::OOMetaClass::getApplicableObjects(const DataCollection& input) const
{
    // Gather list of all surface meshes in the input data collection.
    QVector<DataObjectReference> objects;
    for(const ConstDataObjectPath& path : input.getObjectsRecursive(SurfaceMesh::OOClass())) {
        objects.push_back(path);
    }
    return objects;
}

/******************************************************************************
 * Applies this modifier delegate to the data.
 ******************************************************************************/
Future<PipelineFlowState> SurfaceMeshAffineTransformationModifierDelegate::apply(const ModifierEvaluationRequest& request, PipelineFlowState&& state, const PipelineFlowState& originalState, const std::vector<PipelineFlowState>& additionalInputs)
{
    AffineTransformationModifier* modifier = static_object_cast<AffineTransformationModifier>(request.modifier());

    // The actual work can be performed in a separate thread.
    return asyncLaunch([
            state = std::move(state),
            tm = modifier->effectiveAffineTransformation(originalState),
            selectionOnly = modifier->selectionOnly(),
            createdByNode = request.modificationNodeWeak(),
            inputObjectRef = inputDataObject()]() mutable {

        // Process SurfaceMesh objects.
        visitObjectsToBeProcessed<SurfaceMesh>(state, inputObjectRef, createdByNode, [&](const SurfaceMesh* existingSurface) {
            // Make sure the input mesh data structure is valid.
            existingSurface->verifyMeshIntegrity();

            // Create a copy of the SurfaceMesh.
            SurfaceMesh* newSurface = state.makeMutable(existingSurface);
            // Create a copy of the vertices sub-object (no need to copy the topology when only moving vertices).
            SurfaceMeshVertices* newVertices = newSurface->makeVerticesMutable();

            // Get the input vertex coordinates (as strong reference to force creation of a mutable copy below).
            ConstPropertyPtr inputPositionProperty = newVertices->expectProperty(SurfaceMeshVertices::PositionProperty);

            // Create an uninitialized copy of the vertex position property.
            Property* outputPositionProperty = newVertices->makePropertyMutable(inputPositionProperty, DataBuffer::Uninitialized);

            // Let the modifier do the actual coordinate transformation work.
            AffineTransformationModifier::transformCoordinates(tm, selectionOnly, inputPositionProperty, outputPositionProperty, newVertices->getProperty(SurfaceMeshVertices::SelectionProperty));

            // Apply transformation to the cutting planes attached to the surface mesh.
            if(!newSurface->cuttingPlanes().empty()) {
                QVector<Plane3> cuttingPlanes = newSurface->cuttingPlanes();
                for(Plane3& plane : cuttingPlanes)
                    plane = tm * plane;
                newSurface->setCuttingPlanes(std::move(cuttingPlanes));
            }

            this_task::throwIfCanceled();
        });

        return std::move(state);
    });
}

}   // End of namespace
