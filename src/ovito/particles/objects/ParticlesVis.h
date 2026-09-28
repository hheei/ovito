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

#pragma once


#include <ovito/particles/Particles.h>
#include <ovito/particles/objects/Particles.h>
#include <ovito/stdobj/properties/Property.h>
#include <ovito/core/rendering/SceneRenderer.h>
#include <ovito/core/rendering/ParticlePrimitive.h>
#include <ovito/core/dataset/data/DataVis.h>

namespace Ovito {

/**
 * \brief A visualization element for rendering particles.
 */
class OVITO_PARTICLES_EXPORT ParticlesVis : public DataVis
{
    OVITO_CLASS(ParticlesVis)

public:

    /// The standard shapes supported by the particles visualization element.
    enum ParticleShape {
        Sphere,             // Includes ellipsoids and superquadrics
        Box,                // Includes cubes and non-cubic boxes
        Circle,
        Square,
        Cylinder,
        Spherocylinder,
        Mesh,
        Default
    };
    Q_ENUM(ParticleShape);

public:

    /// Renders the data.
    virtual Future<PipelineStatus> renderAsynchronous(const ConstDataObjectPath& path, const PipelineFlowState& flowState, OORef<FrameGraph> frameGraph, OORef<const SceneNode> sceneNode) override;

    /// Computes the bounding box of the visual element.
    virtual Box3 boundingBoxImmediate(AnimationTime time, const ConstDataObjectPath& path, const Pipeline* pipeline, const PipelineFlowState& flowState, TimeInterval& validityInterval) override;

    /// Returns the default display color for particles.
    Color defaultParticleColor() const { return Color(1,1,1); }

    /// Returns the display color used for selected particles.
    Color selectionParticleColor() const { return Color(1,0,0); }

    /// Returns the actual particle shape used to render the particles.
    static ParticlePrimitive::ParticleShape effectiveParticleShape(ParticleShape shape, const Property* shapeProperty, const Property* orientationProperty, const Property* roundnessProperty);

    /// Returns the actual rendering quality used to render the particles.
    ParticlePrimitive::RenderingQuality effectiveRenderingQuality(bool isInteractiveRenderer, const Particles* particles) const;

    /// Computes the effective display colors of the particles used for rendering.
    /// This takes into account the following sources for particle colors, listed here in order of decreasing priority:
    ///   1) Per-particle color values defined in the "Color" property.
    ///   2) Per-type color values defined in the particle types assigned to the particles.
    ///   3) The uniform default particle color defined in the ParticlesVis visual element.
    /// Optionally, selected particles can be highlighted with a special color defined in the ParticlesVis visual element.
    ConstPropertyPtr effectiveParticleColors(const Particles* particles, bool highlightSelection) const;

    /// Computes the display color of a single particle.
    /// The method takes into account the same inputs as the effectiveParticleColors() method, but is optimized for the efficient computation of the color of a single particle.
    /// If a selection property is provided, the particle is highlighted with a special color defined in the ParticlesVis visual element when it is selected.
    ColorG effectiveParticleColor(size_t particleIndex, BufferReadAccess<ColorG> colorProperty, const Property* typeProperty, BufferReadAccess<SelectionIntType> selectionProperty) const;

    /// Computes the effective particle radii used for rendering particles.
    /// This takes into account the following sources for particle radii, listed here in order of decreasing priority:
    ///   1) Per-particle radius values defined in the "Radius" property.
    ///   2) Per-type radius values defined in the particle types assigned to the particles.
    ///   3) The uniform default particle radius defined in the "Radius" property of the ParticlesVis visual element.
    /// Whenever a radius is zero for a particle, the next source in the above list is used to determine the effective radius for that particle.
    /// Optionally, the global scaling factor defined in in the ParticlesVis visual element can be applied to the particle radii.
    ConstPropertyPtr effectiveParticleRadii(const Particles* particles, bool includeGlobalScaleFactor) const;

    /// Computes the effective display radius of a single particle.
    /// The method takes into account the same inputs as the effectiveParticleRadii() method, but is optimized for the efficient computation of the radius of a single particle.
    /// The global scaling factor defined in the ParticlesVis visual element is always applied to the computed radius.
    GraphicsFloatType effectiveParticleRadius(size_t particleIndex, BufferReadAccess<GraphicsFloatType> radiusProperty, const Property* typeProperty) const;

    /// Render a marker around a particle to highlight it in the viewports.
    void highlightParticle(size_t particleIndex, const Particles* particles, FrameGraph& frameGraph, const SceneNode* sceneNode) const;

