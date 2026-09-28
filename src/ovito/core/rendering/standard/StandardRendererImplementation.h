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
#include <ovito/core/rendering/SceneRenderer.h>
#include <ovito/core/rendering/ObjectPickingMap.h>
#include <ovito/core/rendering/OutlineSettings.h>
#include "CylinderPrimitiveRenderer.h"
#include "ImagePrimitiveRenderer.h"
#include "LinePrimitiveRenderer.h"
#include "MarkerPrimitiveRenderer.h"
#include "MeshPrimitiveRenderer.h"
#include "ParticlePrimitiveRenderer.h"
#include "TextBillboardPrimitiveRenderer.h"
#include "OutlinesRhi.h"

namespace Ovito {

/**
 * SceneRenderer::Implementation subclass that renders FrameGraph primitives
 * directly via QRhi (Metal, Vulkan, Direct3D, OpenGL).
 *
 * Orchestrates the 3-phase rendering process:
 *   Phase 1 (renderFrame):            iterate FrameGraph, build draw calls
 *   Phase 2 (prepareResourceUpdates): upload vertex/texture/UBO data
 *   Phase 3 (compositeInPass):        issue draw calls inside the render pass
 */
class OVITO_CORE_EXPORT StandardRendererImplementation : public SceneRenderer::Implementation
{
public:

    /// Constructor.
    explicit StandardRendererImplementation(RenderThread* rt, bool orderIndependentTransparency = false);

    /// Phase 1: Called BEFORE beginPass(). Iterates the FrameGraph and builds draw calls.
    void renderFrame(const FrameGraph& frameGraph, const SceneRenderer::Configuration& config, QSize renderSize, TaskProgress& progress,
                     bool isPickingPass, int refinementIteration, ObjectPickingMap* pickingMap = nullptr) override;

    /// Phase 2: Called BEFORE beginPass(), after renderFrame(). Uploads GPU resources.
    void prepareResourceUpdates(QRhiRenderTarget* renderTarget, QRhiResourceUpdateBatch* batch,
                                QSize renderSize, bool isPickingPass, const FrameGraph& frameGraph) override;

    /// Phase 2b: Called AFTER prepareResourceUpdates() but BEFORE beginPass().
    /// Dispatches GPU sort compute passes (painter's algorithm) or OIT accumulation render passes.
    void performPrePasses(QRhiCommandBuffer* cb, QSize renderSize, bool isPickingPass,
                          QRhiResourceUpdateBatch*& pendingBatch) override;

    /// Phase 2.5: Called between prepareResourceUpdates() and renderFrame() (before beginPass()).
    /// Returns the intermediate render target when outline/highlight effects are active, nullptr otherwise.
    QRhiRenderTarget* prepareIntermediateTarget(const QRhiRenderPassDescriptor* finalRpd, QSize size) override;

    /// Phase 3: Called INSIDE the render pass. Issues draw calls.
    void compositeInPass(QRhiCommandBuffer* cb, QRhiRenderTarget* renderTarget, bool isPickingPass, bool skipOverLayer = false) override;

    /// Renders only the OverLayer commands into the currently active render pass.
    /// Called inside the final target's render pass after post-processing, so OverLayer appears on top.
    void renderOverLayerOnly(QRhiCommandBuffer* cb, QRhiRenderTarget* target) override;

    /// Builds the resource-update batch for the post-process UBO; must be called before beginPass.
    QRhiResourceUpdateBatch* preparePostProcess(QRhiCommandBuffer* cb) override;

    /// Runs the post-processing pass from intermediate textures into the final target.
    void runPostProcess(QRhiCommandBuffer* cb, QRhiRenderTarget* finalTarget) override;

    /// Performs the QRhi draw calls for one layer of the FrameGraph.
    void renderLayerInPass(const FrameGraph::RenderLayerType& layer, QRhiCommandBuffer* cb, QRhiRenderTarget* renderTarget, bool isPickingPass);

    /// Returns the cache for QRhi resources created during the current frame, which allows them to be re-used in subsequent frames.
    RendererResourceCache::ResourceFrame& rhiCache() { return _currentResourceFrame; }

