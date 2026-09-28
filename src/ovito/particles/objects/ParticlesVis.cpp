////////////////////////////////////////////////////////////////////////////////////////
//
//  Copyright 2026 OVITO GmbH, Germany
//
//  This file is part of OVITO (Open Visualization Tool).
//
//  OVITO is free software; you can redistribute it and/or modify it either under the
//  terms of the GNU General Public License version 3 as published by the Free Software
//  Foundation (the "GPL") or, at your option, under the terms of the MIT License.
//  If you do not alter this notice, a recipient may use your version of this
//  file under either the GPL or the MIT License.
//
//  You should have received a copy of the GPL along with this program in a
//  file LICENSE.GPL.txt.  You should have received a copy of the MIT License along
//  with this program in a file LICENSE.MIT.txt
//
//  This software is distributed on an "AS IS" basis, WITHOUT WARRANTY OF ANY KIND,
//  either express or implied. See the GPL or the MIT License for the specific language
//  governing rights and limitations.
//
////////////////////////////////////////////////////////////////////////////////////////

#include <ovito/particles/Particles.h>
#include <ovito/particles/objects/ParticleType.h>
#include <ovito/particles/objects/Particles.h>
#include <ovito/particles/objects/BondsVis.h>
#include <ovito/core/utilities/units/UnitsManager.h>
#include <ovito/core/dataset/DataSet.h>
#include <ovito/core/rendering/FrameGraph.h>
#include <ovito/core/rendering/ParticlePrimitive.h>
#include <ovito/core/rendering/CylinderPrimitive.h>
#include "ParticlesVis.h"

