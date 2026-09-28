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
#include <ovito/core/rendering/CylinderPrimitive.h>
#include <ovito/core/rendering/FrameGraph.h>
#include <ovito/core/rendering/ObjectPickingMap.h>
#include <ovito/core/viewport/ViewProjectionParameters.h>
#include "PrimitiveRenderer.h"

namespace Ovito {

/**
 * Renders CylinderPrimitive commands (cylinders and arrows) using QRhi.
 * Used by StandardRendererImplementation.
 *
 * Supports NormalShading (ray-cast) and FlatShading (billboard) for both
 * CylinderShape and ArrowShape, with RGB, dual-color, and pseudo-color modes.
 * Transparency uses painter's algorithm (sorted SSBO) or WBOIT.
 */
class OVITO_CORE_EXPORT CylinderPrimitiveRenderer : public PrimitiveRenderer
{
public:

    /// Identifies the shader variant to use.
    enum class ShaderVariant {
        // CylinderShape – NormalShading (ray-cast bounding box)
        CylinderRaycast,
        CylinderRaycastPicking,
        CylinderRaycastSorted,

        // CylinderShape – FlatShading (screen-aligned quad billboard)
        CylinderFlat,
        CylinderFlatPicking,
        CylinderFlatSorted,

        // ArrowShape – NormalShading – cone head
        ArrowHeadRaycast,
        ArrowHeadRaycastPicking,
        ArrowHeadRaycastSorted,

        // ArrowShape – NormalShading – cylinder tail
        ArrowTailRaycast,
        ArrowTailRaycastPicking,
        ArrowTailRaycastSorted,

        // ArrowShape – FlatShading (triangle-list billboard)
        ArrowFlat,
        ArrowFlatPicking,
        ArrowFlatSorted,
    };

    /// A prepared draw call for a cylinder/arrow primitive.
    struct DrawCall
    {
        const CylinderPrimitive* primitive = nullptr;   ///< Valid for the frame.
        AffineTransformation modelWorldTM;
        ShaderVariant shader = ShaderVariant::CylinderRaycast;
        bool hasTransparency  = false;
        bool isPseudoColor    = false;                  ///< Use scalar pseudo-color vertex shader.
        bool excludeFromOutline = false;                ///< If true, skip in the outline depth pre-pass.
        bool filteredOut = false;                       ///< If true, handled by a parent renderer (e.g. ANARI); skip in main color pass but NOT in outline depth pre-pass.
        uint32_t objectId    = 0;
        size_t cylinderCount = 0;
        uint32_t verticesPerInstance = 14;              ///< 14 (raycast), 4 (flat cyl), 15 (flat arrow).
        QRhiGraphicsPipeline::Topology topology = QRhiGraphicsPipeline::TriangleStrip;
        FrameGraph::RenderLayerType layer = FrameGraph::SceneLayer; ///< Frame graph layer this draw call belongs to.

        /// Per-cylinder data buffers.
        struct {
            QRhiBuffer* positions     = nullptr;
            QRhiBuffer* widths        = nullptr;
            QRhiBuffer* color1        = nullptr;    ///< RGB vec3 per cyl (or float scalar if pseudo).
            QRhiBuffer* color2        = nullptr;    ///< Same or dual-color second endpoint.
            QRhiBuffer* transp1       = nullptr;
            QRhiBuffer* transp2       = nullptr;
            QRhiBuffer* selection     = nullptr;
        } bufs;

        QRhiTexture* colorMapTexture = nullptr;         ///< Owned by cache; non-null if pseudo-color.

        /// Per-draw shader resource bindings for the VBO path (non-null only for pseudo-color draws).
        std::unique_ptr<QRhiShaderResourceBindings> vboBindings;

        /// For sorted (painter's algorithm) draw calls.
        struct {
            QRhiBuffer* sortedIndexBuffer = nullptr;
            std::unique_ptr<QRhiShaderResourceBindings> shaderResourceBindings;
        } sorted;
    };

    /// Constructor.
    using PrimitiveRenderer::PrimitiveRenderer;

    /// Phase 1: Builds draw calls from cylinder primitives.
    void buildDrawCalls(const CylinderPrimitive& primitive, const FrameGraph::RenderingCommand& command,
                        bool isPickingPass, ObjectPickingMap* pickingMap,
                        FrameGraph::RenderLayerType layer = FrameGraph::SceneLayer,
                        bool filteredOut = false);

