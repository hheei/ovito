// SPDX-FileCopyrightText: 2026 OVITO GmbH, Germany
// SPDX-License-Identifier: GPL-3.0-only OR MIT

#include <ovito/mesh/Mesh.h>
#include <ovito/stdobj/properties/PropertyContainer.h>
#include "SurfaceMeshAssignColorModifierDelegate.h"

namespace Ovito {

IMPLEMENT_CREATABLE_OVITO_CLASS(SurfaceMeshVerticesAssignColorModifierDelegate);
OVITO_CLASSINFO(SurfaceMeshVerticesAssignColorModifierDelegate, "DisplayName", "Mesh Vertices");
IMPLEMENT_CREATABLE_OVITO_CLASS(SurfaceMeshFacesAssignColorModifierDelegate);
OVITO_CLASSINFO(SurfaceMeshFacesAssignColorModifierDelegate, "DisplayName", "Mesh Faces");
IMPLEMENT_CREATABLE_OVITO_CLASS(SurfaceMeshRegionsAssignColorModifierDelegate);
OVITO_CLASSINFO(SurfaceMeshRegionsAssignColorModifierDelegate, "DisplayName", "Mesh Regions");

/******************************************************************************
* Indicates which data objects in the given input data collection the modifier
* delegate is able to operate on.
******************************************************************************/
QVector<DataObjectReference> SurfaceMeshVerticesAssignColorModifierDelegate::OOMetaClass::getApplicableObjects(const DataCollection& input) const
{
    // Gather list of all surface mesh vertices in the input data collection.
    QVector<DataObjectReference> objects;
    for(const ConstDataObjectPath& path : input.getObjectsRecursive(SurfaceMeshVertices::OOClass())) {
        objects.push_back(path);
    }
    return objects;
}

/******************************************************************************
* Indicates which data objects in the given input data collection the modifier
* delegate is able to operate on.
******************************************************************************/
QVector<DataObjectReference> SurfaceMeshFacesAssignColorModifierDelegate::OOMetaClass::getApplicableObjects(const DataCollection& input) const
{
    // Gather list of all surface mesh faces in the input data collection.
    QVector<DataObjectReference> objects;
    for(const ConstDataObjectPath& path : input.getObjectsRecursive(SurfaceMeshFaces::OOClass())) {
        objects.push_back(path);
    }
    return objects;
}

/******************************************************************************
* Indicates which data objects in the given input data collection the modifier
* delegate is able to operate on.
******************************************************************************/
QVector<DataObjectReference> SurfaceMeshRegionsAssignColorModifierDelegate::OOMetaClass::getApplicableObjects(const DataCollection& input) const
{
    // Gather list of all surface mesh regions in the input data collection.
    QVector<DataObjectReference> objects;
    for(const ConstDataObjectPath& path : input.getObjectsRecursive(SurfaceMeshRegions::OOClass())) {
        objects.push_back(path);
    }
    return objects;
}

}   // End of namespace
