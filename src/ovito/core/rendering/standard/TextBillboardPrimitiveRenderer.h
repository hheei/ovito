// SPDX-FileCopyrightText: 2026 OVITO GmbH, Germany
// SPDX-License-Identifier: GPL-3.0-only OR MIT

#pragma once

#include <ovito/core/Core.h>
#include <ovito/core/rendering/TextBillboardPrimitive.h>
#include <ovito/core/rendering/FrameGraph.h>
#include "PrimitiveRenderer.h"

namespace Ovito {

/**
 * Renders TextBillboardPrimitive commands using QRhi.
 *
 * Each primitive becomes one instanced draw call: camera-facing quads with constant
 * apparent screen size, textured from a shared glyph atlas, depth-tested against the
 * scene but not writing depth. The quads are alpha-blended in the order stored in the
 * primitive's instance buffers (back to front). Used by StandardRendererImplementation.
 */
class OVITO_CORE_EXPORT TextBillboardPrimitiveRenderer : public PrimitiveRenderer
{
public:

    /// A prepared draw call for a text billboard primitive.
    struct DrawCall
    {
        const TextBillboardPrimitive* primitive; ///< Pointer to the label data (valid for the current frame).
        AffineTransformation modelWorldTM;       ///< Model-to-world transformation.
        FrameGraph::RenderLayerType layer;       ///< The render layer this draw call belongs to.
        size_t labelCount;                       ///< Number of label instances (= positions->size()).
        bool alwaysInFront;                      ///< Whether to draw the labels without depth testing.

        /// Per-draw GPU resources (looked up from the graphics cache each frame).
        struct {
            QRhiBuffer* positions = nullptr;     ///< Per-instance anchor points.
            QRhiBuffer* radii = nullptr;         ///< Per-instance world-space radii.
            QRhiBuffer* uvRects = nullptr;       ///< Per-instance atlas regions.
            QRhiBuffer* sizes = nullptr;         ///< Per-instance on-screen extents in pixels.
        } bufs;
        QRhiTexture* texture = nullptr;          ///< The glyph atlas texture (owned by the resource cache).

        /// The shader resource bindings for this draw call. Rebuilt whenever the atlas texture changes.
        std::unique_ptr<QRhiShaderResourceBindings> bindings;
        QRhiTexture* boundTexture = nullptr;     ///< The texture the current bindings were created with.
    };

    /// Constructor.
    using PrimitiveRenderer::PrimitiveRenderer;

    /// Phase 1: Builds draw calls from text billboard primitives.
    void buildDrawCalls(const TextBillboardPrimitive& primitive, const FrameGraph::RenderingCommand& command,
                        FrameGraph::RenderLayerType layer);

    /// Phase 2: Uploads instance buffers, the atlas texture, and UBOs.
    void prepareResourceUpdates(QRhiResourceUpdateBatch* batch, const ViewProjectionParameters& projParams);

    /// Phase 3: Issues draw calls for the given layer inside the render pass.
    void draw(QRhiCommandBuffer* cb, QRhiRenderPassDescriptor* rpd, FrameGraph::RenderLayerType layer);

    /// Clears the draw call list for the next frame.
    void clear();

private:

    /// Ensures the graphics pipeline is valid for the given RPD and depth-test mode.
    QRhiGraphicsPipeline* ensurePipeline(QRhiRenderPassDescriptor* rpd, bool depthTest);

private:

    /// Prepared draw calls for the current frame.
    std::vector<DrawCall> _drawCalls;
    size_t _numDrawCalls = 0; ///< Number of recorded draw calls in the current frame.

    /// Per-draw parameters UBO (packed with dynamic offsets, one slot per draw call).
    std::unique_ptr<QRhiBuffer> _drawParamsUBO;

    /// The aligned size of one draw-params slot in the dynamic UBO.
    quint32 _drawParamsAlignedSize = 0;

    /// Shared linear sampler for the atlas textures.
    std::unique_ptr<QRhiSampler> _sampler;
};

}   // End of namespace
