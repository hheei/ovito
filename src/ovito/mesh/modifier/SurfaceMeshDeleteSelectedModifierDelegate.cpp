// SPDX-FileCopyrightText: 2026 OVITO GmbH, Germany
// SPDX-License-Identifier: GPL-3.0-only OR MIT

#include <ovito/mesh/Mesh.h>
#include <ovito/mesh/surface/SurfaceMesh.h>
#include <ovito/mesh/surface/SurfaceMeshBuilder.h>
#include <ovito/core/dataset/pipeline/ModifierEvaluationRequest.h>
#include <ovito/core/dataset/pipeline/ModificationNode.h>
#include "SurfaceMeshDeleteSelectedModifierDelegate.h"

namespace Ovito {

IMPLEMENT_CREATABLE_OVITO_CLASS(SurfaceMeshRegionsDeleteSelectedModifierDelegate);
OVITO_CLASSINFO(SurfaceMeshRegionsDeleteSelectedModifierDelegate, "DisplayName", "Mesh regions");

/******************************************************************************
* Indicates which data objects in the given input data collection the modifier
* delegate is able to operate on.
******************************************************************************/
QVector<DataObjectReference> SurfaceMeshRegionsDeleteSelectedModifierDelegate::OOMetaClass::getApplicableObjects(const DataCollection& input) const
{
    // Gather list of all surface mesh regions in the input data collection.
    QVector<DataObjectReference> objects;
    for(const ConstDataObjectPath& path : input.getObjectsRecursive(SurfaceMeshRegions::OOClass())) {
        objects.push_back(path);
    }
    return objects;
}

/******************************************************************************
 * Applies this modifier delegate to the data.
 ******************************************************************************/
Future<PipelineFlowState> SurfaceMeshRegionsDeleteSelectedModifierDelegate::apply(const ModifierEvaluationRequest& request, PipelineFlowState&& state, const PipelineFlowState& originalState, const std::vector<PipelineFlowState>& additionalInputs)
{
    // The actual work can be performed in a separate thread.
    return asyncLaunch([
            state = std::move(state),
            createdByNode = request.modificationNodeWeak(),
            inputObjectRef = inputDataObject()]() mutable
        {

        size_t numRegions = 0;
        size_t numDeleted = 0;
        bool hasSelection = false;

        visitObjectsToBeProcessed<SurfaceMesh>(state, inputObjectRef, createdByNode, [&](const SurfaceMesh* existingSurface) {
            // Make sure the input mesh data structure is valid.
            existingSurface->verifyMeshIntegrity();

            // Count total number of input regions.
            numRegions += existingSurface->regions()->elementCount();

            // Check if there is a region selection set.
            BufferReadAccessAndRef<SelectionIntType> regionMask = existingSurface->regions()->getProperty(SurfaceMeshRegions::SelectionProperty);
            if(!regionMask)
                return; // Nothing to do if there is no selection.
            hasSelection = true;

            // Mesh faces must have the "Region" property.
            if(!existingSurface->faces()->getProperty(SurfaceMeshFaces::RegionProperty))
                return; // Nothing to do if there is no face region information.

            // Check if at least one mesh region is currently selected.
            size_t selectionCount = regionMask.buffer()->nonzeroCount();
            if(selectionCount == 0)
                return;

            // Count total number of regions being deleted.
            numDeleted += selectionCount;

            // Create a mutable copy of the SurfaceMesh.
            SurfaceMesh* mutableSurface = state.makeMutable(existingSurface);

            // Create a working data structure for modifying the mesh.
            SurfaceMeshBuilder mesh(mutableSurface);

            // Remove selection property from the regions.
            mesh.removeRegionProperty(SurfaceMeshRegions::SelectionProperty);

            // Get access to the per-face region information.
            BufferReadAccess<int32_t> regionProperty = mesh.expectFaceProperty(SurfaceMeshFaces::RegionProperty);

            // Delete all faces that belong to one of the selected mesh regions.
            BufferFactory<SelectionIntType> faceMask(mesh.faceCount());
            for(SurfaceMesh::face_index face : mesh.facesRange()) {
                SurfaceMesh::region_index region = regionProperty[face];
                faceMask[face] = (region >= 0 && region < regionMask.size() && regionMask[region]);
            }
            regionProperty.reset();

            // Delete the selected faces and regions.
            mesh.deleteFaces(faceMask.take());
            mesh.deleteRegions(regionMask.take());

#ifdef OVITO_DEBUG
            mutableSurface->verifyMeshIntegrity();
#endif
        });

        // Report some statistics:
        QString statusMessage;
        if(!hasSelection) {
            statusMessage = tr("No selection - ");
        }
        statusMessage += tr("%1 of %2 mesh regions deleted (%3%)")
            .arg(numDeleted)
            .arg(numRegions)
            .arg((FloatType)numDeleted * 100 / std::max(numRegions, (size_t)1), 0, 'f', 1);

        // Show the number of deleted mesh regions next to the modifier's title in the pipeline editor.
        if(numDeleted != 0)
            state.combineStatus(std::move(statusMessage),
                numDeleted == 1 ? tr("1 mesh region") : tr("%1 mesh regions").arg(formatNumberForUI(numDeleted)));
        else
            state.combineStatus(std::move(statusMessage));

        return std::move(state);
    });
}

}   // End of namespace