namespace Ovito {

IMPLEMENT_CREATABLE_OVITO_CLASS(ParticlesVis);
OVITO_CLASSINFO(ParticlesVis, "DisplayName", "Particles");
IMPLEMENT_ABSTRACT_OVITO_CLASS(ParticlePickInfo);
DEFINE_PROPERTY_FIELD(ParticlesVis, defaultParticleRadius);
DEFINE_PROPERTY_FIELD(ParticlesVis, radiusScaleFactor);
DEFINE_PROPERTY_FIELD(ParticlesVis, renderingQuality);
DEFINE_PROPERTY_FIELD(ParticlesVis, particleShape);
SET_PROPERTY_FIELD_LABEL(ParticlesVis, defaultParticleRadius, "Radius");
SET_PROPERTY_FIELD_LABEL(ParticlesVis, radiusScaleFactor, "Radius scaling");
SET_PROPERTY_FIELD_LABEL(ParticlesVis, renderingQuality, "Rendering quality");
SET_PROPERTY_FIELD_LABEL(ParticlesVis, particleShape, "Shape");
SET_PROPERTY_FIELD_UNITS_AND_MINIMUM(ParticlesVis, defaultParticleRadius, WorldParameterUnit, 0);
SET_PROPERTY_FIELD_UNITS_AND_MINIMUM(ParticlesVis, radiusScaleFactor, PercentParameterUnit, 0);

/******************************************************************************
* Computes the bounding box of the visual element.
* This method takes into account the coordinates of the particles as well as their sizes and shapes.
* It is called as part of the pipeline bounding box computation in Pipeline::localBoundingBox() and Pipeline::globalBoundingBox().
******************************************************************************/
Box3 ParticlesVis::boundingBoxImmediate(AnimationTime time, const ConstDataObjectPath& path, const Pipeline* pipeline, const PipelineFlowState& flowState, TimeInterval& validityInterval)
{
    const Particles* particles = path.lastAs<Particles>();
    if(!particles)
        return {};

    particles->verifyIntegrity();
    const Property* positionProperty = particles->getProperty(Particles::PositionProperty);
    if(!positionProperty)
        return {};

    // Compute bounding box of particle coordinates.
    Box3 bbox = positionProperty->boundingBox3();

    // Also take into account the sizes and shapes of the particles.
    const Property* radiusProperty = particles->getProperty(Particles::RadiusProperty);
    const Property* typeProperty = particles->getProperty(Particles::TypeProperty);
    const Property* shapeProperty = particles->getProperty(Particles::AsphericalShapeProperty);
    if(particleShape() != Sphere && particleShape() != Box && particleShape() != Cylinder && particleShape() != Spherocylinder)
        shapeProperty = nullptr;

    // Check if any of the particle types have a user-defined mesh shape.
    // For each such type, we store its numeric type identifier and the maximum extent of the assigned shape mesh to be used as bounding box.
    boost::container::flat_map<int, GraphicsFloatType> userShapeParticleTypes;
    if(typeProperty) {
        for(const ElementType* etype : typeProperty->elementTypes()) {
            if(const ParticleType* ptype = dynamic_object_cast<ParticleType>(etype)) {
                if(ptype->shapeMesh() && ptype->shapeMesh()->faceCount() != 0) {
                    // Compute the maximum extent of the user-defined shape mesh.
                    const Box3& bbox = ptype->shapeMesh()->boundingBox();
                    GraphicsFloatType extent = std::max((bbox.minc - Point3::Origin()).length(), (bbox.maxc - Point3::Origin()).length());
                    userShapeParticleTypes.emplace(ptype->numericId(), static_cast<GraphicsFloatType>(extent));
                }
            }
        }
    }

    // Extend box to account for radii and shapes of particles.
    GraphicsFloatType maxAtomRadius = 0;
    GraphicsFloatType defaultRadius = static_cast<GraphicsFloatType>(defaultParticleRadius());

    if(userShapeParticleTypes.empty()) {
        // Standard case - no user-defined particle shapes:
        if(typeProperty) {
            // Find larges radius set on the particle types.
            for(const ElementType* type : typeProperty->elementTypes()) {
                if(const ParticleType* particleType = dynamic_object_cast<ParticleType>(type))
                    maxAtomRadius = std::max(maxAtomRadius, static_cast<GraphicsFloatType>(particleType->radius()));
            }
        }
        if(maxAtomRadius == 0)
            maxAtomRadius = defaultRadius;

        // Take into account the size of aspherical shapes if defined.
        if(shapeProperty) {
            bool shapeSetForAllParticles = true;
            GraphicsFloatType maxShapeSize = 0;
            for(const Vector3G& s : BufferReadAccess<Vector3G>(shapeProperty)) {
                if(s.x() <= 0 && s.y() <= 0 && s.z() <= 0)
                    shapeSetForAllParticles = false;
                else
                    maxShapeSize = std::max(maxShapeSize, std::max(s.x(), std::max(s.y(), s.z())));
            }
            if(particleShape() == Spherocylinder)
                maxShapeSize *= 2;
            if(shapeSetForAllParticles)
                maxAtomRadius = maxShapeSize;
            else
                maxAtomRadius = std::max(maxAtomRadius, maxShapeSize);
        }

        // Take into account the per-particle radii if defined.
        if(radiusProperty && radiusProperty->size() != 0) {
            BufferReadAccess<GraphicsFloatType> radiusAcc(radiusProperty);
            auto minmax = std::minmax_element(radiusAcc.cbegin(), radiusAcc.cend());
            if(*minmax.first <= 0)
                maxAtomRadius = std::max(maxAtomRadius, *minmax.second);
            else
                maxAtomRadius = *minmax.second;
        }
    }
    else {
        // Non-standard case - at least one user-defined particle shape:
        const auto typeRadiusMap = ParticleType::typeRadiusMap(typeProperty);
        if(radiusProperty && radiusProperty->size() == typeProperty->size()) {
            BufferReadAccess<GraphicsFloatType> radiusAcc(radiusProperty);
            BufferReadAccess<int32_t> typeAcc(typeProperty);
            for(const auto [t, r] : std::views::zip(typeAcc, radiusAcc)) {
                // Determine effective radius of the current particle.
                GraphicsFloatType radius = r;
                if(radius <= 0) { if(auto it = typeRadiusMap.find(t); it != typeRadiusMap.end()) radius = it->second; }
                if(radius <= 0) radius = defaultRadius;
                // Effective radius gets multiplied with the extent of the user-defined shape mesh.
                auto iter = userShapeParticleTypes.find(t);
                auto shapeExtent = (iter != userShapeParticleTypes.end()) ? iter->second : 1;
                maxAtomRadius = std::max(maxAtomRadius, radius * shapeExtent);
            }
        }
        else {
            // No per-particle radii defined - only per-type radii.
            for(const auto& it : typeRadiusMap) {
                auto typeRadius = (it.second != 0) ? it.second : defaultRadius;
                // Effective radius gets multiplied with the extent of the user-defined shape mesh.
                auto iter = userShapeParticleTypes.find(it.first);
                auto shapeExtent = (iter != userShapeParticleTypes.end()) ? iter->second : 1;
                maxAtomRadius = std::max(maxAtomRadius, typeRadius * shapeExtent);
            }
        }
    }

    // Extend the bounding box by the largest particle radius.
    return bbox.padBox(std::max(radiusScaleFactor() * maxAtomRadius * std::sqrt(FloatType(3)), FloatType(0)));
}

/******************************************************************************
* Returns the typed particle property used to determine the rendering colors
* of particles (if no per-particle colors are defined).
******************************************************************************/
const Property* ParticlesVis::getParticleTypeColorProperty(const Particles* particles) const
{
    return particles->getProperty(Particles::TypeProperty);
}

/******************************************************************************
* Computes the effective display colors of the particles used for rendering.
* This takes into account the following sources for particle colors, listed here in order of decreasing priority:
*   1) Per-particle color values defined in the "Color" property.
*   2) Per-type color values defined in the particle types assigned to the particles.
*   3) The uniform default particle color defined in the ParticlesVis visual element.
* Optionally, selected particles can be highlighted with a special color defined in the ParticlesVis visual element.
******************************************************************************/
ConstPropertyPtr ParticlesVis::effectiveParticleColors(const Particles* particles, bool highlightSelection) const
{
    OVITO_ASSERT(particles);
    particles->verifyIntegrity();

    // Take particle colors directly from the 'Color' property if available.
    ConstPropertyPtr output = particles->getProperty(Particles::ColorProperty);
    if(!output) {
        // Allocate new output color array.
        output.reset(Particles::OOClass().createStandardProperty(DataBuffer::Uninitialized, particles->elementCount(), Particles::ColorProperty));

        const ColorG defaultColor = defaultParticleColor().toDataType<GraphicsFloatType>();
        if(const Property* typeProperty = getParticleTypeColorProperty(particles)) {
            OVITO_ASSERT(typeProperty->size() == output->size());
            // Assign colors based on particle types.
            // Generate a lookup map for particle type colors.
            const auto colorMap = typeProperty->typeColorMap();
            std::array<ColorG, 16> colorArray;
            // Check if all type IDs are within a small, non-negative range.
            // If yes, we can use an array lookup strategy. Otherwise we have to use a dictionary lookup strategy, which is slower.
            if(std::ranges::all_of(colorMap, [&colorArray](const auto& i) { return i.first >= 0 && i.first < (int)colorArray.size(); })) {
                colorArray.fill(defaultColor);
                for(const auto& entry : colorMap)
                    colorArray[entry.first] = entry.second;
                // Fill color array.
                BufferReadAccess<int32_t> typeData(typeProperty);
                const auto* t = typeData.cbegin();
                for(auto& c : BufferWriteAccess<ColorG, access_mode::discard_write>(output.makeMutableInplace())) {
                    if(*t >= 0 && (size_t)*t < colorArray.size())
                        c = colorArray[*t];
                    else
                        c = defaultColor;
                    ++t;
                }
            }
            else {
                // Fill color array.
                BufferReadAccess<int32_t> typeData(typeProperty);
                const auto* t = typeData.cbegin();
                for(auto& c : BufferWriteAccess<ColorG, access_mode::discard_write>(output.makeMutableInplace())) {
                    if(auto it = colorMap.find(*t); it != colorMap.end())
                        c = it->second;
                    else
                        c = defaultColor;
                    ++t;
                }
            }
        }
        else {
            // Assign a uniform color to all particles.
            output.makeMutableInplace()->fill<ColorG>(defaultColor);
        }
    }

    // Highlight selected particles with a special color.
    if(const Property* selectionProperty = highlightSelection ? particles->getProperty(Particles::SelectionProperty) : nullptr)
        output.makeMutableInplace()->fillSelected<ColorG>(selectionParticleColor().toDataType<GraphicsFloatType>(), *selectionProperty);

    return output;
}


/******************************************************************************
* Computes the display color of a single particle.
* The method takes into account the same inputs as the effectiveParticleColors() method,
* but is optimized for the efficient computation of the color of a single particle.
* If a selection property is provided, the particle is highlighted with a special color
* defined in the ParticlesVis visual element when it is selected.
******************************************************************************/
ColorG ParticlesVis::effectiveParticleColor(size_t particleIndex, BufferReadAccess<ColorG> colorProperty, const Property* typeProperty, BufferReadAccess<SelectionIntType> selectionProperty) const
{
    // Check if particle is selected.
    if(selectionProperty && selectionProperty.size() > particleIndex) {
        if(selectionProperty[particleIndex])
            return selectionParticleColor().toDataType<GraphicsFloatType>();
    }

    if(colorProperty && colorProperty.size() > particleIndex) {
        // Take particle color directly from the color property.
        return colorProperty[particleIndex];
    }

    if(typeProperty && typeProperty->size() > particleIndex) {
        // Return color based on particle type.
        BufferReadAccess<int32_t> typeAcc(typeProperty);
        const ElementType* ptype = typeProperty->elementType(typeAcc[particleIndex]);
        if(ptype)
            return ptype->color().toDataType<GraphicsFloatType>();
    }

    return defaultParticleColor().toDataType<GraphicsFloatType>();
}

/******************************************************************************
* Returns the typed particle property used to determine the rendering radii
* of particles (if no per-particle radii are defined).
******************************************************************************/
const Property* ParticlesVis::getParticleTypeRadiusProperty(const Particles* particles) const
{
    return particles->getProperty(Particles::TypeProperty);
}

/******************************************************************************
* Computes the effective particle radii used for rendering particles.
* This takes into account the following sources for particle radii, listed here in order of decreasing priority:
*   1) Per-particle radius values defined in the "Radius" property.
*   2) Per-type radius values defined in the particle types assigned to the particles.
*   3) The uniform default particle radius defined in the "Radius" property of the ParticlesVis visual element.
* Whenever a radius is zero for a particle, the next source in the above list is used to determine the effective radius for that particle.
* Optionally, the global scaling factor defined in in the ParticlesVis visual element can be applied to the particle radii.
******************************************************************************/
ConstPropertyPtr ParticlesVis::effectiveParticleRadii(const Particles* particles, bool includeGlobalScaleFactor) const
{
    particles->verifyIntegrity();
    GraphicsFloatType defaultRadius = defaultParticleRadius();
    if(includeGlobalScaleFactor)
        defaultRadius *= radiusScaleFactor();

    // Take particle radii directly from the 'Radius' property if available.
    ConstPropertyPtr output = particles->getProperty(Particles::RadiusProperty);
    if(output) {
        // Check if the "Radius" property array contains any zero entries.
        if(std::ranges::contains(BufferReadAccess<GraphicsFloatType>(output), GraphicsFloatType(0))) {
            // Copy per-type radii to those particles whose "Radius" property value is zero.
            if(const Property* typeProperty = getParticleTypeRadiusProperty(particles)) {
                // Build a lookup map for particle type radii.
                const auto radiusMap = ParticleType::typeRadiusMap(typeProperty);
                // Skip the following loop if all per-type radii are zero.
                if(!radiusMap.empty()) {
                    // Update radius array.
                    BufferReadAccess<int32_t> typeAcc(typeProperty);
                    BufferWriteAccess<GraphicsFloatType, access_mode::read_write> radiusAcc(output.makeMutableInplace());
                    for(auto&& [radius, type] : std::views::zip(radiusAcc, typeAcc)) {
                        if(radius <= 0) {
                            if(auto it = radiusMap.find(type); it != radiusMap.end())
                                radius = it->second;
                        }
                    }
                }
            }

            // Replace remaining zero entries in the "Radius" array with the uniform default radius.
            std::ranges::replace(BufferWriteAccess<GraphicsFloatType, access_mode::read_write>(output.makeMutableInplace()), GraphicsFloatType(0), static_cast<GraphicsFloatType>(defaultParticleRadius()));
        }
        // Apply global scaling factor.
        if(includeGlobalScaleFactor && radiusScaleFactor() != 1.0) {
            for(auto& r : BufferWriteAccess<GraphicsFloatType, access_mode::read_write>(output.makeMutableInplace()))
                r *= radiusScaleFactor();
        }
    }
    else {
        // Allocate output array.
        output.reset(Particles::OOClass().createStandardProperty(DataBuffer::Uninitialized, particles->elementCount(), Particles::RadiusProperty));

        if(const Property* typeProperty = getParticleTypeRadiusProperty(particles)) {
            OVITO_ASSERT(typeProperty->size() == output->size());

            // Assign radii based on particle types.
            // Build a lookup map for particle type radii.
            auto radiusMap = ParticleType::typeRadiusMap(typeProperty);
            // Skip the following loop if all per-type radii are zero. In this case, simply use the default radius for all particles.
            if(!radiusMap.empty()) {
                // Apply global scaling factor.
                if(includeGlobalScaleFactor && radiusScaleFactor() != 1) {
                    for(auto& p : radiusMap)
                        p.second *= radiusScaleFactor();
                }
                // Fill radius array.
                BufferReadAccess<int32_t> typeAcc(typeProperty);
                BufferWriteAccess<GraphicsFloatType, access_mode::discard_write> radiusAcc(output.makeMutableInplace());
                std::ranges::transform(typeAcc, radiusAcc.begin(), [&](auto t) {
                    // Set particle radius only if the type's radius is non-zero.
                    if(auto it = radiusMap.find(t); it != radiusMap.end())
                        return static_cast<GraphicsFloatType>(it->second);
                    else
                        return defaultRadius;
                });
            }
            else {
                // Assign the uniform default radius to all particles.
                output.makeMutableInplace()->fill<GraphicsFloatType>(defaultRadius);
            }
        }
        else {
            // Assign the uniform default radius to all particles.
            output.makeMutableInplace()->fill<GraphicsFloatType>(defaultRadius);
        }
    }

    return output;
}

/******************************************************************************
* Computes the effective display radius of a single particle.
* The method takes into account the same inputs as the effectiveParticleRadii() method,
* but is optimized for the efficient computation of the radius of a single particle.
* The global scaling factor defined in the ParticlesVis visual element is always applied to the computed radius.
******************************************************************************/
GraphicsFloatType ParticlesVis::effectiveParticleRadius(size_t particleIndex, BufferReadAccess<GraphicsFloatType> radiusProperty, const Property* typeProperty) const
{
    OVITO_ASSERT(typeProperty == nullptr || typeProperty->typeId() == Particles::TypeProperty);

    if(radiusProperty && radiusProperty.size() > particleIndex) {
        // Take particle radius directly from the radius property.
        GraphicsFloatType r = radiusProperty[particleIndex];
        if(r > 0)
            return r * radiusScaleFactor();
    }

    if(typeProperty && typeProperty->size() > particleIndex) {
        // Determine radius from particle type.
        BufferReadAccess<int32_t> typeAcc(typeProperty);
        const ParticleType* ptype = static_object_cast<ParticleType>(typeProperty->elementType(typeAcc[particleIndex]));
        if(ptype && ptype->radius() > 0)
            return ptype->radius() * radiusScaleFactor();
    }

    return defaultParticleRadius() * radiusScaleFactor();
}

/******************************************************************************
* Returns the actual rendering quality used to render the particles.
******************************************************************************/
ParticlePrimitive::RenderingQuality ParticlesVis::effectiveRenderingQuality(bool isInteractiveRenderer, const Particles* particles) const
{
    ParticlePrimitive::RenderingQuality renderQuality = renderingQuality();
    if(renderQuality != ParticlePrimitive::AutoQuality)
        return renderQuality;

    if(!particles)
        return ParticlePrimitive::HighQuality;

    size_t particleCount = particles->elementCount();
    if(particleCount < 4000 || !isInteractiveRenderer)
        return ParticlePrimitive::HighQuality;
    else {
        if(particleCount < 400000)
            return ParticlePrimitive::MediumQuality;

        if(particles->bonds() && particles->bonds()->elementCount() > 0) {
            // If the system contains bonds, never use the low quality mode, because the missing depth of the atomic spheres
            // would make the bonds appear incorrectly.
            const BondsVis* bondsVis = particles->bonds()->visElement<BondsVis>();
            if(bondsVis && bondsVis->isEnabled())
                return ParticlePrimitive::MediumQuality;
        }

        return ParticlePrimitive::LowQuality;
    }
}

/******************************************************************************
* Returns the effective primitive shape for rendering the particles.
******************************************************************************/
ParticlePrimitive::ParticleShape ParticlesVis::effectiveParticleShape(ParticleShape shape, const Property* shapeProperty, const Property* orientationProperty, const Property* roundnessProperty)
{
    if(shape == Sphere) {
        if(roundnessProperty != nullptr) return ParticlePrimitive::SuperquadricShape;
        if(shapeProperty != nullptr) return ParticlePrimitive::EllipsoidShape;
        else return ParticlePrimitive::SphericalShape;
    }
    else if(shape == Box) {
        if(shapeProperty != nullptr || orientationProperty != nullptr) return ParticlePrimitive::BoxShape;
        else return ParticlePrimitive::SquareCubicShape;
    }
    else if(shape == Circle) {
        return ParticlePrimitive::SphericalShape;
    }
    else if(shape == Square) {
        return ParticlePrimitive::SquareCubicShape;
    }
    else {
        OVITO_ASSERT(false);
        return ParticlePrimitive::SphericalShape;
    }
}

/******************************************************************************
* Lets the visualization element render the data object.
******************************************************************************/
Future<PipelineStatus> ParticlesVis::renderAsynchronous(const ConstDataObjectPath& path, const PipelineFlowState& flowState, OORef<FrameGraph> frameGraph, OORef<const SceneNode> sceneNode)
{
    // Get input particle data.
    DataOORef<const Particles> particles = path.lastAs<Particles>();
    if(!particles)
        co_return Exception(tr("The data object this ParticlesVis is attached to is not a particles object."));
    particles->verifyIntegrity();

    // Make sure the 'Position' property is present.
    if(!particles->getProperty(Particles::PositionProperty))
        throw Exception(tr("Cannot display particles because the 'Position' property is not present."));

    // Make sure we don't exceed the internal limits. Rendering of more than 2 billion particles is not yet supported by OVITO.
    size_t particleCount = particles->elementCount();
    if(particleCount > (size_t)std::numeric_limits<int>::max()) {
        throw Exception(tr("This version of OVITO doesn't support rendering of more than %1 particles.").arg(std::numeric_limits<int>::max()));
    }

    // Create a command group for rendering the particles. This must be done in the main thread. All rendering commands generated by this visual element will be added to this group.
    FrameGraph::RenderingCommandGroup& commandGroup = frameGraph->addCommandGroup(FrameGraph::SceneLayer);

    // Get the world transformation matrix of the scene node. This must happen in the main thread, because the scene node is not thread-safe.
    // The world transform will be applied to all rendering commands generated by this visual element.
    const AffineTransformation tm = sceneNode->getWorldTransform(frameGraph->time());

    // Perform the actual rendering in a background thread to not block the GUI for too long.
    co_await ExecutorAwaiter(ThreadPoolExecutor());

    // Render all mesh-based particle types.
    renderMeshBasedParticles(particles, *frameGraph, commandGroup, sceneNode, tm);

    // Render all primitive particle types.
    renderPrimitiveParticles(particles, *frameGraph, commandGroup, sceneNode, tm);

    // Render all (sphero-)cylindrical particle types.
    renderCylindricalParticles(particles, *frameGraph, commandGroup, sceneNode, tm);

    // Check whether all numeric particle type IDs map to a corresponding ParticleType object.
    // If not, issue a warning because this likely indicates a problem with the input data.
    // The rendering will still proceed, but the affected particles will be rendered with
    // the default particle color and radius.
    PipelineStatus status;
    if(const Property* typeProperty = particles->getProperty(Particles::TypeProperty)) {
        status = frameGraph->visCache().lookup<PipelineStatus>(
            RendererResourceKey<struct ParticlesVisTypeIdCheckCache, ConstPropertyPtr>{typeProperty},
            [&](PipelineStatus& status) {
                boost::container::flat_set<int> ids;
                for(const ElementType* type : typeProperty->elementTypes()) {
                    if(type) {
                        // Insert numeric ID into set of valid type IDs. Also detect duplicate type IDs and issue a warning in this case.
                        if(ids.insert(type->numericId()).second == false) {
                            status.combine(PipelineStatus(PipelineStatus::Warning, tr("Duplicate numeric particle type ID %1 detected. This likely indicates a problem with the input data.").arg(type->numericId())));
                        }
                    }
                }
                std::vector<int32_t> undefinedTypeIds;
                for(const auto id : BufferReadAccess<int32_t>(typeProperty)) {
                    if(ids.find(id) == ids.end()) {
                        undefinedTypeIds.push_back(id);
                        ids.insert(id); // Avoid issuing multiple warnings for the same missing type ID.
                        if(undefinedTypeIds.size() >= 4)
                            break;
                    }
                }
                for(int id : undefinedTypeIds)
                    status.combine(PipelineStatus(PipelineStatus::Warning, tr("The numeric particle type ID %1, assigned to some particles, does not exist. Affected particles will be rendered with the default color, radius, and shape.").arg(id)));
            });
    }

    co_return status;
}

/******************************************************************************
* Renders particle types that have a mesh-based shape assigned.
******************************************************************************/
void ParticlesVis::renderMeshBasedParticles(const Particles* particles, FrameGraph& frameGraph, FrameGraph::RenderingCommandGroup& commandGroup, const SceneNode* sceneNode, const AffineTransformation& tm) const
{
    // Get input particle data.
    const Property* positionProperty = particles->getProperty(Particles::PositionProperty);
    const Property* radiusProperty = particles->getProperty(Particles::RadiusProperty);
    const Property* colorProperty = particles->getProperty(Particles::ColorProperty);
    const Property* typeProperty = particles->getProperty(Particles::TypeProperty);
    const Property* selectionProperty = frameGraph.isInteractive() ? particles->getProperty(Particles::SelectionProperty) : nullptr;
    const Property* transparencyProperty = particles->getProperty(Particles::TransparencyProperty);
    const Property* orientationProperty = particles->getProperty(Particles::OrientationProperty);
    if(!positionProperty || !typeProperty)
        return;

    // Compile list of particle types that have a mesh geometry assigned.
    QVarLengthArray<int, 10> shapeMeshParticleTypes;
    for(const ElementType* etype : typeProperty->elementTypes()) {
        if(const ParticleType* ptype = dynamic_object_cast<ParticleType>(etype)) {
            if(ptype->shape() == ParticlesVis::ParticleShape::Mesh && ptype->shapeMesh() && ptype->shapeMesh()->faceCount() != 0) {
                shapeMeshParticleTypes.push_back(ptype->numericId());
            }
        }
    }
    if(shapeMeshParticleTypes.empty())
        return;

    // The type of lookup key used for caching the mesh rendering primitives:
    using ShapeMeshCacheKey = RendererResourceKey<struct ParticlesVisMeshCache,
        ConstDataObjectRef,         // Particle type property
        FloatType,                  // Default particle radius
        FloatType,                  // Global radius scaling factor
        ConstDataObjectRef,         // Position property
        ConstDataObjectRef,         // Orientation property
        ConstDataObjectRef,         // Color property
        ConstDataObjectRef,         // Selection property
        ConstDataObjectRef,         // Transparency property
        ConstDataObjectRef          // Radius property
    >;

    // The data structure created for each mesh-based particle type.
    struct MeshParticleType {
        MeshPrimitive meshPrimitive;
        OORef<ParticlePickInfo> pickInfo;
        bool useMeshColors; ///< Controls the use of the original face colors from the mesh instead of the per-particle colors.
    };

    // Look up the rendering primitives for mesh-based particle types in the vis cache.
    const std::vector<MeshParticleType>& meshVisCache = frameGraph.visCache().lookup<std::vector<MeshParticleType>>(
        ShapeMeshCacheKey{
            typeProperty,
            defaultParticleRadius(),
            radiusScaleFactor(),
            positionProperty,
            orientationProperty,
            colorProperty,
            selectionProperty,
            transparencyProperty,
            radiusProperty},
        [&](std::vector<MeshParticleType>& meshVisCache) {

            // This data structure stores temporary per-particle instance data, separated by mesh-based particle type.
            struct MeshTypePerInstanceData {
                BufferFactory<AffineTransformationG> particleTMs{0};   /// AffineTransformation of each particle to be rendered.
                BufferFactory<ColorAG> particleColors{0};  /// Color of each particle to be rendered.
                BufferFactory<int32_t> particleIndices{0}; /// Index of each particle to be rendered in the original particles list.
            };
            std::vector<MeshTypePerInstanceData> perInstanceData;

            meshVisCache.reserve(shapeMeshParticleTypes.size());
            perInstanceData.reserve(shapeMeshParticleTypes.size());

            // Create one instanced mesh primitive for each mesh-based particle type.
            for(int typeId : shapeMeshParticleTypes) {
                // Create a new instanced mesh primitive for the particle type.
                const ParticleType* ptype = static_object_cast<ParticleType>(typeProperty->elementType(typeId));
                OVITO_ASSERT(ptype->shapeMesh());
                MeshParticleType meshType;
                meshType.meshPrimitive.setEmphasizeEdges(ptype->highlightShapeEdges());
                meshType.meshPrimitive.setWireframeWidth(0.0f); // Hardcode to thin wireframe edges for now. We could add a user-configurable property for this in the future if needed.
                meshType.meshPrimitive.setCullFaces(ptype->shapeBackfaceCullingEnabled());
                meshType.meshPrimitive.setMesh(ptype->shapeMesh());
                meshType.useMeshColors = ptype->shapeUseMeshColor();
                meshVisCache.push_back(std::move(meshType));
                perInstanceData.emplace_back();
            }

            // Compile the per-instance particle data (positions, orientations, colors, etc) for each mesh-based particle type.
            BufferReadAccessAndRef<ColorG> colors = effectiveParticleColors(particles, frameGraph.isInteractive());
            BufferReadAccessAndRef<GraphicsFloatType> radii = effectiveParticleRadii(particles, true);
            BufferReadAccess<int32_t> types(typeProperty);
            BufferReadAccess<Point3> positions(positionProperty);
            BufferReadAccess<QuaternionG> orientations(orientationProperty);
            BufferReadAccess<GraphicsFloatType> transparencies(transparencyProperty);
            size_t particleCount = particles->elementCount();
            for(size_t i = 0; i < particleCount; i++) {
                if(radii[i] <= 0)
                    continue;
                auto iter = std::ranges::find(shapeMeshParticleTypes, types[i]);
                if(iter == shapeMeshParticleTypes.end())
                    continue;
                size_t typeIndex = std::distance(shapeMeshParticleTypes.begin(), iter);
                AffineTransformationG tm = AffineTransformationG::scaling(radii[i]);
                if(positions)
                    tm.translation() = positions[i].toDataType<GraphicsFloatType>() - Point3G::Origin();
                if(orientations)
                    tm = tm * Matrix_3<GraphicsFloatType>::rotation(orientations[i].safelyNormalized());
                perInstanceData[typeIndex].particleTMs.push_back(tm);
                perInstanceData[typeIndex].particleColors.push_back(ColorAG(colors[i], transparencies ? qBound<GraphicsFloatType>(0, 1 - transparencies[i], 1) : 1));
                perInstanceData[typeIndex].particleIndices.push_back(i);
            }

            // Store the per-particle data into the mesh rendering primitives.
            for(size_t typeIndex = 0; typeIndex < meshVisCache.size(); typeIndex++) {
                if(meshVisCache[typeIndex].useMeshColors)
                    perInstanceData[typeIndex].particleColors.reset();
                meshVisCache[typeIndex].meshPrimitive.setInstancedRendering(
                    perInstanceData[typeIndex].particleTMs.take(),
                    perInstanceData[typeIndex].particleColors.take());
                // Create a picking structure for this set of particles.
                meshVisCache[typeIndex].pickInfo = OORef<ParticlePickInfo>::create(this, particles, perInstanceData[typeIndex].particleIndices.take());
            }
        });

    OVITO_ASSERT(meshVisCache.size() == shapeMeshParticleTypes.size());

    // Render the instanced mesh primitives, one for each particle type with a mesh-based shape.
    for(const MeshParticleType& t : meshVisCache) {

        // Update the pick info record with the latest particle data.
        t.pickInfo->setParticles(particles);

        // Add the instanced mesh primitive to the frame graph.
        commandGroup.addPrimitive(std::make_unique<MeshPrimitive>(t.meshPrimitive), tm, t.meshPrimitive.computeBoundingBox(frameGraph.visCache()), sceneNode, t.pickInfo);
    }
}

/******************************************************************************
* Renders all particles with a primitive shape (spherical, box, (super)quadrics).
******************************************************************************/
void ParticlesVis::renderPrimitiveParticles(const Particles* particles, FrameGraph& frameGraph, FrameGraph::RenderingCommandGroup& commandGroup, const SceneNode* sceneNode, const AffineTransformation& tm) const
{
    // Determine whether all particle types use the same uniform shape or not.
    ParticlesVis::ParticleShape uniformShape = particleShape();
    OVITO_ASSERT(uniformShape != ParticleShape::Default);
    if(uniformShape == ParticleShape::Default)
        return;
    const Property* typeProperty = particles->getProperty(Particles::TypeProperty);
    if(typeProperty) {
        for(const ElementType* etype : typeProperty->elementTypes()) {
            if(const ParticleType* ptype = dynamic_object_cast<ParticleType>(etype)) {
                ParticleShape ptypeShape = ptype->shape();
                if(ptypeShape == ParticleShape::Default)
                    ptypeShape = particleShape();
                if(ptypeShape != uniformShape) {
                    uniformShape = ParticleShape::Default; // This value indicates that particles do NOT all use one uniform shape.
                    break;
                }
            }
        }
    }

    // Quit early if all particles have a shape not handled by this method.
    if(uniformShape != ParticleShape::Default) {
        if(uniformShape != ParticleShape::Sphere &&
            uniformShape != ParticleShape::Box &&
            uniformShape != ParticleShape::Circle &&
            uniformShape != ParticleShape::Square)
            return;
    }

    // Get input particle data.
    const Property* positionProperty = particles->getProperty(Particles::PositionProperty);
    const Property* radiusProperty = particles->getProperty(Particles::RadiusProperty);
    const Property* colorProperty = particles->getProperty(Particles::ColorProperty);
    const Property* typeRadiusProperty = getParticleTypeRadiusProperty(particles);
    const Property* selectionProperty = frameGraph.isInteractive() ? particles->getProperty(Particles::SelectionProperty) : nullptr;
    const Property* transparencyProperty = particles->getProperty(Particles::TransparencyProperty);
    const Property* asphericalShapeProperty = particles->getProperty(Particles::AsphericalShapeProperty);
    const Property* orientationProperty = particles->getProperty(Particles::OrientationProperty);
    const Property* roundnessProperty = particles->getProperty(Particles::SuperquadricRoundnessProperty);
    if(!positionProperty)
        return;

    // Prepare the particle rendering primitive.
    ParticlePrimitive primitive;

    // Pick render quality level adaptively based on current number of particles.
    ParticlePrimitive::RenderingQuality primitiveRenderQuality = effectiveRenderingQuality(frameGraph.isInteractive(), particles);
    primitive.setRenderingQuality(primitiveRenderQuality);

    // Fill rendering primitive with particle properties.
    primitive.setPositions(positionProperty);
    primitive.setTransparencies(transparencyProperty);
    primitive.setSelection(selectionProperty);
    primitive.setOrientations(orientationProperty);
    primitive.setRoundness(roundnessProperty);
    primitive.setSelectionColor(selectionParticleColor());

    // Aspherical shape array may require extra work, because it is affected by the uniform particle scaling factor.
    if(radiusScaleFactor() == 1 || !asphericalShapeProperty) {
        primitive.setAsphericalShapes(asphericalShapeProperty);
    }
    else {
        // The lookup key for the cached aspherical shape property array with applied uniform scaling factor:
        using ParticleShapeCacheKey = RendererResourceKey<struct ParticlesVisShapeCache,
            ConstDataObjectRef,     // Aspherical shape property
            FloatType               // Scaling factor
        >;
        // Look up the scaled aspherical shape array in the vis cache, which have been multiplied with the uniform scaling factor.
        const ConstDataBufferPtr& scaledShapes = frameGraph.visCache().lookup<ConstDataBufferPtr>(
            ParticleShapeCacheKey(asphericalShapeProperty, radiusScaleFactor()),
            [&](ConstDataBufferPtr& scaledShapes) {
                // Make a copy of the original aspherical shape array and multiply all vectors with the scaling factor.
                BufferWriteAccessAndRef<Vector3G, access_mode::read_write> values = ConstDataBufferPtr::makeCopy(asphericalShapeProperty);
                for(Vector3G& s : values)
                    s *= radiusScaleFactor();
                scaledShapes = values.take();
            });
        primitive.setAsphericalShapes(scaledShapes);
    }

    // Create separate rendering primitives for the different shapes supported by the method.
    for(ParticlesVis::ParticleShape shape : {ParticleShape::Sphere, ParticleShape::Box, ParticleShape::Circle, ParticleShape::Square}) {

        // Skip this shape type if all particles are known to have another shape.
        if(uniformShape != ParticleShape::Default && uniformShape != shape)
            continue;

        // The lookup key for the cached particle indices for the current shape type:
        using ParticleCacheKey = RendererResourceKey<struct ParticlesVisPrimitiveCache,
            ConstDataObjectRef,                 // Particle type property
            ParticlesVis::ParticleShape,        // Current particle shape
            ParticlesVis::ParticleShape,        // Global particle shape
            size_t                              // Particle count
        >;

        // Look up or generate an array of particle indices for the current shape type.
        const ConstDataBufferPtr& indices = frameGraph.visCache().lookup<ConstDataBufferPtr>(
            ParticleCacheKey(
                typeProperty,
                shape,
                uniformShape,
                particles->elementCount()),
            [&](ConstDataBufferPtr& indices) {
                // Determine the set of particles to be rendered using the current primitive shape.
                if(uniformShape != shape) {
                    OVITO_ASSERT(typeProperty);

                    // Build list of type IDs that use the current shape.
                    std::vector<int> activeParticleTypes;
                    for(const ElementType* etype : typeProperty->elementTypes()) {
                        if(const ParticleType* ptype = dynamic_object_cast<ParticleType>(etype)) {
                            if(ptype->shape() == shape || (ptype->shape() == ParticleShape::Default && shape == particleShape()) || (ptype->shape() == ParticleShape::Mesh && !ptype->shapeMesh() && shape == ParticleShape::Box))
                                activeParticleTypes.push_back(ptype->numericId());
                        }
                    }

                    // Collect indices of all particles that have an active type.
                    BufferFactory<int32_t> activeParticleIndices(0);
                    size_t index = 0;
                    for(auto t : BufferReadAccess<int32_t>(typeProperty)) {
                        if(std::ranges::contains(activeParticleTypes, t))
                            activeParticleIndices.push_back(index);
                        index++;
                    }
                    indices = activeParticleIndices.take();
                }
            });

        if(indices && indices->size() == 0)
            continue;   // No particles to render using the current shape type.

        if(!primitive.radii()) {
            // The type of lookup key used for caching the particle radii:
            using RadiiCacheKey = RendererResourceKey<struct ParticlesVisPrimitiveRadiusCache,
                FloatType,                          // Default particle radius
                FloatType,                          // Global radius scaling factor
                ConstDataObjectRef,                 // Radius property
                ConstDataObjectRef,                 // Type property
                size_t                              // Particle count
            >;
            primitive.setRadii(frameGraph.visCache().lookup<ConstPropertyPtr>(
                RadiiCacheKey(
                    defaultParticleRadius(),
                    radiusScaleFactor(),
                    radiusProperty,
                    typeRadiusProperty,
                    particles->elementCount()),
                [&](ConstPropertyPtr& radiusBuffer) {
                    radiusBuffer = effectiveParticleRadii(particles, true);
                }));
        }

        if(!primitive.colors()) {
            // The type of lookup key used for caching the particle colors:
            using ColorCacheKey = RendererResourceKey<struct ParticlesVisPrimitiveColorCache,
                ConstDataObjectRef,                 // Type property
                ConstDataObjectRef,                 // Color property
                size_t                              // Particle count
            >;
            primitive.setColors(frameGraph.visCache().lookup<ConstPropertyPtr>(
                ColorCacheKey(
                    typeProperty,
                    colorProperty,
                    particles->elementCount()),
                [&](ConstPropertyPtr& colorBuffer) {
                    colorBuffer = effectiveParticleColors(particles, false);
                }));
        }

        // Configure rendering shape and shading style.
        ParticlePrimitive::ParticleShape primitiveParticleShape = effectiveParticleShape(shape, asphericalShapeProperty, orientationProperty, roundnessProperty);
        ParticlePrimitive::ShadingMode primitiveShadingMode = (shape == Circle || shape == Square) ? ParticlePrimitive::FlatShading : ParticlePrimitive::NormalShading;
        primitive.setParticleShape(primitiveParticleShape);
        primitive.setShadingMode(primitiveShadingMode);

        // Look up or create the picking info object.
        const auto& pickingInfo = frameGraph.visCache().lookup<OORef<ParticlePickInfo>>(
            RendererResourceKey<struct ParticlesVisPrimitivePickinginfoCache, ConstDataObjectRef, ConstDataObjectRef>{particles, indices},
            [&](OORef<ParticlePickInfo>& pickingInfo) {
                pickingInfo = OORef<ParticlePickInfo>::create(this, particles, indices);
            });

        // Filter the particle data arrays in the rendering primitive to contain only the particles with the current shape type.
        auto subsetPrimitive = std::make_unique<ParticlePrimitive>(primitive);
        subsetPrimitive->makeSubset(indices, frameGraph.visCache());

        // Add the particles primitive to the frame graph.
        const Box3 boundingBox = subsetPrimitive->computeBoundingBox(frameGraph.visCache());
        commandGroup.addPrimitive(std::move(subsetPrimitive), tm, boundingBox, sceneNode, pickingInfo);
    }
}

/******************************************************************************
* Renders all particles with a (sphero-)cylindrical shape.
******************************************************************************/
void ParticlesVis::renderCylindricalParticles(const Particles* particles, FrameGraph& frameGraph, FrameGraph::RenderingCommandGroup& commandGroup, const SceneNode* sceneNode, const AffineTransformation& tm) const
{
    // Determine whether all particle types use the same uniform shape or not.
    ParticlesVis::ParticleShape uniformShape = particleShape();
    OVITO_ASSERT(uniformShape != ParticleShape::Default);
    if(uniformShape == ParticleShape::Default)
        return;
    const Property* typeProperty = particles->getProperty(Particles::TypeProperty);
    if(typeProperty) {
        for(const ElementType* etype : typeProperty->elementTypes()) {
            if(const ParticleType* ptype = dynamic_object_cast<ParticleType>(etype)) {
                ParticleShape ptypeShape = ptype->shape();
                if(ptypeShape == ParticleShape::Default)
                    ptypeShape = particleShape();
                if(ptypeShape != uniformShape) {
                    uniformShape = ParticleShape::Default; // This value indicates that particles do NOT all use one uniform shape.
                    break;
                }
            }
        }
    }

    // Quit early if all particles have a shape not handled by this method.
    if(uniformShape != ParticleShape::Default) {
        if(uniformShape != ParticleShape::Cylinder &&
            uniformShape != ParticleShape::Spherocylinder)
            return;
    }

    // Get input particle data.
    const Property* positionProperty = particles->getProperty(Particles::PositionProperty);
    const Property* radiusProperty = particles->getProperty(Particles::RadiusProperty);
    const Property* colorProperty = particles->getProperty(Particles::ColorProperty);
    const Property* selectionProperty = frameGraph.isInteractive() ? particles->getProperty(Particles::SelectionProperty) : nullptr;
    const Property* transparencyProperty = particles->getProperty(Particles::TransparencyProperty);
    const Property* asphericalShapeProperty = particles->getProperty(Particles::AsphericalShapeProperty);
    const Property* orientationProperty = particles->getProperty(Particles::OrientationProperty);
    if(!positionProperty)
        return;

    ConstPropertyPtr colorBuffer;
    ConstPropertyPtr radiusBuffer;

    /// Create separate rendering primitives for the different shapes supported by the method.
    for(ParticlesVis::ParticleShape shape : {ParticleShape::Cylinder, ParticleShape::Spherocylinder}) {

        // Skip this shape if all particles are known to have a different shape.
        if(uniformShape != ParticleShape::Default && uniformShape != shape)
            continue;

        // The lookup key for the cached rendering primitive:
        using ParticleCacheKey = RendererResourceKey<struct ParticlesVisCylindersCache,
            ConstDataObjectRef,                 // Position property
            ConstDataObjectRef,                 // Type property
            ConstDataObjectRef,                 // Selection property
            ConstDataObjectRef,                 // Color property
            ConstDataObjectRef,                 // Transparency property
            ConstDataObjectRef,                 // Aspherical shape property
            ConstDataObjectRef,                 // Orientation property
            ConstDataObjectRef,                 // Radius property
            FloatType,                          // Default particle radius
            FloatType,                          // Global radius scaling factor
            ParticlesVis::ParticleShape,        // Global particle shape
            ParticlesVis::ParticleShape         // Local particle shape
        >;

        // Look up the rendering primitive in the vis cache.
        const auto& [cylinderPrimitive, spheresPrimitives, pickInfo] = frameGraph.visCache().lookup<std::tuple<CylinderPrimitive, std::array<ParticlePrimitive, 2>, OORef<ParticlePickInfo>>>(
            ParticleCacheKey(
                positionProperty,
                typeProperty,
                selectionProperty,
                colorProperty,
                transparencyProperty,
                asphericalShapeProperty,
                orientationProperty,
                radiusProperty,
                defaultParticleRadius(),
                radiusScaleFactor(),
                particleShape(),
                shape),
            [&](CylinderPrimitive& cylinderPrimitive, std::array<ParticlePrimitive, 2>& spheresPrimitives, OORef<ParticlePickInfo>& pickInfo) {

                // Determine the set of particles to be rendered using the current shape.
                BufferFactory<int32_t> activeParticleIndices;
                if(uniformShape != shape) {
                    OVITO_ASSERT(typeProperty);

                    // Build list of type IDs that use the current shape.
                    std::vector<int> activeParticleTypes;
                    for(const ElementType* etype : typeProperty->elementTypes()) {
                        if(const ParticleType* ptype = dynamic_object_cast<ParticleType>(etype)) {
                            if(ptype->shape() == shape || (ptype->shape() == ParticleShape::Default && shape == particleShape()))
                                activeParticleTypes.push_back(ptype->numericId());
                        }
                    }

                    // Collect indices of all particles that have an active type.
                    activeParticleIndices = BufferFactory<int32_t>(0);
                    size_t index = 0;
                    for(auto t : BufferReadAccess<int32_t>(typeProperty)) {
                        if(std::ranges::contains(activeParticleTypes, t))
                            activeParticleIndices.push_back(index);
                        index++;
                    }

                    if(activeParticleIndices.size() == 0) {
                        return;   // No particles to be rendered using the current primitive shape.
                    }
                }
                int effectiveParticleCount = activeParticleIndices ? activeParticleIndices.size() : particles->elementCount();

                // Create the rendering primitive for the cylinders.
                cylinderPrimitive.setShape(CylinderPrimitive::CylinderShape);
                cylinderPrimitive.setShadingMode(CylinderPrimitive::NormalShading);

                // Determine cylinder colors.
                if(!colorBuffer)
                    colorBuffer = effectiveParticleColors(particles, frameGraph.isInteractive());

                // Determine cylinder radii (only needed if aspherical shape property is not present).
                if(!radiusBuffer && !asphericalShapeProperty)
                    radiusBuffer = effectiveParticleRadii(particles, false);

                // Allocate cylinder data buffers.
                BufferFactory<Point3G> cylinderVertices(effectiveParticleCount * 2);
                BufferFactory<GraphicsFloatType> cylinderWidths(effectiveParticleCount);
                BufferFactory<ColorG> cylinderColors(effectiveParticleCount);
                BufferFactory<GraphicsFloatType> cylinderTransparencies = transparencyProperty ? BufferFactory<GraphicsFloatType>(effectiveParticleCount) : BufferFactory<GraphicsFloatType>{};
                BufferFactory<Point3G> spherePositions1;
                BufferFactory<Point3G> spherePositions2;
                BufferFactory<GraphicsFloatType> sphereRadii;
                if(shape == ParticleShape::Spherocylinder) {
                    spherePositions1 = BufferFactory<Point3G>(effectiveParticleCount);
                    spherePositions2 = BufferFactory<Point3G>(effectiveParticleCount);
                    sphereRadii = BufferFactory<GraphicsFloatType>(effectiveParticleCount);
                }

                // Fill data buffers.
                BufferReadAccess<Point3> positionArray(positionProperty);
                BufferReadAccess<Vector3G> asphericalShapeArray(asphericalShapeProperty);
                BufferReadAccess<QuaternionG> orientationArray(orientationProperty);
                BufferReadAccess<ColorG> colorsArray(colorBuffer);
                BufferReadAccess<GraphicsFloatType> radiiArray(radiusBuffer);
                BufferReadAccess<GraphicsFloatType> transparencies(transparencyProperty);
                const GraphicsFloatType scalingFactor = radiusScaleFactor();
                for(int index = 0; index < effectiveParticleCount; index++) {
                    int effectiveParticleIndex = activeParticleIndices ? activeParticleIndices[index] : index;
                    const Point3& center = positionArray[effectiveParticleIndex];
                    GraphicsFloatType radius, length;
                    if(asphericalShapeArray) {
                        radius = std::abs(asphericalShapeArray[effectiveParticleIndex].x()) * scalingFactor;
                        length = asphericalShapeArray[effectiveParticleIndex].z() * scalingFactor;
                    }
                    else {
                        radius = radiiArray[effectiveParticleIndex] * scalingFactor;
                        length = radius * 2;
                    }
                    Vector3G dir(0, 0, length);
                    if(orientationArray) {
                        dir = orientationArray[effectiveParticleIndex].safelyNormalized() * dir;
                    }
                    Point3G p = center.toDataType<GraphicsFloatType>() - (dir * GraphicsFloatType(0.5));
                    cylinderVertices[2 * index] = p;
                    cylinderVertices[2 * index + 1] = p + dir;
                    cylinderWidths[index] = 2 * radius;
                    cylinderColors[index] = colorsArray[effectiveParticleIndex];
                    if(cylinderTransparencies)
                        cylinderTransparencies[index] = transparencies[effectiveParticleIndex];
                    if(shape == ParticleShape::Spherocylinder) {
                        spherePositions1[index] = cylinderVertices[2 * index];
                        spherePositions2[index] = cylinderVertices[2 * index + 1];
                        sphereRadii[index] = radius;
                    }
                }
                cylinderPrimitive.setVertexPositions(cylinderVertices.take());
                cylinderPrimitive.setWidths(cylinderWidths.take());
                cylinderPrimitive.setColors(cylinderColors.take());
                cylinderPrimitive.setTransparencies(cylinderTransparencies.take());

                // Create the rendering primitives for the spheres.
                if(shape == ParticleShape::Spherocylinder) {
                    spheresPrimitives[0].setParticleShape(ParticlePrimitive::SphericalShape);
                    spheresPrimitives[0].setShadingMode(ParticlePrimitive::NormalShading);
                    spheresPrimitives[0].setRenderingQuality(ParticlePrimitive::HighQuality);
                    spheresPrimitives[0].setPositions(spherePositions1.take());
                    spheresPrimitives[0].setRadii(sphereRadii.take());
                    spheresPrimitives[0].setColors(cylinderPrimitive.colors());
                    spheresPrimitives[0].setTransparencies(cylinderPrimitive.transparencies());
                    spheresPrimitives[1].setParticleShape(ParticlePrimitive::SphericalShape);
                    spheresPrimitives[1].setShadingMode(ParticlePrimitive::NormalShading);
                    spheresPrimitives[1].setRenderingQuality(ParticlePrimitive::HighQuality);
                    spheresPrimitives[1].setPositions(spherePositions2.take());
                    spheresPrimitives[1].setRadii(spheresPrimitives[0].radii());
                    spheresPrimitives[1].setColors(cylinderPrimitive.colors());
                    spheresPrimitives[1].setTransparencies(cylinderPrimitive.transparencies());
                }

                // Also create the corresponding picking record.
                pickInfo = OORef<ParticlePickInfo>::create(this, particles, activeParticleIndices.take());
            });

        if(!pickInfo)
            continue;

        // Update the pick info record with the latest particle data.
        pickInfo->setParticles(particles);

        // Add the cylinders to the frame graph.
        commandGroup.addPrimitive(std::make_unique<CylinderPrimitive>(cylinderPrimitive), tm, cylinderPrimitive.computeBoundingBox(frameGraph.visCache()), sceneNode, pickInfo);

        // Render the spherical caps.
        if(spheresPrimitives[0].positions()) {
            commandGroup.addPrimitive(std::make_unique<ParticlePrimitive>(spheresPrimitives[0]), tm, spheresPrimitives[0].computeBoundingBox(frameGraph.visCache()), sceneNode, pickInfo);
            commandGroup.addPrimitive(std::make_unique<ParticlePrimitive>(spheresPrimitives[1]), tm, spheresPrimitives[1].computeBoundingBox(frameGraph.visCache()), sceneNode, pickInfo);
        }
    }
}

/******************************************************************************
* Render a marker around a particle to highlight it in the viewports.
******************************************************************************/
void ParticlesVis::highlightParticle(size_t particleIndex, const Particles* particles, FrameGraph& frameGraph, const SceneNode* sceneNode) const
{
    // Fetch properties of selected particle which are needed to render the highlighting overlay.
    const Property* posProperty = nullptr;
    const Property* radiusProperty = nullptr;
    const Property* colorProperty = nullptr;
    const Property* selectionProperty = nullptr;
    const Property* shapeProperty = nullptr;
    const Property* orientationProperty = nullptr;
    const Property* roundnessProperty = nullptr;
    const Property* typeProperty = nullptr;
    for(const Property* property : particles->properties()) {
        if(property->typeId() == Particles::PositionProperty && property->size() >= particleIndex)
            posProperty = property;
        else if(property->typeId() == Particles::RadiusProperty && property->size() >= particleIndex)
            radiusProperty = property;
        else if(property->typeId() == Particles::TypeProperty && property->size() >= particleIndex)
            typeProperty = property;
        else if(property->typeId() == Particles::ColorProperty && property->size() >= particleIndex)
            colorProperty = property;
        else if(property->typeId() == Particles::SelectionProperty && property->size() >= particleIndex)
            selectionProperty = property;
        else if(property->typeId() == Particles::AsphericalShapeProperty && property->size() >= particleIndex)
            shapeProperty = property;
        else if(property->typeId() == Particles::OrientationProperty && property->size() >= particleIndex)
            orientationProperty = property;
        else if(property->typeId() == Particles::SuperquadricRoundnessProperty && property->size() >= particleIndex)
            roundnessProperty = property;
    }
    if(!posProperty || particleIndex >= posProperty->size())
        return;

    // Get the particle type.
    const ParticleType* ptype = nullptr;
    if(typeProperty && particleIndex < typeProperty->size()) {
        BufferReadAccess<int32_t> typeArray(typeProperty);
        ptype = dynamic_object_cast<ParticleType>(typeProperty->elementType(typeArray[particleIndex]));
    }

    // Check if the particle must be rendered using a custom shape.
    if(ptype && ptype->shape() == ParticleShape::Mesh && ptype->shapeMesh())
        return; // Note: Highlighting of particles with user-defined shapes is not implemented yet.

    // The rendering shape of the highlighted particle.
    ParticleShape shape = particleShape();
    if(ptype && ptype->shape() != ParticleShape::Default)
        shape = ptype->shape();

    // Determine position of the selected particle.
    Point3 pos = BufferReadAccess<Point3>(posProperty)[particleIndex];

    // Determine radius of selected particle.
    GraphicsFloatType radius = effectiveParticleRadius(particleIndex, radiusProperty, typeProperty);

    // Get world transformation matrix of scene node.
    const AffineTransformation& nodeTM = sceneNode->getWorldTransform(frameGraph.time());

    // Determine the display color of selected particle.
    ColorG color = effectiveParticleColor(particleIndex, colorProperty, typeProperty, selectionProperty);
    ColorG highlightColor = selectionParticleColor().toDataType<GraphicsFloatType>();
    color = color * GraphicsFloatType(0.5) + highlightColor * GraphicsFloatType(0.5);

    // Determine rendering quality used to render the particles.
    ParticlePrimitive::RenderingQuality renderQuality = effectiveRenderingQuality(frameGraph.isInteractive(), particles);

    std::unique_ptr<ParticlePrimitive> particleBuffer;
    std::unique_ptr<CylinderPrimitive> cylinderBuffer;
    if(shape != Cylinder && shape != Spherocylinder) {
        // Determine effective particle shape and shading mode.
        ParticlePrimitive::ParticleShape primitiveParticleShape = effectiveParticleShape(shape, shapeProperty, orientationProperty, roundnessProperty);
        ParticlePrimitive::ShadingMode primitiveShadingMode = ParticlePrimitive::NormalShading;
        if(shape == ParticlesVis::Circle || shape == ParticlesVis::Square)
            primitiveShadingMode = ParticlePrimitive::FlatShading;

        // Prepare data buffers.
        BufferFactory<Point3> positionBuffer(1);
        positionBuffer[0] = pos;
        BufferFactory<Vector3G> asphericalShapeBuffer;
        BufferFactory<Vector3G> asphericalShapeBufferHighlight;
        if(shapeProperty) {
            asphericalShapeBuffer = BufferFactory<Vector3G>(1);
            asphericalShapeBufferHighlight = BufferFactory<Vector3G>(1);
            const Vector3G shape = BufferReadAccess<Vector3G>(shapeProperty)[particleIndex];
            asphericalShapeBuffer[0] = shape * radiusScaleFactor();
        }
        BufferFactory<QuaternionG> orientationBuffer;
        if(orientationProperty) {
            orientationBuffer = BufferFactory<QuaternionG>(1);
            orientationBuffer[0] = BufferReadAccess<QuaternionG>(orientationProperty)[particleIndex];
        }
        BufferFactory<Vector_2<GraphicsFloatType>> roundnessBuffer;
        if(roundnessProperty) {
            roundnessBuffer = BufferFactory<Vector_2<GraphicsFloatType>>(1);
            roundnessBuffer[0] = BufferReadAccess<Vector_2<GraphicsFloatType>>(roundnessProperty)[particleIndex];
        }

        particleBuffer = std::make_unique<ParticlePrimitive>();
        particleBuffer->setParticleShape(primitiveParticleShape);
        particleBuffer->setShadingMode(primitiveShadingMode);
        particleBuffer->setRenderingQuality(renderQuality);
        particleBuffer->setUniformColor(color.toDataType<FloatType>());
        particleBuffer->setPositions(positionBuffer.take());
        particleBuffer->setUniformRadius(radius);
        particleBuffer->setAsphericalShapes(asphericalShapeBuffer.take());
        particleBuffer->setOrientations(orientationBuffer.take());
        particleBuffer->setRoundness(roundnessBuffer.take());
    }
    else if(shape == Cylinder || shape == Spherocylinder) {
        GraphicsFloatType radius, length;
        if(shapeProperty) {
            Vector3G shape = BufferReadAccess<Vector3G>(shapeProperty)[particleIndex] * radiusScaleFactor();
            radius = std::abs(shape.x());
            length = shape.z();
        }
        else {
            radius = defaultParticleRadius() * radiusScaleFactor();
            length = radius * 2;
        }
        Vector3G dir(0, 0, length);
        if(orientationProperty) {
            QuaternionG q = BufferReadAccess<QuaternionG>(orientationProperty)[particleIndex];
            dir = q.safelyNormalized() * dir;
        }
        BufferFactory<Point3G> vertexBuffer(2);
        vertexBuffer[0] = pos.toDataType<GraphicsFloatType>() - (dir * GraphicsFloatType(0.5));
        vertexBuffer[1] = pos.toDataType<GraphicsFloatType>() + (dir * GraphicsFloatType(0.5));
        cylinderBuffer = std::make_unique<CylinderPrimitive>();
        cylinderBuffer->setShape(CylinderPrimitive::CylinderShape);
        cylinderBuffer->setShadingMode(CylinderPrimitive::NormalShading);
        cylinderBuffer->setUniformColor(color.toDataType<FloatType>());
        cylinderBuffer->setUniformWidth(2 * radius);
        cylinderBuffer->setVertexPositions(vertexBuffer.take());
        if(shape == Spherocylinder) {
            particleBuffer = std::make_unique<ParticlePrimitive>();
            particleBuffer->setParticleShape(ParticlePrimitive::SphericalShape);
            particleBuffer->setShadingMode(ParticlePrimitive::NormalShading);
            particleBuffer->setRenderingQuality(ParticlePrimitive::HighQuality);
            particleBuffer->setPositions(cylinderBuffer->vertexPositions());
            particleBuffer->setUniformRadius(radius);
            particleBuffer->setUniformColor(color.toDataType<FloatType>());
        }
    }

    FrameGraph::RenderingCommandGroup& commandGroup = frameGraph.addCommandGroup(FrameGraph::HighlightLayer);

    // Highlight commands must not produce depth-aware outlines themselves — the highlight
    // post-process renders its own silhouette outline via the mask dilation step.
    if(particleBuffer)
        frameGraph.addPrimitiveNonpickable(commandGroup, std::move(particleBuffer), sceneNode).setExcludeFromOutline(true);
    if(cylinderBuffer)
        frameGraph.addPrimitiveNonpickable(commandGroup, std::move(cylinderBuffer), sceneNode).setExcludeFromOutline(true);
}

/******************************************************************************
* Given an sub-object ID returned by the Viewport::pick() method, looks up the
* corresponding particle index.
******************************************************************************/
size_t ParticlePickInfo::particleIndexFromSubObjectID(uint32_t subobjID) const
{
    if(_subobjectToParticleMapping && subobjID < _subobjectToParticleMapping->size())
        return BufferReadAccess<int32_t>(_subobjectToParticleMapping)[subobjID];
    return subobjID;
}

/******************************************************************************
* Returns a human-readable string describing the picked object,
* which will be displayed in the status bar by OVITO.
******************************************************************************/
QString ParticlePickInfo::infoString(const Pipeline* pipeline, uint32_t subobjectId)
{
    size_t particleIndex = particleIndexFromSubObjectID(subobjectId);
    return particles()->elementInfoString(particleIndex);
}

}   // End of namespace
