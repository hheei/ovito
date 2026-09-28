// SPDX-FileCopyrightText: 2026 OVITO GmbH, Germany
// SPDX-License-Identifier: GPL-3.0-only OR MIT

#include <ovito/mesh/Mesh.h>
#include <ovito/stdobj/properties/PropertyContainer.h>
#include "SurfaceMeshColorCodingModifierDelegate.h"

namespace Ovito {

IMPLEMENT_CREATABLE_OVITO_CLASS(SurfaceMeshVerticesColorCodingModifierDelegate);
OVITO_CLASSINFO(SurfaceMeshVerticesColorCodingModifierDelegate, "DisplayName", "Mesh Vertices");
IMPLEMENT_CREATABLE_OVITO_CLASS(SurfaceMeshFacesColorCodingModifierDelegate);
OVITO_CLASSINFO(SurfaceMeshFacesColorCodingModifierDelegate, "DisplayName", "Mesh Faces");
IMPLEMENT_CREATABLE_OVITO_CLASS(SurfaceMeshRegionsColorCodingModifierDelegate);
OVITO_CLASSINFO(SurfaceMeshRegionsColorCodingModifierDelegate, "DisplayName", "Mesh Regions");

/******************************************************************************
* Indicates which data objects in the given input data collection the modifier
* delegate is able to operate on.
******************************************************************************/
QVector<DataObjectReference> SurfaceMeshVerticesColorCodingModifierDelegate::OOMetaClass::getApplicableObjects(const DataCollection& input) const
{
    // Gather list of all surface mesh vertices in the input data collection.
    QVector<DataObjectReference> objects;
    for(const ConstDataObjectPath& path : input.getObjectsRecursive(SurfaceMeshVertices::OOClass())) {
        if(static_object_cast<SurfaceMeshVertices>(path.back())->properties().empty() == false)
            objects.push_back(path);
    }
    return objects;
}

/******************************************************************************
* Indicates which data objects in the given input data collection the modifier
* delegate is able to operate on.
******************************************************************************/
QVector<DataObjectReference> SurfaceMeshFacesColorCodingModifierDelegate::OOMetaClass::getApplicableObjects(const DataCollection& input) const
{
    // Gather list of all surface mesh faces in the input data collection.
    QVector<DataObjectReference> objects;
    for(const ConstDataObjectPath& path : input.getObjectsRecursive(SurfaceMeshFaces::OOClass())) {
        if(static_object_cast<SurfaceMeshFaces>(path.back())->properties().empty() == false)
            objects.push_back(path);
    }
    return objects;
}

/******************************************************************************
* Indicates which data objects in the given input data collection the modifier
* delegate is able to operate on.
******************************************************************************/
QVector<DataObjectReference> SurfaceMeshRegionsColorCodingModifierDelegate::OOMetaClass::getApplicableObjects(const DataCollection& input) const
{
    // Gather list of all surface mesh regions in the input data collection.
    QVector<DataObjectReference> objects;
    for(const ConstDataObjectPath& path : input.getObjectsRecursive(SurfaceMeshRegions::OOClass())) {
        if(static_object_cast<SurfaceMeshRegions>(path.back())->properties().empty() == false)
            objects.push_back(path);
    }
    return objects;
}

}   // End of namespace
