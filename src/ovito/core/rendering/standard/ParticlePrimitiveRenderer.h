// SPDX-FileCopyrightText: 2026 OVITO GmbH, Germany
// SPDX-License-Identifier: GPL-3.0-only OR MIT

#pragma once

#include <ovito/core/Core.h>
#include <ovito/core/rendering/ParticlePrimitive.h>
#include <ovito/core/rendering/FrameGraph.h>
#include <ovito/core/rendering/ObjectPickingMap.h>
#include <ovito/core/viewport/ViewProjectionParameters.h>
#include "PrimitiveRenderer.h"

namespace Ovito {

/**
 * Renders ParticlePrimitive commands using QRhi.
 * Used by StandardRendererImplementation.
 *
 * Transparency rendering modes (visual pass only):
 *  - Painter's algorithm: particles are sorted back-to-front (on the CPU),
 *    then rendered with SSBO-based vertex shaders that read data in sorted order.
 *  - Weighted Blended OIT: order-independent transparency using off-screen RGBA16F accumulation
 *    and R8 reveal buffers, composited into the main pass.
 *
 * Picking passes always use storage order (fully opaque, no transparency).
 */
class OVITO_CORE_EXPORT ParticlePrimitiveRenderer : public PrimitiveRenderer
{
public:

    /// Identifies the shader variant to use.
    enum class ShaderVariant {
        // SphericalShape – NormalShading
        RaycastSphere,                    ///< Ray-cast sphere, VBO visual.
        ImposterSphereWithDepth,          ///< Imposter, circle discard, depth correction in shader.
        ImposterSphereNoDepth,            ///< Imposter, circle discard, no depth correction.

        // SphericalShape – FlatShading
        ImposterSphereFlatShading,

        // SphericalShape picking
        RaycastSpherePicking,
        ImposterSphereWithDepthPicking,
        ImposterSphereNoDepthPicking,

        // SphericalShape sorted SSBO
        RaycastSphereSorted,
        ImposterSphereWithDepthSorted,
        ImposterSphereNoDepthSorted,
        ImposterSphereFlatShadingSorted,

        // SquareCubicShape – NormalShading (cube mesh, 14-vertex triangle strip)
        CubeMesh,
        CubeMeshPicking,
        CubeMeshSorted,

        // SquareCubicShape – FlatShading (screen-aligned square billboard)
        SquareBillboard,
        SquareBillboardPicking,
        SquareBillboardSorted,

        // BoxShape – NormalShading (oriented box mesh)
        BoxOrientedMesh,
        BoxOrientedMeshPicking,
        BoxOrientedMeshSorted,

        // EllipsoidShape – NormalShading (ellipsoid raycast, 14-vertex bounding box)
        EllipsoidRaycast,
        EllipsoidRaycastPicking,
        EllipsoidRaycastSorted,

        // SuperquadricShape – NormalShading (superquadric raycast, 14-vertex bounding box)
        SuperquadricRaycast,
        SuperquadricRaycastPicking,
        SuperquadricRaycastSorted,
    };

    /// A prepared draw call for a particle primitive.
    struct DrawCall
    {
        const ParticlePrimitive* primitive;     ///< Pointer to the particle data (valid for the frame).
        AffineTransformation modelWorldTM;      ///< Model-to-world transformation.
        ShaderVariant shader;                   ///< Which shader variant to use.
        bool hasTransparency = false;           ///< Whether this draw call needs alpha blending.
        bool excludeFromOutline = false;        ///< If true, skip in the outline depth pre-pass.
        bool filteredOut = false;               ///< If true, handled by a parent renderer (e.g. ANARI); skip in main color pass but NOT in outline depth pre-pass.
        uint32_t objectId = 0;                  ///< Object ID assigned during picking pass (0 if not picking).
        uint32_t elementOffset;                 ///< Element offset for picking primitiveId.
        size_t particleCount;                   ///< Number of particles to draw.
        size_t baseParticleIndex = 0;           ///< Start index of the sub-range if the particles list has been split into chunks.
        uint32_t verticesPerInstance = 4;       ///< 4 for billboard/raycast quads, 14 for mesh triangle strips.
        FrameGraph::RenderLayerType layer = FrameGraph::SceneLayer; ///< Frame graph layer this draw call belongs to.

        /// Per-particle data buffers.
        struct {
            QRhiBuffer* positions = nullptr;
            QRhiBuffer* radii = nullptr;
            QRhiBuffer* colors = nullptr;
            QRhiBuffer* transparencies = nullptr;
            QRhiBuffer* selection = nullptr;
            QRhiBuffer* asphericalShapes = nullptr; ///< For box/ellipsoid/superquadric (vec3 per particle).
            QRhiBuffer* orientations = nullptr;     ///< For box/ellipsoid/superquadric (vec4 per particle).
            QRhiBuffer* roundness = nullptr;        ///< For superquadric (vec2 per particle).
        } bufs;

        /// For sorted draw calls:
        struct {
            QRhiBuffer* sortedIndexBuffer = nullptr; ///< uint32 SSBO with sorted particle indices.
            std::shared_ptr<QRhiShaderResourceBindings> shaderResourceBindings; ///< Resource bindings for the sorted SSBO and other per-instance buffers.
        } sorted;
    };

    /// Constructor.
    using PrimitiveRenderer::PrimitiveRenderer;

    /// Phase 1: Builds draw calls from particle primitives.
    void buildDrawCalls(const ParticlePrimitive& primitive, const FrameGraph::RenderingCommand& command,
                        bool isPickingPass, ObjectPickingMap* pickingMap,
                        FrameGraph::RenderLayerType layer = FrameGraph::SceneLayer,
                        bool filteredOut = false);