    /// Uploads some data to the device as a QRhiBuffer object and caches it for potential reuse in subsequent frames and/or other viewports.
    /// The function returns a pointer to the QRhiBuffer that can be used for rendering. The buffer is owned by the cache and will be automatically released when it is no longer needed.
    /// The lifetime is guaranteed for at least the current frame, but it may be reused across multiple frames if the same cache key is used.
    /// The caller must provide an arbitrary \c KeyType that is used for looking up the QRhiBuffer in the cache
    /// (shared across all renderers using the same QRhi device). The \c KeyType is typically an instantiation of the \c RendererResourceKey
    /// class template that is local to the code where the buffer is used.
    /// If the buffer does not yet exist in the cache, it is newly allocated and initialized by invoking the \c dataFunc provided by the caller,
    /// which is supposed to return a QByteArray with the application-specific bytes.
    template<typename KeyType, typename DataFunc>
    QRhiBuffer* createCachedBuffer(KeyType&& cacheKey, QRhiResourceUpdateBatch* batch, QRhiBuffer::UsageFlags usage, DataFunc&& dataFunc) {
        return rhiCache().lookup<std::unique_ptr<QRhiBuffer>>(
            std::tuple<std::decay_t<KeyType>, QRhiBuffer::UsageFlags>{std::forward<KeyType>(cacheKey), usage},
            [&](std::unique_ptr<QRhiBuffer>& buf) {
                QByteArray data = dataFunc();
                buf.reset(rhi()->newBuffer(QRhiBuffer::Immutable, usage, data.size()));
                if(!buf->create())
                    throw Exception(QStringLiteral("Failed to allocate QRhi buffer of size %1 bytes").arg(data.size()));
                batch->uploadStaticBuffer(buf.get(), 0, std::move(data));
            }).get();
    }

    /// Uploads an OVITO DataBuffer to the QRhi device and returns a QRhiBuffer, which can be used for rendering.
    /// The buffer is immutable and is owned by the cache and will be automatically released when it is no longer needed.
    QRhiBuffer* uploadDataBuffer(const ConstDataBufferPtr& dataBuffer, QRhiResourceUpdateBatch* batch, QRhiBuffer::UsageFlags usage = QRhiBuffer::VertexBuffer) {
        return uploadDataBuffer(dataBuffer, 0, dataBuffer->size(), batch, usage);
    }

    /// Uploads an OVITO DataBuffer to the QRhi device and returns a QRhiBuffer, which can be used for rendering.
    /// The buffer is immutable and is owned by the cache and will be automatically released when it is no longer needed.
    /// This method supports uploading only a sub-range of the buffer.
    QRhiBuffer* uploadDataBuffer(const ConstDataBufferPtr& dataBuffer, size_t startIndex, size_t elementCount, QRhiResourceUpdateBatch* batch, QRhiBuffer::UsageFlags usage = QRhiBuffer::VertexBuffer);

    /// Helper to create a QRhiBuffer that contains a single value repeated N times.
    /// The buffer is immutable and is owned by the cache and will be automatically released when it is no longer needed.
    template<typename T>
    QRhiBuffer* uniformValueBuffer(const T value, quint32 count, QRhiResourceUpdateBatch* batch, QRhiBuffer::UsageFlags usage = QRhiBuffer::VertexBuffer) {
        return createCachedBuffer(
            RendererResourceKey<struct UniformValueBufferCache, T, quint32>{value, count}, batch, usage, [&]() -> QByteArray
        {
            QByteArray data(count * sizeof(T), Qt::Uninitialized);
            T* dataPtr = reinterpret_cast<T*>(data.data());
            std::fill(dataPtr, dataPtr + count, value);
            return data;
        });
    }

    /// A callback function that can decide which rendering commands in the frame graph should actually be executed by the renderer.
    /// The function is called for each rendering command in the frame graph along with its command group.
    /// A return value of false means that the command should be skipped by the StandardRendererImplementation.
    /// The filter function allows a parent renderer to control which commands should be handled by this sub-renderer.
    using RenderingCommandFilter = fu2::unique_function<bool(StandardRendererImplementation&, const FrameGraph::RenderingCommandGroup&, const FrameGraph::RenderingCommand&, bool)>;

    /// Sets an optional filter function that decides which rendering commands in the frame graph should be executed by this StandardRendererImplementation.
    /// If no filter function is set, all commands are executed.
    void setRenderingCommandFilter(RenderingCommandFilter filter) { _renderingCommandFilter = std::move(filter); }

    /// Returns whether to use order-independent transparency instead of painter's algorithm when rendering transparent primitives.
    /// Note: On D3D11, storage buffers (SSBOs) in vertex shaders are not supported (UAVs are only supported in CS/FS stages),
    /// so the sorted SSBO rendering path cannot be used. OIT is forced on D3D11 regardless of the user setting.
    bool orderIndependentTransparency() const {
        return _orderIndependentTransparency || (rhi() && rhi()->backend() == QRhi::D3D11);
    }

    /// Sets whether to use order-independent transparency instead of painter's algorithm when rendering transparent primitives.
    void setOrderIndependentTransparency(bool enabled) { _orderIndependentTransparency = enabled; }

