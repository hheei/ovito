// SPDX-FileCopyrightText: 2026 OVITO GmbH, Germany
// SPDX-License-Identifier: GPL-3.0-only OR MIT

#pragma once

#include <ovito/core/Core.h>
#include <ovito/core/rendering/MarkerPrimitive.h>
#include <ovito/core/rendering/FrameGraph.h>
#include <ovito/core/rendering/ObjectPickingMap.h>
#include "PrimitiveRenderer.h"

namespace Ovito {

/**
 * Renders MarkerPrimitive commands using QRhi.
 * Supports BoxShape markers drawn as instanced wireframe cubes (Lines topology).
 * Used by StandardRendererImplementation.
 */
class OVITO_CORE_EXPORT MarkerPrimitiveRenderer : public PrimitiveRenderer
{
public:

    /// Identifies the shader variant to use.
    enum class ShaderVariant {
        BoxVisual,   ///< Normal rendering of wireframe box markers.
        BoxPicking,  ///< Picking pass for wireframe box markers.
    };

    /// A prepared draw call for a marker primitive.
    struct DrawCall
    {
        const MarkerPrimitive* primitive;    ///< Pointer to the marker data (valid for the current frame).
        AffineTransformation modelWorldTM;   ///< Model-to-world transformation.
        FrameGraph::RenderLayerType layer;   ///< The render layer this draw call belongs to.
        ShaderVariant shader;                ///< Which shader variant to use.
        uint32_t objectId;                   ///< Picking object ID (0 if not a picking pass).
        size_t markerCount;                  ///< Number of markers (= positions->size()).

        /// Per-draw vertex buffers (looked up from the graphics cache each frame).
        struct {
            QRhiBuffer* positions = nullptr; ///< Per-instance center positions buffer.
        } bufs;
    };

    /// Constructor.
    using PrimitiveRenderer::PrimitiveRenderer;

    /// Phase 1: Builds draw calls from marker primitives.
    void buildDrawCalls(const MarkerPrimitive& primitive, const FrameGraph::RenderingCommand& command,
                        FrameGraph::RenderLayerType layer, bool isPickingPass, ObjectPickingMap* pickingMap);

    /// Phase 2: Uploads position buffers and UBOs.
    void prepareResourceUpdates(QRhiResourceUpdateBatch* batch, const ViewProjectionParameters& projParams,
                                QSize renderSize, bool isYUpInNDC, bool isYUpInFramebuffer, bool isPicking);

    /// Phase 3: Issues draw calls for the given layer inside the render pass.
    void draw(QRhiCommandBuffer* cb, QRhiRenderPassDescriptor* rpd,
              FrameGraph::RenderLayerType layer, bool isPicking);

    /// Clears the draw call list for the next frame.
    void clear();

private:

    /// Ensures the shared shader resource bindings (scene params + draw params UBOs) are created.
    QRhiShaderResourceBindings* ensureShaderResourceBindings();

    /// Ensures a graphics pipeline is valid for the given variant and depth settings.
    QRhiGraphicsPipeline* ensurePipeline(QRhiRenderPassDescriptor* rpd,
                                         ShaderVariant variant, bool depthTest, bool depthWrite);

private:

    /// Prepared draw calls for the current frame.
    std::vector<DrawCall> _drawCalls;

    /// Per-draw parameters UBO (packed with dynamic offsets, one slot per draw call).
    std::unique_ptr<QRhiBuffer> _drawParamsUBO;

    /// The aligned size of one draw-params slot in the dynamic UBO.
    quint32 _drawParamsAlignedSize = 0;

    /// Shared shader resource bindings (scene params at binding 0, draw params at binding 1).
    std::unique_ptr<QRhiShaderResourceBindings> _bindings;
};

}   // End of namespace
