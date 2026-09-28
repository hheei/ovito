// SPDX-FileCopyrightText: 2026 OVITO GmbH, Germany
// SPDX-License-Identifier: GPL-3.0-only OR MIT

#include <ovito/mesh/Mesh.h>
#include <ovito/stdobj/simcell/SimulationCell.h>
#include "SurfaceMesh.h"
#include "SurfaceMeshVis.h"
#include "SurfaceMeshReadAccess.h"

namespace Ovito {

IMPLEMENT_CREATABLE_OVITO_CLASS(SurfaceMesh);
OVITO_CLASSINFO(SurfaceMesh, "DisplayName", "Surface mesh");
DEFINE_PROPERTY_FIELD(SurfaceMesh, spaceFillingRegion);
DEFINE_REFERENCE_FIELD(SurfaceMesh, topology);
DEFINE_REFERENCE_FIELD(SurfaceMesh, vertices);
DEFINE_REFERENCE_FIELD(SurfaceMesh, faces);
DEFINE_REFERENCE_FIELD(SurfaceMesh, regions);
SET_PROPERTY_FIELD_LABEL(SurfaceMesh, vertices, "Vertices");
SET_PROPERTY_FIELD_LABEL(SurfaceMesh, faces, "Faces");
SET_PROPERTY_FIELD_LABEL(SurfaceMesh, regions, "Regions");

constexpr SurfaceMesh::size_type SurfaceMesh::InvalidIndex;

/******************************************************************************
* Constructor.
******************************************************************************/
void SurfaceMesh::initializeObject(ObjectInitializationFlags flags, const QString& title)
{
    PeriodicDomainObject::initializeObject(flags, title);

    if(!flags.testFlag(ObjectInitializationFlag::DontInitializeObject)) {
        if(!flags.testFlag(ObjectInitializationFlag::DontCreateVisElement)) {
            // Attach a visualization element for rendering the surface mesh.
            setVisElement(OORef<SurfaceMeshVis>::create(flags));
        }

        // Create the sub-object for storing the mesh topology.
        setTopology(DataOORef<SurfaceMeshTopology>::create(flags));

        // Create the sub-object for storing the vertex properties.
        setVertices(DataOORef<SurfaceMeshVertices>::create(flags));

        // Create the sub-object for storing the face properties.
        setFaces(DataOORef<SurfaceMeshFaces>::create(flags));

        // Create the sub-object for storing the region properties.
        setRegions(DataOORef<SurfaceMeshRegions>::create(flags));
    }
}

/******************************************************************************
* Checks if the surface mesh is valid and all vertex and face properties
* are consistent with the topology of the mesh. If this is not the case,
* the method throws an exception.
******************************************************************************/
void SurfaceMesh::verifyMeshIntegrity() const
{
    OVITO_CHECK_OBJECT_POINTER(topology());
    if(!topology())
        throw Exception(tr("Surface mesh has no topology object attached."));

    OVITO_CHECK_OBJECT_POINTER(vertices());
    if(!vertices())
        throw Exception(tr("Surface mesh has no vertex properties container attached."));
    if(!vertices()->getProperty(SurfaceMeshVertices::PositionProperty))
        throw Exception(tr("Surface mesh is missing the position vertex property."));
    OVITO_ASSERT(topology()->vertexCount() == vertices()->elementCount());
    if(topology()->vertexCount() != vertices()->elementCount())
        throw Exception(tr("Length of vertex property arrays of surface mesh does not match number of vertices in the mesh topology."));

    OVITO_CHECK_OBJECT_POINTER(faces());
    if(!faces())
        throw Exception(tr("Surface mesh has no face properties container attached."));
    OVITO_ASSERT(faces()->properties().empty() || topology()->faceCount() == faces()->elementCount());
    if(!faces()->properties().empty() && topology()->faceCount() != faces()->elementCount())
        throw Exception(tr("Length of face property arrays of surface mesh does not match number of faces in the mesh topology."));

    OVITO_CHECK_OBJECT_POINTER(regions());
    if(!regions())
        throw Exception(tr("Surface mesh has no region properties container attached."));

    OVITO_ASSERT(spaceFillingRegion() == InvalidIndex || spaceFillingRegion() >= 0);
    if(spaceFillingRegion() != InvalidIndex && spaceFillingRegion() < 0)
        throw Exception(tr("Space filling region ID set for surface mesh must not be negative."));

    vertices()->verifyIntegrity();
    faces()->verifyIntegrity();
    regions()->verifyIntegrity();
}

/******************************************************************************
* Determines which spatial region contains the given point in space.
* Returns -1 if the point is exactly on a region boundary.
******************************************************************************/
std::optional<std::pair<SurfaceMesh::region_index, FloatType>> SurfaceMesh::locatePoint(const Point3& location, FloatType epsilon) const
{
    verifyMeshIntegrity();
    return SurfaceMeshReadAccess(this).locatePoint(location, epsilon);
}

}   // End of namespace