    /// Returns the QRhi scene parameters UBO (shared across all draw calls in a frame).
    QRhiBuffer* sceneParamsUBO() const { return _sceneParamsUBO.get(); }

    /// Creates (or retrieves from cache) a 256×1 RGBA8 colorMap texture from a PseudoColorMapping.
    std::pair<QRhiTexture*, QRhiSampler*> ensureColorMapTexture(const PseudoColorMapping& mapping, QRhiResourceUpdateBatch* batch);

    /// Performs the QRhi draw calls for rendering a MeshPrimitive in wireframe mode (lines).
    void renderMeshWireframeLines(const MeshPrimitive& primitive, const FrameGraph::RenderingCommand& command);

    /// A callback a parent (raytracer) renderer registers to add its own excluded-depth
    /// contribution. It is invoked from WITHIN the excluded-depth pre-pass (after the QRhi
    /// excluded geometry, before endPass), so the parent's depth combines with it via the depth
    /// test in a single render pass. The callback must only record draw calls (no begin/endPass).
    using ExcludedDepthDrawCallback = fu2::unique_function<void(QRhiCommandBuffer*, QRhiRenderPassDescriptor*)>;

    /// Registers the excluded-depth extra-draw callback (see ExcludedDepthDrawCallback).
    void setExcludedDepthExtraDraw(ExcludedDepthDrawCallback cb) { _excludedDepthExtraDraw = std::move(cb); }

    /// Enables outline depth despiking. Set by a parent raytracer (VisRTX/OSPRay) compositor,
    /// whose crisp single-ray depth can leave isolated far-plane outliers at analytic primitive
    /// seams. Left false for pure QRhi rasterization, whose depth is coherent, so the despike's
    /// neighbour fetches are skipped. See OutlinesRhi::setDespike().
    void setDespikeOutlineDepth(bool enabled) { _despikeOutlineDepth = enabled; }

private:

    /// GPU resources for the intermediate scene render target and post-process pipeline.
    /// Allocated only when outline or highlight effects are active (conditional path).
    struct IntermediateState {
        std::unique_ptr<QRhiTexture>                colorTexture;          ///< RGBA8 intermediate scene color.
        std::unique_ptr<QRhiTexture>                depthTexture;          ///< D32F intermediate scene depth (sampleable in post-process shader).
        std::unique_ptr<QRhiTextureRenderTarget>    target;                ///< Render target wrapping color + depth.
        std::unique_ptr<QRhiRenderPassDescriptor>   rpd;
        std::unique_ptr<QRhiTexture>                highlightColorTexture; ///< RGBA8: HighlightLayer geometry rendered depth-test-disabled (silhouette mask).
        std::unique_ptr<QRhiTexture>                highlightDepthTexture; ///< D32F: dummy depth for the highlight pre-pass (needed for shaders that write gl_FragDepth).
        std::unique_ptr<QRhiTextureRenderTarget>    highlightTarget;       ///< Color+depth render target for the highlight silhouette pre-pass.
        std::unique_ptr<QRhiRenderPassDescriptor>   highlightRpd;
        std::unique_ptr<QRhiTexture>                excludedDepthTexture;   ///< D32F sampleable: depth of ExcludeFromOutline SceneLayer geometry only.
        std::unique_ptr<QRhiTextureRenderTarget>    excludedDepthTarget;    ///< Depth-only render target for the excluded-depth pre-pass.
        std::unique_ptr<QRhiRenderPassDescriptor>   excludedDepthRpd;
        std::unique_ptr<OutlinesRhi>                outlinesRhi;           ///< Compute pipeline for outline generation (null when compute unsupported).
        std::unique_ptr<QRhiBuffer>                 postProcessParamsUBO;  ///< PostProcessParams UBO for the fullscreen pass.
        std::unique_ptr<QRhiSampler>                sampler;               ///< Nearest sampler shared by all post-process textures.
        std::unique_ptr<QRhiShaderResourceBindings> outlineBindings;       ///< Bindings for the outline/highlight post-process pipeline.
        QSize size;                                                        ///< Current allocation size.
    };

    /// Ensures intermediate render target resources are allocated and match \a size.
    /// Rebuilds lazily when size changes or resources are first requested.
    /// \param finalRpd  Render pass descriptor of the final target (for post-process pipeline creation).
    bool ensureIntermediateResources(const QRhiRenderPassDescriptor* finalRpd, QSize size);

    /// Debug helper: reads back excludedDepthTexture and depthTexture and writes them
    /// as float32 NumPy .npy files to %TEMP%/ovito_depth_dump/. Fires once per process.
    void dumpDepthTexturesToNpy(QRhiCommandBuffer* cb);