    /// Returns the typed particle property used to determine the rendering colors of particles (if no per-particle colors are defined).
    virtual const Property* getParticleTypeColorProperty(const Particles* particles) const;

    /// Returns the typed particle property used to determine the rendering radii of particles (if no per-particle radii are defined).
    virtual const Property* getParticleTypeRadiusProperty(const Particles* particles) const;

public:

    Q_PROPERTY(Ovito::ParticlePrimitive::RenderingQuality renderingQuality READ renderingQuality WRITE setRenderingQuality)
    Q_PROPERTY(Ovito::ParticlesVis::ParticleShape particleShape READ particleShape WRITE setParticleShape)

private:

    /// Renders particle types that have a mesh-based shape assigned.
    void renderMeshBasedParticles(const Particles* particles, FrameGraph& frameGraph, FrameGraph::RenderingCommandGroup& commandGroup, const SceneNode* sceneNode, const AffineTransformation& tm) const;

    /// Renders all particles with a primitive shape (spherical, box, (super)quadrics).
    void renderPrimitiveParticles(const Particles* particles, FrameGraph& frameGraph, FrameGraph::RenderingCommandGroup& commandGroup, const SceneNode* sceneNode, const AffineTransformation& tm) const;

    /// Renders all particles with a (sphero-)cylindrical shape.
    void renderCylindricalParticles(const Particles* particles, FrameGraph& frameGraph, FrameGraph::RenderingCommandGroup& commandGroup, const SceneNode* sceneNode, const AffineTransformation& tm) const;

private:

    /// Controls the default display radius of particles.
    DECLARE_MODIFIABLE_PROPERTY_FIELD_FLAGS(FloatType{1.2}, defaultParticleRadius, setDefaultParticleRadius, PROPERTY_FIELD_MEMORIZE | PROPERTY_FIELD_RESETTABLE);

    /// Controls the global scaling factor, which is applied to all rendered particles.
    DECLARE_MODIFIABLE_PROPERTY_FIELD_FLAGS(FloatType{1.0}, radiusScaleFactor, setRadiusScaleFactor, PROPERTY_FIELD_MEMORIZE | PROPERTY_FIELD_RESETTABLE);

    /// Controls the rendering quality mode for particles.
    DECLARE_MODIFIABLE_PROPERTY_FIELD_FLAGS(ParticlePrimitive::RenderingQuality{ParticlePrimitive::AutoQuality}, renderingQuality, setRenderingQuality, PROPERTY_FIELD_RESETTABLE);

    /// Controls the display shape of particles.
    DECLARE_MODIFIABLE_PROPERTY_FIELD_FLAGS(ParticleShape{Sphere}, particleShape, setParticleShape, PROPERTY_FIELD_RESETTABLE);
};

/**
 * \brief This information record is attached to the particles by the ParticlesVis when rendering
 * them in the viewports. It facilitates the picking of particles with the mouse.
 */
class OVITO_PARTICLES_EXPORT ParticlePickInfo : public ObjectPickInfo
{
    OVITO_CLASS(ParticlePickInfo)

public:

    /// Constructor.
    void initializeObject(const ParticlesVis* visElement, DataOORef<const Particles> particles, ConstDataBufferPtr subobjectToParticleMapping = {}) {
        ObjectPickInfo::initializeObject();
        _visElement = visElement;
        _particles = std::move(particles);
        _subobjectToParticleMapping = std::move(subobjectToParticleMapping);
    }

    /// Returns the particles object.
    const DataOORef<const Particles>& particles() const { OVITO_ASSERT(_particles); return _particles; }

    /// Updates the reference to the particles object.
    void setParticles(DataOORef<const Particles> particles) { _particles = std::move(particles); }

    /// Returns a human-readable string describing the picked object, which will be displayed in the status bar by OVITO.
    virtual QString infoString(const Pipeline* pipeline, uint32_t subobjectId) override;

    /// Given an sub-object ID returned by the Viewport::pick() method, looks up the
    /// corresponding particle index.
    size_t particleIndexFromSubObjectID(uint32_t subobjID) const;

private:

    /// The vis element that rendered the particles.
    OORef<const ParticlesVis> _visElement;

    /// The particles object.
    DataOORef<const Particles> _particles;

    /// Stores the indices of the particles associated with the rendering primitives.
    ConstDataBufferPtr _subobjectToParticleMapping;
};

}   // End of namespace
