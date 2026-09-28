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


#include <ovito/core/Core.h>
#include <ovito/core/dataset/data/DataBuffer.h>
#include "RenderingPrimitive.h"

namespace Ovito {

/**
 * \brief A batch of camera-facing text labels rendered as textured quads in the 3d scene.
 *
 * Unlike TextPrimitive, which is drawn as a 2d overlay on top of the rendered image, this
 * primitive lives in the scene layer: the label quads are depth-tested against the scene
 * geometry. The label glyphs are supplied as one texture-atlas image, which is rasterized
 * and deduplicated by the creator of the primitive (see TextLabelsVis); each label instance
 * references a rectangular region of the atlas.
 *
 * The quads keep a constant apparent size on screen (their extent is given in device pixels)
 * and always face the camera. In the vertex shader, each label's anchor point is additionally
 * shifted toward the camera by a per-label radius plus a uniform depth offset, so that a label
 * attached to a glyph surfaces in front of it instead of being buried inside.
 */
class OVITO_CORE_EXPORT TextBillboardPrimitive final : public RenderingPrimitive
{
    Q_GADGET

#ifndef OVITO_BUILD_MONOLITHIC
    // Give this exported c++ class a "key function" to work around dynamic_cast problems (observed on macOS platform).
    // This function is not actually used but ensures that the class' vtable ends up in the core module.
    // See also http://itanium-cxx-abi.github.io/cxx-abi/abi.html#vague-vtable
    virtual void __key_function() override;
#endif

public:

    /// Sets the texture atlas image containing the rasterized label glyphs.
    void setAtlasImage(QImage image) { _atlasImage = std::move(image); }

    /// Returns the texture atlas image containing the rasterized label glyphs.
    const QImage& atlasImage() const { return _atlasImage; }

    /// Sets the anchor points of the labels (Point3, model space, sorted back to front).
    void setPositions(ConstDataBufferPtr positions) {
        OVITO_ASSERT(positions);
        OVITO_ASSERT(positions->componentCount() == 3 && positions->dataType() == DataBuffer::FloatDefault);
        _positions = std::move(positions);
    }

    /// Returns the buffer storing the label anchor points.
    const ConstDataBufferPtr& positions() const { return _positions; }

    /// Sets the per-label world-space radii by which the anchor points are shifted toward the camera.
    /// May be null, in which case the uniform radius applies to all labels.
    void setRadii(ConstDataBufferPtr radii) {
        OVITO_ASSERT(!radii || (radii->componentCount() == 1 && radii->dataType() == DataBuffer::Float32));
        _radii = std::move(radii);
    }

    /// Returns the buffer storing the per-label radii (may be null).
    const ConstDataBufferPtr& radii() const { return _radii; }

    /// Sets the world-space radius applied to all labels when no per-label radii are given.
    void setUniformRadius(FloatType radius) { _uniformRadius = radius; }

    /// Returns the world-space radius applied to all labels when no per-label radii are given.
    FloatType uniformRadius() const { return _uniformRadius; }

    /// Sets the largest radius occurring in the label set. It pads the primitive's bounding
    /// box so that the near/far clipping planes account for the forward shift of the anchors.
    void setMaxRadius(FloatType radius) { _maxRadius = radius; }

    /// Returns the largest radius occurring in the label set.
    FloatType maxRadius() const { return _maxRadius; }

    /// Sets the atlas regions of the labels (Float32, 4 components: u0,v0,u1,v1 in normalized atlas coordinates).
    void setUVRects(ConstDataBufferPtr uvRects) {
        OVITO_ASSERT(uvRects);
        OVITO_ASSERT(uvRects->componentCount() == 4 && uvRects->dataType() == DataBuffer::Float32);
        _uvRects = std::move(uvRects);
    }

    /// Returns the buffer storing the atlas regions of the labels.
    const ConstDataBufferPtr& uvRects() const { return _uvRects; }

    /// Sets the on-screen extents of the labels (Float32, 2 components: width/height in device pixels).
    void setSizes(ConstDataBufferPtr sizes) {
        OVITO_ASSERT(sizes);
        OVITO_ASSERT(sizes->componentCount() == 2 && sizes->dataType() == DataBuffer::Float32);
        _sizes = std::move(sizes);
    }

