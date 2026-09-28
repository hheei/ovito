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
#include <ovito/core/rendering/MeshPrimitive.h>
#include <ovito/core/rendering/FrameGraph.h>
#include <ovito/core/rendering/ObjectPickingMap.h>
#include <ovito/core/rendering/PseudoColorMapping.h>
#include <ovito/core/viewport/ViewProjectionParameters.h>
#include "PrimitiveRenderer.h"

namespace Ovito {

/**
 * Renders MeshPrimitive commands (triangle meshes) using QRhi.
 * Used by StandardRendererImplementation.
 *
 * Supports:
 *   - Non-instanced and instanced rendering
 *   - Per-vertex/per-face colors, material colors, uniform color
 *   - Pseudo-color mapping (non-instanced only)
 *   - Backface culling
 *   - Wireframe edge emphasis (thin 1px and thick screen-aligned quads)
 *   - Depth-sorted transparency (painter's algorithm)
 *   - Weighted Blended OIT
 *   - Picking (one ID per face for non-instanced, one ID per instance for instanced)
 */
class OVITO_CORE_EXPORT MeshPrimitiveRenderer : public PrimitiveRenderer
{
public:

    /// Identifies the shader variant to use for a draw call.
    enum class ShaderVariant {
        // Hidden faces
        None,

        // Non-instanced visual
        Mesh,
        MeshOITAccum,
        MeshOITReveal,

        // Non-instanced picking
        MeshPicking,

        // Non-instanced pseudo-color
        MeshColorMapping,

        // Instanced visual (no per-instance colors)
        MeshInstanced,
        MeshInstancedOITAccum,
        MeshInstancedOITReveal,

        // Instanced visual (with per-instance colors)
        MeshInstancedColors,
        MeshInstancedColorsOITAccum,
        MeshInstancedColorsOITReveal,

        // Instanced sorted (SSBO-based back-to-front reordering)
        MeshInstancedSorted,

        // Instanced picking
        MeshInstancedPicking,

        // Non-instanced wireframe: 1px Lines
        Wireframe,

        // Instanced wireframe: 1px Lines
        WireframeInstanced,

        // Non-instanced wireframe: thick screen-aligned quads (TriangleStrip)
        WireframeTri,

        // Instanced wireframe: thick screen-aligned quads (TriangleStrip)
        WireframeTriInstanced,
    };

    /// A prepared draw call for one mesh primitive.
    struct DrawCall
    {
        const MeshPrimitive* primitive = nullptr;  ///< Valid for the current frame.
        AffineTransformation modelWorldTM;
        ShaderVariant faceShader = ShaderVariant::Mesh;
        ShaderVariant wireframeShader = ShaderVariant::Wireframe;
        bool hasTransparency    = false;
        bool isPseudoColor      = false;
        bool needsWireframe     = false;
        bool isInstanced        = false;
        bool hasInstanceColors  = false;
        bool excludeFromOutline = false; ///< If true, skip in the outline depth pre-pass.
        bool filteredOut = false;        ///< If true, handled by a parent renderer (e.g. ANARI); skip in main color pass but NOT in outline depth pre-pass.
        uint32_t objectId       = 0;
        size_t faceCount       = 0;
        size_t instanceCount   = 0;
        size_t wireframeEdgeCount = 0;

        /// GPU buffers filled during Phase 2.
        struct {
            QRhiBuffer* vertices         = nullptr;  ///< MeshPrimitive::RenderVertex[]
            QRhiBuffer* sortedFaceIdxBuf = nullptr;  ///< uint32[] sorted face indices × 3
            QRhiBuffer* instanceTMs      = nullptr;  ///< per-instance: 3 × vec4 rows
            QRhiBuffer* instanceColors   = nullptr;  ///< per-instance: ColorAF
            QRhiBuffer* sortedInstIdxBuf = nullptr;  ///< uint32[] sorted instance indices (SSBO)
            QRhiBuffer* instanceTMsSSBO  = nullptr;  ///< instance TMs as SSBO (for sorted path)
            QRhiBuffer* instanceColorsSSBO = nullptr; ///< instance colors as SSBO (sorted path)
            QRhiBuffer* wireframeLines   = nullptr;  ///< Point3F pairs (edge endpoints)
            QRhiBuffer* wireframeTMsBuf  = nullptr;  ///< TM rows interleaved with edge pairs (instanced thick)
        } bufs;

        QRhiTexture* colorMapTexture = nullptr;