    /// Phase 2: Uploads vertex buffers and UBOs. Computes sort keys for transparent draw calls.
    void prepareResourceUpdates(QRhiResourceUpdateBatch* batch, const ViewProjectionParameters& projParams,
                                QSize renderSize, bool isYUpInNDC, bool isYUpInFramebuffer, bool isPicking,
                                bool isInteractive);

    /// Phase 3: Issues draw calls inside the main render pass.
    void draw(QRhiCommandBuffer* cb, QRhiRenderPassDescriptor* rpd,
              bool opaqueOnly, bool transparentOnly, bool isPicking);

    /// Draws all OverLayer cylinders with depth testing disabled (called from renderLayerInPass for OverLayer).
    void drawOverlay(QRhiCommandBuffer* cb, QRhiRenderPassDescriptor* rpd);

    /// Draws all HighlightLayer cylinders with depth testing disabled into the highlight silhouette render target.
    void drawHighlightSilhouette(QRhiCommandBuffer* cb, QRhiRenderPassDescriptor* rpd);

    /// Draws ExcludeFromOutline SceneLayer cylinders into the depth-only excluded-depth pre-pass target.
    void drawExclusionDepth(QRhiCommandBuffer* cb, QRhiRenderPassDescriptor* rpd);

    /// Returns true if any draw call has transparency.
    bool hasTransparentDrawCalls() const;

    /// Draws transparent cylinders into the OIT accumulation render pass.
    void drawOITAccumPass(QRhiCommandBuffer* cb, QRhiRenderPassDescriptor* rpd);

    /// Draws transparent cylinders into the OIT reveal render pass.
    void drawOITRevealPass(QRhiCommandBuffer* cb, QRhiRenderPassDescriptor* rpd);

    /// Clears the draw call list for the next frame.
    void clear();

    /// Returns the estimated Metal TBDR parameter buffer bytes used by all current draw calls.
    size_t metalParamBufferUsage() const;

private:

    enum class PipelineFlag {
        DepthTest      = (1 << 0),
        DepthWrite     = (1 << 1),
        Blend          = (1 << 2),
        NoTransparency = (1 << 3),
        NoSelection    = (1 << 4),
        OITAccum       = (1 << 5),
        OITReveal      = (1 << 6),
        OITDepthPrime  = (1 << 7),
        SortedSSBO     = (1 << 8),
        PseudoColor    = (1 << 9),
        DepthOnly      = (1 << 10),  ///< Outline depth pre-pass: depth-only render target (zero color attachments).
    };
    Q_DECLARE_FLAGS(PipelineFlags, PipelineFlag)

    static ShaderVariant toSortedVariant(ShaderVariant v);

    /// Returns the shared VBO shader resource bindings (UBOs only, for non-pseudo draw calls).
    QRhiShaderResourceBindings* ensureShaderResourceBindings();

    /// Ensures a graphics pipeline is valid for the given configuration.
    QRhiGraphicsPipeline* ensurePipeline(QRhiRenderPassDescriptor* rpd, ShaderVariant variant, PipelineFlags flags, const DrawCall& dc);

    /// Uploads VBO data for a draw call.
    void uploadInstanceData(QRhiResourceUpdateBatch* batch, DrawCall& dc, bool isPicking);

    /// Uploads SSBO data for a sorted transparent draw call.
    void uploadInstanceDataSorted(QRhiResourceUpdateBatch* batch, DrawCall& dc, bool isInteractive, const ViewProjectionParameters& projParams);

    /// Internal draw overload that handles all rendering modes.
    void draw(QRhiCommandBuffer* cb, QRhiRenderPassDescriptor* rpd,
              bool opaqueOnly, bool transparentOnly, bool isPicking,
              bool oitAccum, bool oitReveal, bool oitDepthPrime = false,
              bool overlayOnly = false, bool highlightSilhouetteOnly = false,
              bool excludedDepthOnly = false);

    /// Extracts every elementStride-th element (starting at elementOffset) from buffer into a new buffer.
    QRhiBuffer* extractStridedBuffer(const ConstDataBufferPtr& buffer,
                                     size_t elementOffset, size_t elementStride,
                                     size_t count, size_t componentCount,
                                     QRhiResourceUpdateBatch* batch,
                                     QRhiBuffer::UsageFlags usage = QRhiBuffer::VertexBuffer);

    std::vector<DrawCall> _drawCalls;

    std::unique_ptr<QRhiBuffer> _drawParamsUBO;
    quint32 _drawParamsAlignedSize = 0;

    /// Shared VBO shader resource bindings (binding 0: SceneParams, binding 1: DrawParams dynamic).
    std::unique_ptr<QRhiShaderResourceBindings> _bindings;
};

}   // End of namespace