    /// Phase 2: Uploads vertex buffers and UBOs. Also computes sort keys and allocates sort buffers.
    void prepareResourceUpdates(QRhiResourceUpdateBatch* batch, const ViewProjectionParameters& projParams,
                                QSize renderSize, bool isYUpInNDC, bool isYUpInFramebuffer, bool isPicking,
                                bool isInteractive);

    /// Phase 3: Issues draw calls inside the main render pass.
    void draw(QRhiCommandBuffer* cb, QRhiRenderPassDescriptor* rpd,
              bool opaqueOnly, bool transparentOnly, bool isPicking);

    /// Draws all OverLayer particles with depth testing disabled (called from renderLayerInPass for OverLayer).
    void drawOverlay(QRhiCommandBuffer* cb, QRhiRenderPassDescriptor* rpd);

    /// Draws all HighlightLayer particles with depth testing disabled into the highlight silhouette render target.
    void drawHighlightSilhouette(QRhiCommandBuffer* cb, QRhiRenderPassDescriptor* rpd);

    /// Draws ExcludeFromOutline SceneLayer particles into the depth-only excluded-depth pre-pass target.
    void drawExclusionDepth(QRhiCommandBuffer* cb, QRhiRenderPassDescriptor* rpd);

    /// Returns true if any draw call requires transparency (used by OIT dispatch in StandardRendererImplementation).
    bool hasTransparentDrawCalls() const;

    /// Draws all transparent particles into the OIT accumulation render pass (additive blend).
    /// Called from StandardRendererImplementation::performPrePasses() after beginPass().
    void drawOITAccumPass(QRhiCommandBuffer* cb, QRhiRenderPassDescriptor* rpd);

    /// Draws all transparent particles into the OIT reveal render pass (multiplicative blend).
    /// Called from StandardRendererImplementation::performPrePasses() after beginPass().
    void drawOITRevealPass(QRhiCommandBuffer* cb, QRhiRenderPassDescriptor* rpd);

    /// Clears the draw call list for the next frame.
    void clear();

    /// Returns the estimated Metal TBDR parameter buffer bytes used by all current draw calls.
    size_t metalParamBufferUsage() const;

private:

    /// Options for graphics pipeline variants.
    enum class PipelineFlag {
        DepthTest       = (1 << 0),
        DepthWrite      = (1 << 1),
        Blend           = (1 << 2),    ///< Standard alpha blend (painter's algorithm transparent pass).
        NoTransparency  = (1 << 3),    ///< All particles have the same (zero) transparency.
        NoSelection     = (1 << 4),    ///< No selection buffer. Don't perform highlighting of selected particles.
        OITAccum        = (1 << 5),    ///< OIT accumulation pass (additive blend, new fragment shader).
        OITReveal       = (1 << 6),    ///< OIT reveal pass (multiplicative blend, new fragment shader).
        OITDepthPrime   = (1 << 7),    ///< OIT depth-only pre-pass: write opaque depths, suppress all color writes.
        SortedSSBO      = (1 << 8),    ///< Sorted SSBO rendering (no vertex input layout, sorted vertex shader).
        DepthOnly       = (1 << 9),    ///< Outline depth pre-pass: depth-only render target (zero color attachments).
    };
    Q_DECLARE_FLAGS(PipelineFlags, PipelineFlag)

    /// Determines the base shader variant for a given particle primitive.
    ShaderVariant selectShaderVariant(const ParticlePrimitive& primitive, bool isPicking) const;

    /// Returns the "sorted" version of a non-picking shader variant.
    static ShaderVariant toSortedVariant(ShaderVariant v);

    /// Ensures the standard shader resource bindings are created (for VBO-based draw calls).
    QRhiShaderResourceBindings* ensureShaderResourceBindings();

    /// Ensures a graphics pipeline is valid for the given configuration.
    QRhiGraphicsPipeline* ensurePipeline(QRhiRenderPassDescriptor* rpd, ShaderVariant variant, PipelineFlags flags, const DrawCall& dc);

    /// Uploads per-instance data buffers for a draw call (VBO-based, standard path).
    void uploadInstanceData(QRhiResourceUpdateBatch* batch, DrawCall& dc, bool isPicking);

    /// Uploads per-instance data as SSBOs for a sorted draw call.
    void uploadInstanceDataSorted(QRhiResourceUpdateBatch* batch, DrawCall& dc, bool isInteractive, const ViewProjectionParameters& projParams);

    /// Internal draw overload that also handles OIT accumulation / reveal passes.
    void draw(QRhiCommandBuffer* cb, QRhiRenderPassDescriptor* rpd,
              bool opaqueOnly, bool transparentOnly, bool isPicking,
              bool oitAccum, bool oitReveal, bool oitDepthPrime = false,
              bool overlayOnly = false, bool highlightSilhouetteOnly = false,
              bool excludedDepthOnly = false);

private:

    /// Prepared draw calls for the current frame.
    std::vector<DrawCall> _drawCalls;

    /// Per-draw parameters UBO (packed with dynamic offsets, one slot per draw call).
    std::unique_ptr<QRhiBuffer> _drawParamsUBO;

    /// The aligned size of one draw-params slot in the dynamic UBO.
    quint32 _drawParamsAlignedSize = 0;

    /// Standard resource bindings for VBO-based draw calls (binding 0: SceneParams, binding 1: DrawParams).
    std::unique_ptr<QRhiShaderResourceBindings> _bindings;
};

}   // End of namespace