        /// Per-draw shader resource bindings for pseudo-color (includes colorMap sampler).
        std::unique_ptr<QRhiShaderResourceBindings> pseudoColorBindings;

        /// Per-draw shader resource bindings for sorted instanced path.
        std::unique_ptr<QRhiShaderResourceBindings> sortedInstanceBindings;
    };

    /// Constructor.
    using PrimitiveRenderer::PrimitiveRenderer;

    /// Phase 1: Build draw calls from mesh primitives.
    void buildDrawCalls(const MeshPrimitive& primitive, const FrameGraph::RenderingCommand& command,
                        bool isPickingPass, ObjectPickingMap* pickingMap, bool renderOnlyWireframe = false,
                        bool filteredOut = false);

    /// Phase 2: Upload vertex buffers and UBOs.
    void prepareResourceUpdates(QRhiResourceUpdateBatch* batch, const ViewProjectionParameters& projParams,
                                QSize renderSize, bool isYUpInNDC, bool isYUpInFramebuffer,
                                bool isPicking, bool isInteractive);

    /// Phase 3: Issue draw calls inside the main render pass.
    void draw(QRhiCommandBuffer* cb, QRhiRenderPassDescriptor* rpd,
              bool opaqueOnly, bool transparentOnly, bool isPicking);

    /// Returns true if any draw call has transparency.
    bool hasTransparentDrawCalls() const;

    /// Draws ExcludeFromOutline SceneLayer meshes into the depth-only excluded-depth pre-pass target.
    void drawExclusionDepth(QRhiCommandBuffer* cb, QRhiRenderPassDescriptor* rpd);

    /// Draws transparent meshes into the OIT accumulation render pass.
    void drawOITAccumPass(QRhiCommandBuffer* cb, QRhiRenderPassDescriptor* rpd);

    /// Draws transparent meshes into the OIT reveal render pass.
    void drawOITRevealPass(QRhiCommandBuffer* cb, QRhiRenderPassDescriptor* rpd);

    /// Clears the draw call list for the next frame.
    void clear();

private:

    enum class PipelineFlag {
        DepthTest  = (1 << 0),
        DepthWrite = (1 << 1),
        Blend      = (1 << 2),
        CullBack   = (1 << 3),
        OITAccum   = (1 << 4),
        OITReveal  = (1 << 5),
        DepthBias  = (1 << 6),  ///< Positive depth bias for mesh faces under wireframe.
        NegativeDepthBias  = (1 << 7),  ///< Negative depth bias for wireframe over mesh faces.
        DepthOnly  = (1 << 8),  ///< Depth-only render pass (zero color attachments).
        OITDepthPrime = (1 << 9), ///< OIT depth pre-prime: write opaque depths, suppress all color writes.
    };
    Q_DECLARE_FLAGS(PipelineFlags, PipelineFlag)

    static ShaderVariant toOITAccumVariant(ShaderVariant v);
    static ShaderVariant toOITRevealVariant(ShaderVariant v);

    /// Uploads face vertex data and index/sorting buffers for one draw call.
    void uploadFaceData(QRhiResourceUpdateBatch* batch, DrawCall& dc,
                        bool isPicking, bool isInteractive, const ViewProjectionParameters& projParams);

    /// Uploads wireframe line buffers for one draw call.
    void uploadWireframeData(QRhiResourceUpdateBatch* batch, DrawCall& dc);

    /// Ensures the shared VBO shader resource bindings are valid.
    QRhiShaderResourceBindings* ensureShaderResourceBindings();

    /// Ensures a graphics pipeline for the given variant and flags.
    QRhiGraphicsPipeline* ensurePipeline(QRhiRenderPassDescriptor* rpd,
                                         ShaderVariant variant, PipelineFlags flags,
                                         const DrawCall& dc);

    /// Internal draw overload that handles all rendering modes.
    void draw(QRhiCommandBuffer* cb, QRhiRenderPassDescriptor* rpd,
              bool opaqueOnly, bool transparentOnly, bool isPicking,
              bool oitAccum, bool oitReveal, bool excludedDepthOnly = false,
              bool oitDepthPrime = false);

    std::vector<DrawCall> _drawCalls;

    std::unique_ptr<QRhiBuffer> _drawParamsUBO;
    quint32 _drawParamsAlignedSize = 0;

    /// Shared VBO shader resource bindings (binding 0: SceneParams, binding 1: DrawParams dynamic).
    std::unique_ptr<QRhiShaderResourceBindings> _bindings;
};

}   // End of namespace