    /// GPU textures and render targets for WBOIT accumulation (shared across all primitive renderers).
    struct OITState {
        std::unique_ptr<QRhiTexture>                accumTexture;    ///< RGBA16F: weighted color/alpha accumulation.
        std::unique_ptr<QRhiTexture>                revealTexture;   ///< RGBA8: product of (1 - alpha_i).
        std::unique_ptr<QRhiTexture>                depthTexture;    ///< Shared D32F depth texture (QRhiTexture required for PreserveDepthStencilContents to work).
        std::unique_ptr<QRhiTextureRenderTarget>    accumTarget;
        std::unique_ptr<QRhiRenderPassDescriptor>   accumRpd;
        std::unique_ptr<QRhiTextureRenderTarget>    revealTarget;
        std::unique_ptr<QRhiRenderPassDescriptor>   revealRpd;
        std::unique_ptr<QRhiShaderResourceBindings> compositeBindings;
        std::unique_ptr<QRhiSampler>                sampler;
        QSize size;
    };

    /// Ensures OIT render targets exist at the given size. Returns false on failure.
    bool ensureOITResources(QSize size);

    /// Ensures the OIT composite pipeline is valid. Returns nullptr on failure.
    QRhiGraphicsPipeline* ensureOITCompositePipeline(QRhiRenderPassDescriptor* rpd);

    /// Issues the OIT composite draw call (fullscreen triangle) inside the main render pass.
    void compositeOIT(QRhiCommandBuffer* cb, QRhiRenderPassDescriptor* rpd);

    /// Sub-renderers for each primitive type.
    CylinderPrimitiveRenderer _cylinderRenderer;
    ImagePrimitiveRenderer    _imageRenderer;
    LinePrimitiveRenderer     _lineRenderer;
    MarkerPrimitiveRenderer   _markerRenderer;
    MeshPrimitiveRenderer     _meshRenderer;
    ParticlePrimitiveRenderer _particleRenderer;
    TextBillboardPrimitiveRenderer _textBillboardRenderer;

    /// OIT render targets (accum + reveal textures) shared across all primitive renderers.
    OITState _oitState;

    /// Intermediate render target for outline/highlight post-processing.
    IntermediateState _intermediateState;

    /// Outline settings cached from the last renderFrame() call (from the StandardRenderer::Configuration).
    OutlineSettings _currentOutlineSettings;

    /// True if the current frame has any commands in the HighlightLayer.
    bool _hasHighlightLayer = false;

    /// True when the Metal TBDR parameter buffer budget is exceeded by the current scene.
    /// Forces the intermediate render target path to obtain a memory-backed depth texture.
    bool _needsMemoryBackedDepth = false;

    /// Near/far clipping planes and projection type cached from the last prepareResourceUpdates() call.
    /// Used by runPostProcess() to linearize NDC depth to view-space distance.
    float _cachedZNear = 0.1f;
    float _cachedZFar  = 1000.0f;
    bool  _cachedIsPerspective = true;

    /// Caches QRhi resources created during the current frame so that they can be re-used in subsequent frames.
    RendererResourceCache::ResourceFrame _currentResourceFrame;

    /// After a frame was completed, this keeps cached QRhi resources alive until a new frame starts.
    RendererResourceCache::ResourceFrame _previousResourceFrame;

    /// Optional filter function that decides which rendering commands in the frame graph should be executed by this StandardRendererImplementation.
    /// If no filter function is set, all commands are executed.
    RenderingCommandFilter _renderingCommandFilter;

    /// Optional callback invoked inside the excluded-depth pre-pass to let a parent (raytracer)
    /// renderer add the depth of its own ExcludeFromOutline objects. See setExcludedDepthExtraDraw().
    ExcludedDepthDrawCallback _excludedDepthExtraDraw;

    /// Whether to despike isolated depth outliers in the outline init stage (raytracer mode only).
    bool _despikeOutlineDepth = false;

    /// Whether to use order-independent transparency instead of painter's algorithm when rendering transparent primitives.
    bool _orderIndependentTransparency = false;

    /// Scene parameters UBO (shared across all draw calls in a frame).
    std::unique_ptr<QRhiBuffer> _sceneParamsUBO;

    /// Shared linear sampler for interpolated colorMap textures.
    std::unique_ptr<QRhiSampler> _colorMapSamplerLinear;

    /// Shared nearest sampler for discrete colorMap textures.
    std::unique_ptr<QRhiSampler> _colorMapSamplerNearest;
};

}   // End of namespace
