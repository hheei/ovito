// SPDX-FileCopyrightText: 2026 OVITO GmbH, Germany
// SPDX-License-Identifier: GPL-3.0-only OR MIT

#include <ovito/mesh/Mesh.h>
#include "SurfaceMeshReadAccess.h"
#include "SurfaceMeshFaces.h"
#include "SurfaceMeshVis.h"

namespace Ovito {

IMPLEMENT_CREATABLE_OVITO_CLASS(SurfaceMeshFaces);
OVITO_CLASSINFO(SurfaceMeshFaces, "DisplayName", "Mesh Faces");

/******************************************************************************
* Creates a storage object for standard face properties.
******************************************************************************/
PropertyPtr SurfaceMeshFaces::OOMetaClass::createStandardPropertyInternal(DataBuffer::BufferInitialization init, size_t elementCount, int type, const ConstDataObjectPath& containerPath) const
{
    int dataType;
    size_t componentCount;

    switch(type) {
    case SelectionProperty:
        dataType = Property::IntSelection;
        componentCount = 1;
        break;
    case RegionProperty:
    case FaceTypeProperty:
        dataType = Property::Int32;
        componentCount = 1;
        break;
    case ColorProperty:
        dataType = Property::FloatGraphics;
        componentCount = 3;
        break;
    case BurgersVectorProperty:
    case CrystallographicNormalProperty:
        dataType = Property::FloatDefault;
        componentCount = 3;
        OVITO_ASSERT(componentCount * sizeof(FloatType) == sizeof(Vector3));
        break;
    default:
        OVITO_ASSERT_MSG(false, "SurfaceMeshFaces::createStandardPropertyInternal", "Invalid standard property type");
        throw Exception(tr("This is not a valid standard face property type: %1").arg(type));
    }
    const QStringList& componentNames = standardPropertyComponentNames(type);
    const QString& propertyName = standardPropertyName(type);

    OVITO_ASSERT(componentCount == standardPropertyComponentCount(type));

    PropertyPtr property = PropertyPtr::create(DataBuffer::Uninitialized, elementCount, dataType, componentCount, propertyName, type, componentNames);

    // Initialize memory if requested.
    if(init == DataBuffer::Initialized && containerPath.size() >= 2) {
        // Certain standard properties need to be initialized with default values determined by the attached visual elements.
        if(type == ColorProperty) {
            if(const SurfaceMesh* surfaceMesh = dynamic_object_cast<SurfaceMesh>(containerPath[containerPath.size()-2])) {
                BufferReadAccess<ColorG> regionColorProperty = surfaceMesh->regions()->getProperty(SurfaceMeshRegions::ColorProperty);
                BufferReadAccess<int32_t> faceRegionProperty = surfaceMesh->faces()->getProperty(SurfaceMeshFaces::RegionProperty);
                if(regionColorProperty && faceRegionProperty && faceRegionProperty.size() == elementCount) {
                    // Inherit face colors from regions.
                    std::ranges::transform(faceRegionProperty, BufferWriteAccess<ColorG, access_mode::discard_write>(property).begin(),
                        [&](int region) { return (region >= 0 && region < regionColorProperty.size()) ? regionColorProperty[region] : ColorG(1,1,1); });
                    init = DataBuffer::Uninitialized;
                }
                else if(SurfaceMeshVis* vis = surfaceMesh->visElement<SurfaceMeshVis>()) {
                    // Initialize face colors from uniform color set in SurfaceMeshVis.
                    property->fill<ColorG>(vis->surfaceColor().toDataType<GraphicsFloatType>());
                    init = DataBuffer::Uninitialized;
                }
            }
        }
    }

    if(init == DataBuffer::Initialized) {
        // Default-initialize property values with zeros.
        property->fillZero();
    }

    return property;
}

/******************************************************************************
* Registers all standard properties with the property traits class.
******************************************************************************/
void SurfaceMeshFaces::OOMetaClass::initialize()
{
    PropertyContainerClass::initialize();

    setPropertyClassDisplayName(tr("Mesh Faces"));
    setElementDescriptionName(QStringLiteral("faces"));
    setPythonName(QStringLiteral("faces"));

    const QStringList emptyList;
    const QStringList xyzList = QStringList() << "X" << "Y" << "Z";
    const QStringList rgbList = QStringList() << "R" << "G" << "B";

    registerStandardProperty(SelectionProperty, tr("Selection"), Property::IntSelection, emptyList);
    registerStandardProperty(ColorProperty, tr("Color"), Property::FloatGraphics, rgbList, nullptr, tr("Face colors"));
    registerStandardProperty(FaceTypeProperty, tr("Type"), Property::Int32, emptyList);
    registerStandardProperty(RegionProperty, tr("Region"), Property::Int32, emptyList);
    registerStandardProperty(BurgersVectorProperty, tr("Burgers Vector"), Property::FloatDefault, xyzList, nullptr, tr("Burgers vectors"));
    registerStandardProperty(CrystallographicNormalProperty, tr("Crystallographic Normal"), Property::FloatDefault, xyzList);
}

/******************************************************************************
* Generates a human-readable string representation of the data object reference.
******************************************************************************/
QString SurfaceMeshFaces::OOMetaClass::formatDataObjectPath(const ConstDataObjectPath& path) const
{
    QString str;
    for(const DataObject* obj : path) {
        if(!str.isEmpty())
            str += QStringLiteral(u" \u2192 ");  // Unicode arrow
        str += obj->objectTitle();
    }
    return str;
}

/******************************************************************************
 * Returns the data for visualizing a vector property from this container using a VectorVis element.
 ******************************************************************************/
VectorVis::VectorData SurfaceMeshFaces::getVectorVisData(const ConstDataObjectPath& path, const PipelineFlowState& state,
                                                         const RendererResourceCache::ResourceFrame& visCache) const
{
    OVITO_ASSERT(path.nextToLastAs<SurfaceMeshFaces>() == this);
    if(const SurfaceMesh* mesh = path.nextToNextToLastAs<SurfaceMesh>()) { // Data object path is: SurfaceMesh -> SurfaceMeshFaces -> Property
        mesh->verifyMeshIntegrity();
        ConstDataBufferPtr basePositions = faceCentroids(mesh, visCache);

        // Look up the filtered vector values in the cache.
        const auto& vectorProperty = visCache.lookup<ConstDataBufferPtr>(
            RendererResourceKey<struct SurfaceMeshFacesFilteredVectorsCache, ConstDataObjectRef, ConstDataObjectRef>{
                mesh, path.lastAs<DataBuffer>()},
            [&](ConstDataBufferPtr& vectorProperty) {
                vectorProperty = path.lastAs<DataBuffer>();
                if(!vectorProperty || vectorProperty->componentCount() != 3) return;
                OVITO_ASSERT(vectorProperty->dataType() == Property::FloatDefault);
                if(vectorProperty->dataType() != Property::FloatDefault) return;
                // Does the mesh have cutting planes and do we need to perform point culling?
                if(mesh->cuttingPlanes().empty()) return;

                // Create a copy of the vector property in which the values of culled and degenerate faces
                // will be nulled out to hide the arrow glyphs for these faces.
                BufferWriteAccessAndRef<Vector3, access_mode::write> filteredVectors = vectorProperty.makeCopy();
                const SurfaceMeshReadAccess meshAccess(mesh);
                BufferReadAccess<Point3> centroids(basePositions);
                for(SurfaceMesh::face_index face : mesh->topology()->facesRange()) {
                    if(meshAccess.firstFaceEdge(face) == SurfaceMesh::InvalidIndex || mesh->isPointCulled(centroids[face]))
                        filteredVectors[face].setZero();
                }
                vectorProperty = filteredVectors.take();
            });

        return {.positions = std::move(basePositions),
                .directions = vectorProperty,
                .colors = nullptr,
                .transparencies = nullptr,
                .selection = nullptr};
    }
    return {};
}

/******************************************************************************
 * Returns the data for visualizing a property from this container as text labels
 * using a TextLabelsVis element.
 ******************************************************************************/
TextLabelsVis::LabelData SurfaceMeshFaces::getLabelVisData(const ConstDataObjectPath& path,
                                                           const PipelineFlowState& state,
                                                           const RendererResourceCache::ResourceFrame& visCache,
                                                           TextLabelsVis::LabelDataRequest request,
                                                           TextLabelsVis::ElementAnchor anchor) const
{
    OVITO_ASSERT(path.nextToLastAs<SurfaceMeshFaces>() == this);
    if(const SurfaceMesh* mesh =
           path.nextToNextToLastAs<SurfaceMesh>()) {  // Data object path is: SurfaceMesh -> SurfaceMeshFaces -> Property
        mesh->verifyMeshIntegrity();
        ConstDataBufferPtr basePositions = faceCentroids(mesh, visCache);

        // Suppress the labels of those faces which are cut away by the cutting planes of the mesh, as well as
        // the labels of degenerate faces, which the mesh vis element does not render either. The cutting
        // planes are applied at render time only, which is why the filtering has to happen here.
        // Filtering the labels is expensive. That's why we store the result in the vis cache.
        const auto& texts = visCache.lookup<ConstDataBufferPtr>(
            RendererResourceKey<struct SurfaceMeshFacesFilteredLabelsCache, ConstDataObjectRef, ConstDataObjectRef>{
                mesh, path.lastAs<DataBuffer>()},
            [&](ConstDataBufferPtr& texts) {
                const SurfaceMeshReadAccess meshAccess(mesh);
                texts = TextLabelsVis::cullLabels(path.lastAs<DataBuffer>(), basePositions, true,
                    [&](size_t face, const Point3& p) {
                        return meshAccess.firstFaceEdge(face) == SurfaceMesh::InvalidIndex || mesh->isPointCulled(p);
                    });
            });

        return {.positions = std::move(basePositions), .texts = texts};
    }
    return {};
}

/******************************************************************************
 * Computes the centroids of the mesh faces, which serve as anchor points for visual elements.
 ******************************************************************************/
ConstDataBufferPtr SurfaceMeshFaces::faceCentroids(const SurfaceMesh* mesh, const RendererResourceCache::ResourceFrame& visCache) const
{
    // Computing the centroids is expensive. That's why we store the result in the vis cache.
    return visCache.lookup<ConstDataBufferPtr>(
        RendererResourceKey<struct SurfaceMeshFacesCentroidsCache, ConstDataObjectRef>{mesh}, [&](ConstDataBufferPtr& basePositions) {
            const SurfaceMeshReadAccess meshAccess(mesh);
            BufferReadAccess<Point3> vertexPositions(meshAccess.expectVertexProperty(SurfaceMeshVertices::PositionProperty));
            BufferFactory<Point3> centroids(mesh->faces()->elementCount());
            for(SurfaceMesh::face_index face : mesh->topology()->facesRange()) {
                Vector3 c = Vector3::Zero();
                Vector3 com = Vector3::Zero();
                int n = 0;
                SurfaceMesh::edge_index firstFaceEdge = meshAccess.firstFaceEdge(face);
                if(firstFaceEdge != SurfaceMesh::InvalidIndex) {
                    SurfaceMesh::edge_index edge = firstFaceEdge;
                    do {
                        c += meshAccess.edgeVector(edge, vertexPositions);
                        com += c;
                        n++;
                        edge = meshAccess.nextFaceEdge(edge);
                    } while(edge != firstFaceEdge);
                    centroids[face] = meshAccess.wrapPoint(vertexPositions[meshAccess.vertex1(firstFaceEdge)] + (com / n));
                }
                else {
                    centroids[face] = Point3::Origin();
                }
            }
            basePositions = centroids.take();
        });
}

}   // End of namespace