    /// Returns the buffer storing the on-screen extents of the labels.
    const ConstDataBufferPtr& sizes() const { return _sizes; }

    /// Sets the fraction of the quad size (per axis) between the quad's top-left corner and the
    /// anchor point. (0,0) pins the top-left corner to the anchor, (1,1) the bottom-right corner,
    /// (0.5,0.5) centers the quad on the anchor. The y-axis points downward, as in window coordinates.
    void setAlignmentFactor(const Vector2& factor) { _alignmentFactor = factor; }

    /// Returns the alignment factor of the label quads.
    const Vector2& alignmentFactor() const { return _alignmentFactor; }

    /// Sets the unit vector (window coordinates, y down) in which each label is pushed away
    /// from its anchor by the label's on-screen radius. May be the null vector for centered labels.
    void setShiftDirection(const Vector2& direction) { _shiftDirection = direction; }

    /// Returns the direction in which labels are pushed away from their anchors.
    const Vector2& shiftDirection() const { return _shiftDirection; }

    /// Sets the constant offset (in device pixels, window coordinates) applied to all labels.
    void setPixelOffset(const Vector2& offset) { _pixelOffset = offset; }

    /// Returns the constant pixel offset applied to all labels.
    const Vector2& pixelOffset() const { return _pixelOffset; }

    /// Sets the extra distance, in world units, by which a label is lifted toward the camera on top
    /// of its radius. Avoids depth-test ties with the annotated glyph. Being an absolute length, it
    /// lifts the labels of radius-less elements too.
    void setDepthOffset(FloatType offset) { _depthOffset = offset; }

    /// Returns the extra camera-facing shift applied to the labels, in world units.
    FloatType depthOffset() const { return _depthOffset; }

    /// Sets whether the labels are drawn in front of all other objects in the scene. When enabled,
    /// the labels are rendered without depth testing and are never occluded by scene geometry.
    void setAlwaysInFront(bool enable) { _alwaysInFront = enable; }

    /// Returns whether the labels are drawn in front of all other objects in the scene.
    bool alwaysInFront() const { return _alwaysInFront; }

    /// Computes the 3d bounding box of the primitive in local coordinate space.
    /// The box covers only the anchor points (padded by the maximum radius plus the depth offset);
    /// the screen-space extent of the labels is view-dependent and deliberately ignored, following
    /// the policy of MarkerPrimitive.
    virtual Box3 computeBoundingBox(const RendererResourceCache::ResourceFrame& visCache) const override {
        return visCache.lookup<Box3>(
            RendererResourceKey<struct TextBillboardBoundingBoxCache, ConstDataBufferPtr, FloatType, FloatType>{positions(), maxRadius(), depthOffset()},
            [this](Box3& bb) {
                if(positions())
                    bb = positions()->boundingBox3().padBox(maxRadius() + depthOffset());
            });
    }

private:

    /// The texture atlas image containing the rasterized label glyphs.
    QImage _atlasImage;

    /// The label anchor points (Point3, model space, sorted back to front).
    ConstDataBufferPtr _positions;

    /// Optional per-label world-space radii (Float32, scalar).
    ConstDataBufferPtr _radii;

    /// The atlas regions of the labels (Float32, 4 components).
    ConstDataBufferPtr _uvRects;

    /// The on-screen extents of the labels in device pixels (Float32, 2 components).
    ConstDataBufferPtr _sizes;

    /// The alignment factor of the label quads relative to their anchor points.
    Vector2 _alignmentFactor{0.5, 0.5};

    /// The direction (window coordinates, y down) in which labels are pushed away from their anchors.
    Vector2 _shiftDirection = Vector2::Zero();

    /// The constant offset (device pixels) applied to all labels.
    Vector2 _pixelOffset = Vector2::Zero();

    /// The extra camera-facing shift applied to the labels, in world units.
    FloatType _depthOffset = 0.05;

    /// Whether the labels are drawn without depth testing, in front of all other scene objects.
    bool _alwaysInFront = false;

    /// The world-space radius applied to all labels when no per-label radii are given.
    FloatType _uniformRadius = 0;

    /// The largest radius occurring in the label set (used for bounding-box padding).
    FloatType _maxRadius = 0;
};

}   // End of namespace
