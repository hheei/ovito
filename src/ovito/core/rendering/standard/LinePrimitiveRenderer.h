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
#include <ovito/core/rendering/LinePrimitive.h>
#include <ovito/core/rendering/FrameGraph.h>
#include <ovito/core/rendering/ObjectPickingMap.h>
#include "PrimitiveRenderer.h"

namespace Ovito {

/**
 * Renders LinePrimitive commands using QRhi.
 * Supports 1-pixel thin lines (Lines topology) and wide lines (instanced triangle-strip quads).
 * Used by StandardRendererImplementation.
 */
class OVITO_CORE_EXPORT LinePrimitiveRenderer : public PrimitiveRenderer
{
public:

    /// Identifies the shader variant to use.
    enum class ShaderVariant {
        ThinLine,           ///< Visual, 1-pixel lines.
        ThinLinePicking,    ///< Picking, 1-pixel lines.
        ThickLine,          ///< Visual, wide lines rendered as instanced triangle-strip quads.
        ThickLinePicking,   ///< Picking, wide lines rendered as instanced triangle-strip quads.
    };

    /// A prepared draw call for a line primitive.
    struct DrawCall
    {
        const LinePrimitive* primitive;     ///< Pointer to the line data (valid for the current frame).
        AffineTransformation modelWorldTM;  ///< Model-to-world transformation (Zero() for pre-projected NDC coords).
        FrameGraph::RenderLayerType layer;  ///< The render layer this line belongs to.
        ShaderVariant shader;               ///< Which shader variant to use.
        bool isPreprojected;                ///< True when vertex positions are already in NDC space.
        float lineWidth;                    ///< Line width in device pixels.
        uint32_t objectId;                  ///< Picking object ID (0 if not a picking pass).
        size_t lineSegmentCount;            ///< Number of line segments (= positions->size() / 2).
        bool excludeFromOutline;            ///< True if this command is excluded from the outline effect.

        /// Per-draw vertex buffers (looked up from the graphics cache each frame).
        struct {
            QRhiBuffer* positions = nullptr; ///< Positions buffer (per-vertex for thin, per-instance pairs for thick).
            QRhiBuffer* colors = nullptr;    ///< Colors buffer (per-vertex for thin, per-instance pairs for thick). Null during picking.
        } bufs;
    };

    /// Constructor.
    using PrimitiveRenderer::PrimitiveRenderer;

    /// Phase 1: Builds draw calls from line primitives.
    void buildDrawCalls(const LinePrimitive& primitive, const FrameGraph::RenderingCommand& command,
                        FrameGraph::RenderLayerType layer, bool isPickingPass, ObjectPickingMap* pickingMap);

    /// Phase 2: Uploads vertex buffers and UBOs.
    void prepareResourceUpdates(QRhiResourceUpdateBatch* batch, const ViewProjectionParameters& projParams,
                                QSize renderSize, bool isYUpInNDC, bool isYUpInFramebuffer, bool isPicking);

    /// Phase 3: Issues draw calls for the given layer inside the render pass.
    void draw(QRhiCommandBuffer* cb, QRhiRenderPassDescriptor* rpd,
              FrameGraph::RenderLayerType layer, bool isPicking);

    /// Draws ExcludeFromOutline SceneLayer lines into the depth-only excluded-depth pre-pass target.
    void drawExclusionDepth(QRhiCommandBuffer* cb, QRhiRenderPassDescriptor* rpd);

    /// Clears the draw call list for the next frame.
    void clear();

private:

    /// Options for pipeline variants.
    enum class PipelineFlag {
        DepthTest    = (1 << 0),
        DepthWrite   = (1 << 1),
        UniformColor = (1 << 2), ///< No per-vertex/per-segment colors; uniform color uses a 1-pair VBO with max step rate (thick lines only).
        DepthOnly    = (1 << 3), ///< Depth-only target (no color attachments); uses the empty line_depth.frag.
    };
    Q_DECLARE_FLAGS(PipelineFlags, PipelineFlag)

    /// Uploads vertex buffers for one draw call.
    void uploadVertexData(QRhiResourceUpdateBatch* batch, DrawCall& dc, bool isPicking);

    /// Ensures the shared shader resource bindings (scene params + draw params UBOs) are created.
    QRhiShaderResourceBindings* ensureShaderResourceBindings();

    /// Ensures a graphics pipeline is valid for the given variant and pipeline flags.
    QRhiGraphicsPipeline* ensurePipeline(QRhiRenderPassDescriptor* rpd,
                                         ShaderVariant variant, PipelineFlags flags);

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
