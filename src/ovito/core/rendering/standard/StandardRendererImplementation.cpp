// SPDX-FileCopyrightText: 2026 OVITO GmbH, Germany
// SPDX-License-Identifier: GPL-3.0-only OR MIT

// #define EXPORT_BUFFERS

#include <ovito/core/Core.h>
#ifdef EXPORT_BUFFERS
#include <QDir>
#include <QFile>
#include <QFileInfo>
#endif
#include <ovito/core/rendering/FrameGraph.h>
#include <ovito/core/rendering/CylinderPrimitive.h>
#include <ovito/core/rendering/ImagePrimitive.h>
#include <ovito/core/rendering/LinePrimitive.h>
#include <ovito/core/rendering/MarkerPrimitive.h>
#include <ovito/core/rendering/MeshPrimitive.h>
#include <ovito/core/rendering/ParticlePrimitive.h>
#include <ovito/core/rendering/standard/StandardRenderer.h>
#include <ovito/core/rendering/RendererService.h>
#include "StandardRendererImplementation.h"

namespace Ovito {

/******************************************************************************
* Constructor.
******************************************************************************/
StandardRendererImplementation::StandardRendererImplementation(RendererService* service, bool orderIndependentTransparency)
    : Implementation(service), _cylinderRenderer(this), _imageRenderer(this), _lineRenderer(this), _markerRenderer(this), _meshRenderer(this), _particleRenderer(this), _textBillboardRenderer(this), _orderIndependentTransparency(orderIndependentTransparency)
{
}

/******************************************************************************
* Phase 1: Called BEFORE beginPass(). Iterates the FrameGraph and builds draw calls.
******************************************************************************/
void StandardRendererImplementation::renderFrame(const FrameGraph& frameGraph, const SceneRenderer::Configuration& config,
                                                 QSize renderSize, TaskProgress& progress, bool isPickingPass,
                                                 int refinementIteration, ObjectPickingMap* pickingMap)
{
    // Clear previous frame's draw calls.
    _cylinderRenderer.clear();
    _imageRenderer.clear();
    _lineRenderer.clear();
    _markerRenderer.clear();
    _meshRenderer.clear();
    _particleRenderer.clear();
    _textBillboardRenderer.clear();

    // Open a new resource cache frame.
    _previousResourceFrame = std::exchange(_currentResourceFrame, service()->rhiResourceCache().acquireResourceFrame());

    bool isYUpInNDC = rhi()->isYUpInNDC();

    // Iterate all command groups in the frame graph.
    for(const auto& group : frameGraph.commandGroups()) {
        FrameGraph::RenderLayerType layer = group.layerType();

        for(const auto& command : group.commands()) {
            // Skip commands not relevant to the current pass.
            if(isPickingPass && command.skipInPickingPass())
                continue;
            if(!isPickingPass && command.skipInVisualPass())
                continue;

            // Apply the optional rendering command filter, if set.
            // A return value of false means the command is handled by the parent renderer
            // (ANARI/OSPRay) and must be skipped here entirely: eligible SceneLayer geometry
            // rendered by the raytracer no longer needs QRhi draw calls. The outline effect
            // derives its edges from the composited scene depth, and only ExcludeFromOutline
            // geometry feeds the excluded-depth pre-pass — raytraced excluded objects are
            // handled by the raytracer's own excluded-depth contribution, not re-rasterized.
            // This avoids duplicating data and rendering effort for the whole scene.
            bool filteredOut = false;
            if(_renderingCommandFilter && !_renderingCommandFilter(*this, group, command, isPickingPass))
                continue;

            // Determine the primitive type and dispatch to the appropriate sub-renderer.
            RenderingPrimitive* prim = command.primitive();
            if(!prim)
                continue;

            if(auto* particlePrim = dynamic_cast<ParticlePrimitive*>(prim)) {
                _particleRenderer.buildDrawCalls(*particlePrim, command, isPickingPass, pickingMap, layer, filteredOut);
            }
            else if(auto* cylPrim = dynamic_cast<CylinderPrimitive*>(prim)) {
                _cylinderRenderer.buildDrawCalls(*cylPrim, command, isPickingPass, pickingMap, layer, filteredOut);
            }
            else if(auto* meshPrim = dynamic_cast<MeshPrimitive*>(prim)) {
                _meshRenderer.buildDrawCalls(*meshPrim, command, isPickingPass, pickingMap, false, filteredOut);
            }
            else if(!filteredOut) {
                // Lines, markers, and images have no outline depth pre-pass; skip when filteredOut.
                if(auto* linePrim = dynamic_cast<LinePrimitive*>(prim)) {
                    _lineRenderer.buildDrawCalls(*linePrim, command, layer, isPickingPass, pickingMap);
                }
                else if(auto* markerPrim = dynamic_cast<MarkerPrimitive*>(prim)) {
                    _markerRenderer.buildDrawCalls(*markerPrim, command, layer, isPickingPass, pickingMap);
                }
                else if(auto* imagePrim = dynamic_cast<ImagePrimitive*>(prim)) {
                    OVITO_ASSERT(layer != FrameGraph::SceneLayer);
                    OVITO_ASSERT(!isPickingPass);
                    _imageRenderer.buildDrawCalls(*imagePrim, command, layer, renderSize, isYUpInNDC);
                }
                else if(auto* textBillboardPrim = dynamic_cast<TextBillboardPrimitive*>(prim)) {
                    OVITO_ASSERT(!isPickingPass); // Text billboards are always added with the ExcludeFromPicking flag.
                    _textBillboardRenderer.buildDrawCalls(*textBillboardPrim, command, layer);
                }
            }
        }
    }

    // For visual passes, extract the outline settings and detect HighlightLayer commands.
    // This is used later by prepareIntermediateTarget() to decide if post-processing is needed.
    // outlineSettings lives on the base SceneRenderer::Configuration so it is accessible
    // regardless of which renderer (Standard, ANARI, OSPRay) provided the configuration.
    if(!isPickingPass) {
        _currentOutlineSettings = config.outlineSettings;
        _hasHighlightLayer = std::ranges::any_of(frameGraph.commandGroups(),
            [](const FrameGraph::RenderingCommandGroup& g) { return g.layerType() == FrameGraph::HighlightLayer; });

        if(_currentOutlineSettings.enabled) {
            if(!_currentOutlineSettings.useCustomColor) {
                // Determine an appropriate outline color that contrasts well against the background clear color.
                // This is important for the default black outlines to be visible when the background is light, and white outlines to be visible when the background is dark.
                if(frameGraph.clearColor().a() == 0) {
                    // Transparent background — default to black outlines.
                    _currentOutlineSettings.customColor = Color(0, 0, 0);
                }
                else {
                    // Pick black or white for maximum contrast against the background (WCAG 2.0).
                    // https://www.w3.org/TR/WCAG20/#relativeluminancedef
                    FloatType L = FloatType(0.2126) * frameGraph.clearColor().r()
                                + FloatType(0.7152) * frameGraph.clearColor().g()
                                + FloatType(0.0722) * frameGraph.clearColor().b();
                    // Black has better contrast when (L+0.05)/0.05 > 1.05/(L+0.05), i.e. L > sqrt(0.0525)-0.05.
                    if((L + FloatType(0.05)) / FloatType(0.05) > FloatType(1.05) / (L + FloatType(0.05)))
                        _currentOutlineSettings.customColor = Color(0, 0, 0);
                    else
                        _currentOutlineSettings.customColor = Color(1, 1, 1);
                }
                _currentOutlineSettings.useCustomColor = true;
            }
        }

        // On Metal, estimate whether the TBDR parameter buffer budget will be exceeded.
        // If so, _needsMemoryBackedDepth forces the intermediate render target path, whose
        // QRhiTexture depth attachment maps to MTLStorageModePrivate (memory-backed) and allows
        // the tiler to spill geometry to system memory instead of aborting with a pink image.
        if(rhi()->backend() == QRhi::Metal) {
            /// Conservative 50% of the empirical ~2 GB Metal TBDR parameter buffer limit.
            constexpr size_t kMetalParamBufferBudget = size_t{1} << 30; // 1 GiB
            const size_t metalBytes = _particleRenderer.metalParamBufferUsage()
                                    + _cylinderRenderer.metalParamBufferUsage();
            _needsMemoryBackedDepth = (metalBytes > kMetalParamBufferBudget);
        }
        else {
            _needsMemoryBackedDepth = false;
        }
    }
    else {
        _currentOutlineSettings = {};
        _hasHighlightLayer = false;
        _needsMemoryBackedDepth = false;
    }
}

/******************************************************************************
* Performs the QRhi draw calls for rendering a MeshPrimitive in wireframe mode (lines).
******************************************************************************/
void StandardRendererImplementation::renderMeshWireframeLines(const MeshPrimitive& primitive, const FrameGraph::RenderingCommand& command)
{
    const bool isPickingPass = false; // Wireframe lines are never rendered in the picking pass.
    const bool renderOnlyWireframe = true;
    _meshRenderer.buildDrawCalls(primitive, command, isPickingPass, nullptr, renderOnlyWireframe);
}

/******************************************************************************
* Phase 2: Called BEFORE beginPass(), after renderFrame(). Uploads GPU resources.
******************************************************************************/
void StandardRendererImplementation::prepareResourceUpdates(QRhiRenderTarget* renderTarget,
                                                            QRhiResourceUpdateBatch* batch,
                                                            QSize renderSize, bool isPickingPass,
                                                            const FrameGraph& frameGraph)
{
    bool isYUpInNDC = rhi()->isYUpInNDC();
    bool isYUpInFramebuffer = rhi()->isYUpInFramebuffer();

    // Must match the std140 'SceneParams' UBO layout in 'scene_params.glsl'.
    struct SceneParamsData {
        Matrix4F viewMatrix;
        Matrix4F projectionMatrix;
        Matrix4F inverseProjectionMatrix;
        Matrix4F clipSpaceCorrMatrix;
        Matrix4F clipProjectionMatrix; // = clipSpaceCorrMatrix * projectionMatrix (precomputed)
        float viewportWidth;
        float viewportHeight;
        int isYUpInNDC;
        int isYUpInFramebuffer;
        int isPerspective;
        float _pad[3]; // Padding to ensure 16-byte alignment of the struct size.
    };
    static_assert(sizeof(SceneParamsData) == 352);

    // Create the scene params UBO if it doesn't exist yet.
    if(!_sceneParamsUBO) {
        _sceneParamsUBO.reset(rhi()->newBuffer(QRhiBuffer::Dynamic, QRhiBuffer::UniformBuffer, sizeof(SceneParamsData)));
        if(!_sceneParamsUBO->create()) {
            service()->reportWarning("StandardRendererImplementation: Failed to create scene params UBO.");
            _sceneParamsUBO.reset();
            return;
        }
    }

    // Upload scene params (shared across all line draw calls).
    SceneParamsData sceneParams;
    const auto& projParams = frameGraph.projectionParams();
    sceneParams.viewMatrix = Matrix4F(projParams.viewMatrix.toDataType<float>());
    sceneParams.projectionMatrix = projParams.projectionMatrix.toDataType<float>();
    sceneParams.inverseProjectionMatrix = projParams.inverseProjectionMatrix.toDataType<float>();
    sceneParams.clipSpaceCorrMatrix = rhi()->clipSpaceCorrMatrix();
    sceneParams.clipProjectionMatrix = sceneParams.clipSpaceCorrMatrix * sceneParams.projectionMatrix;
    sceneParams.viewportWidth = static_cast<float>(renderSize.width());
    sceneParams.viewportHeight = static_cast<float>(renderSize.height());
    sceneParams.isYUpInNDC = isYUpInNDC ? 1 : 0;
    sceneParams.isYUpInFramebuffer = isYUpInFramebuffer ? 1 : 0;
    sceneParams.isPerspective = projParams.isPerspective ? 1 : 0;
    sceneParams._pad[0] = sceneParams._pad[1] = sceneParams._pad[2] = 0;
    batch->updateDynamicBuffer(_sceneParamsUBO.get(), 0, sizeof(SceneParamsData), &sceneParams);

    // Cache projection parameters for use in runPostProcess() (depth linearization).
    _cachedZNear       = static_cast<float>(projParams.znear);
    _cachedZFar        = static_cast<float>(projParams.zfar);
    _cachedIsPerspective = projParams.isPerspective;

    _imageRenderer.prepareResourceUpdates(batch, renderSize, isYUpInNDC);
    _lineRenderer.prepareResourceUpdates(batch, projParams, renderSize, isYUpInNDC, isYUpInFramebuffer, isPickingPass);
    _markerRenderer.prepareResourceUpdates(batch, projParams, renderSize, isYUpInNDC, isYUpInFramebuffer, isPickingPass);
    _meshRenderer.prepareResourceUpdates(batch, projParams, renderSize, isYUpInNDC, isYUpInFramebuffer, isPickingPass, frameGraph.isInteractive());
    _particleRenderer.prepareResourceUpdates(batch, projParams, renderSize, isYUpInNDC, isYUpInFramebuffer, isPickingPass, frameGraph.isInteractive());
    _cylinderRenderer.prepareResourceUpdates(batch, projParams, renderSize, isYUpInNDC, isYUpInFramebuffer, isPickingPass, frameGraph.isInteractive());
    _textBillboardRenderer.prepareResourceUpdates(batch, projParams);

    // Ensure the OIT offscreen render targets are ready.
    if(orderIndependentTransparency() && !isPickingPass) {
        ensureOITResources(renderSize);
    }
}

/******************************************************************************
* Phase 2b: Called AFTER prepareResourceUpdates() but BEFORE beginPass().
* Dispatches GPU sort compute passes (painter's algorithm) or OIT accumulation passes.
******************************************************************************/
void StandardRendererImplementation::performPrePasses(QRhiCommandBuffer* cb, QSize renderSize,
                                                      bool isPickingPass, QRhiResourceUpdateBatch*& pendingBatch)
{
    if(isPickingPass)
        return;

    if(orderIndependentTransparency()) {
        // OIT mode: dispatch the off-screen accumulation and reveal passes.
        const bool hasTransparent = _particleRenderer.hasTransparentDrawCalls()
                                 || _cylinderRenderer.hasTransparentDrawCalls()
                                 || _meshRenderer.hasTransparentDrawCalls();
        if(hasTransparent && _oitState.accumTarget && _oitState.revealTarget) {
            // Accumulation pass: additive blend. The pendingBatch is committed here
            // (passed to beginPass) so all uploaded data is available on the GPU.
            cb->beginPass(_oitState.accumTarget.get(), QColor(0, 0, 0, 0), { 1.0f, 0 }, pendingBatch);
            pendingBatch = nullptr;
            cb->setViewport(QRhiViewport(0, 0, float(_oitState.size.width()), float(_oitState.size.height())));
            cb->setScissor(QRhiScissor(0, 0, _oitState.size.width(), _oitState.size.height()));
            _particleRenderer.drawOITAccumPass(cb, _oitState.accumRpd.get());
            _cylinderRenderer.drawOITAccumPass(cb, _oitState.accumRpd.get());
            _meshRenderer.drawOITAccumPass(cb, _oitState.accumRpd.get());
            cb->endPass();

            // Reveal pass: multiplicative blend.
            cb->beginPass(_oitState.revealTarget.get(), QColor(255, 255, 255, 255), { 1.0f, 0 });
            cb->setViewport(QRhiViewport(0, 0, float(_oitState.size.width()), float(_oitState.size.height())));
            cb->setScissor(QRhiScissor(0, 0, _oitState.size.width(), _oitState.size.height()));
            _particleRenderer.drawOITRevealPass(cb, _oitState.revealRpd.get());
            _cylinderRenderer.drawOITRevealPass(cb, _oitState.revealRpd.get());
            _meshRenderer.drawOITRevealPass(cb, _oitState.revealRpd.get());
            cb->endPass();
        }
    }

    // Highlight silhouette pre-pass: render HighlightLayer geometry without depth testing
    // into highlightColorTexture. The post-process dilates this texture to produce the
    // silhouette outline ring drawn unconditionally on top of the scene.
    if(_hasHighlightLayer && _intermediateState.highlightTarget) {
        // Clear to transparent black, then render HighlightLayer geometry depth-test-disabled.
        // pendingBatch may be null here (already committed by OIT passes) — that is fine.
        cb->beginPass(_intermediateState.highlightTarget.get(), QColor(0, 0, 0, 0), { 1.0f, 0 }, pendingBatch);
        pendingBatch = nullptr;
        cb->setViewport(QRhiViewport(0, 0, float(_intermediateState.size.width()), float(_intermediateState.size.height())));
        cb->setScissor(QRhiScissor(0, 0, _intermediateState.size.width(), _intermediateState.size.height()));
        _particleRenderer.drawHighlightSilhouette(cb, _intermediateState.highlightRpd.get());
        _cylinderRenderer.drawHighlightSilhouette(cb, _intermediateState.highlightRpd.get());
        cb->endPass();
    }
    else if(_intermediateState.highlightTarget) {
        // No selection active, but the texture was created (outline is enabled).
        // D3D12 forbids sampling a CREATE_NOT_ZEROED resource that has never been cleared,
        // so issue a clear-only pass to initialize it to transparent black.
        cb->beginPass(_intermediateState.highlightTarget.get(), QColor(0, 0, 0, 0), { 1.0f, 0 }, pendingBatch);
        pendingBatch = nullptr;
        cb->endPass();
    }

    // Excluded-depth pre-pass: render ONLY ExcludeFromOutline SceneLayer geometry (e.g. the
    // simulation cell, gizmos, opaque slicing planes) into a depth-only texture. The outline
    // init stage compares this against the composited scene depth to flag pixels where an
    // excluded object is the frontmost surface. Raytraced excluded objects are not re-rasterized
    // here; the raytracer adds its own excluded-depth contribution into the same texture.
    if(_currentOutlineSettings.enabled && _intermediateState.excludedDepthTarget) {
        // Clear depth to 1.0 (far plane = no excluded geometry). Color clear value is ignored for depth-only targets.
        cb->beginPass(_intermediateState.excludedDepthTarget.get(), QColor(), { 1.0f, 0 }, pendingBatch);
        pendingBatch = nullptr;
        cb->setViewport(QRhiViewport(0, 0, float(_intermediateState.size.width()), float(_intermediateState.size.height())));
        cb->setScissor(QRhiScissor(0, 0, _intermediateState.size.width(), _intermediateState.size.height()));
        _particleRenderer.drawExclusionDepth(cb, _intermediateState.excludedDepthRpd.get());
        _cylinderRenderer.drawExclusionDepth(cb, _intermediateState.excludedDepthRpd.get());
        _meshRenderer.drawExclusionDepth(cb, _intermediateState.excludedDepthRpd.get());
        _lineRenderer.drawExclusionDepth(cb, _intermediateState.excludedDepthRpd.get());
        // Let a parent raytracer renderer add its own excluded objects' depth in the same pass,
        // combined with the QRhi excluded geometry via the depth test.
        if(_excludedDepthExtraDraw)
            _excludedDepthExtraDraw(cb, _intermediateState.excludedDepthRpd.get());
        cb->endPass();
    }
    else if(_intermediateState.excludedDepthTarget) {
        // Outline is disabled but the texture was created (highlight is active).
        // Clear it so D3D12 doesn't read an uninitialized CREATE_NOT_ZEROED resource.
        cb->beginPass(_intermediateState.excludedDepthTarget.get(), QColor(), { 1.0f, 0 }, pendingBatch);
        pendingBatch = nullptr;
        cb->endPass();
    }

    // The outline compute pipeline is dispatched later, in preparePostProcess(), because it
    // needs the full scene depth (depthTexture), which is only populated by the main scene
    // render pass that runs after performPrePasses().
}

/******************************************************************************
* Phase 3: Called INSIDE the render pass. Issues draw calls.
******************************************************************************/
void StandardRendererImplementation::compositeInPass(QRhiCommandBuffer* cb, QRhiRenderTarget* renderTarget, bool isPickingPass, bool skipOverLayer)
{
    renderLayerInPass(FrameGraph::UnderLayer, cb, renderTarget, isPickingPass);
    renderLayerInPass(FrameGraph::SceneLayer, cb, renderTarget, isPickingPass);
    if(!skipOverLayer)
        renderLayerInPass(FrameGraph::OverLayer, cb, renderTarget, isPickingPass);
}

/******************************************************************************
* Renders only the OverLayer commands into the currently active render pass.
******************************************************************************/
void StandardRendererImplementation::renderOverLayerOnly(QRhiCommandBuffer* cb, QRhiRenderTarget* target)
{
    renderLayerInPass(FrameGraph::OverLayer, cb, target, /*isPickingPass=*/false);
}

/******************************************************************************
* Phase 2.5: Returns an intermediate color+depth render target when outline or
* highlight effects are active, so the post-process can composite into the
* final target afterwards.
******************************************************************************/
QRhiRenderTarget* StandardRendererImplementation::prepareIntermediateTarget(
    const QRhiRenderPassDescriptor* finalRpd, QSize size)
{
    // Activate the intermediate path when:
    //  - outline or highlight effects are active, OR
    //  - the Metal TBDR parameter buffer budget would be exceeded (_needsMemoryBackedDepth).
    // Fallback renderers (used inside ANARI/OSPRay) are not the primary renderer and do not
    // have a StandardRenderer::Configuration, so _currentOutlineSettings remains disabled.
    if(!_currentOutlineSettings.enabled && !_hasHighlightLayer && !_needsMemoryBackedDepth)
        return nullptr;

    if(!ensureIntermediateResources(finalRpd, size))
        return nullptr;

    return _intermediateState.target.get();
}

/******************************************************************************
* Creates or resizes the intermediate color+depth render target and the
* post-process pipeline resources. Called from prepareIntermediateTarget().
******************************************************************************/
bool StandardRendererImplementation::ensureIntermediateResources(
    const QRhiRenderPassDescriptor* /*finalRpd*/, QSize size)
{
    const bool needsRebuild = (_intermediateState.size != size || !_intermediateState.target);

    if(needsRebuild) {
        // Destroy all size-dependent resources before (re-)creating them.
        _intermediateState.outlineBindings.reset();
        _intermediateState.outlinesRhi.reset();
        _intermediateState.excludedDepthTarget.reset();
        _intermediateState.excludedDepthRpd.reset();
        _intermediateState.excludedDepthTexture.reset();
        _intermediateState.highlightTarget.reset();
        _intermediateState.highlightRpd.reset();
        _intermediateState.highlightDepthTexture.reset();
        _intermediateState.highlightColorTexture.reset();
        _intermediateState.target.reset();
        _intermediateState.rpd.reset();
        _intermediateState.colorTexture.reset();
        _intermediateState.depthTexture.reset();

        // Create the RGBA8 scene color texture.
        _intermediateState.colorTexture.reset(rhi()->newTexture(
            QRhiTexture::RGBA8, size, 1, QRhiTexture::RenderTarget));
        if(!_intermediateState.colorTexture->create()) {
            service()->reportWarning(QStringLiteral("StandardRendererImplementation: Failed to create intermediate color texture."));
            _intermediateState.colorTexture.reset();
            return false;
        }

        // Create the D32F scene depth texture.
        // Qt's Vulkan backend automatically adds VK_IMAGE_USAGE_SAMPLED_BIT alongside
        // VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT, so this texture is sampleable in shaders.
        _intermediateState.depthTexture.reset(rhi()->newTexture(
            QRhiTexture::D32F, size, 1, QRhiTexture::RenderTarget));
        if(!_intermediateState.depthTexture->create()) {
            service()->reportWarning(QStringLiteral("StandardRendererImplementation: Failed to create intermediate depth texture."));
            _intermediateState.colorTexture.reset();
            _intermediateState.depthTexture.reset();
            return false;
        }

        // Create the main scene render target (RGBA8 color + D32F depth).
        {
            QRhiTextureRenderTargetDescription desc;
            desc.setColorAttachments({ QRhiColorAttachment(_intermediateState.colorTexture.get()) });
            desc.setDepthTexture(_intermediateState.depthTexture.get());
            _intermediateState.target.reset(rhi()->newTextureRenderTarget(desc));
            _intermediateState.rpd.reset(_intermediateState.target->newCompatibleRenderPassDescriptor());
            _intermediateState.target->setRenderPassDescriptor(_intermediateState.rpd.get());
            if(!_intermediateState.target->create()) {
                service()->reportWarning(QStringLiteral("StandardRendererImplementation: Failed to create intermediate render target."));
                _intermediateState.colorTexture.reset();
                _intermediateState.depthTexture.reset();
                _intermediateState.target.reset();
                _intermediateState.rpd.reset();
                return false;
            }
        }

        // Create the RGBA8 highlight silhouette texture (rendered depth-test-disabled).
        _intermediateState.highlightColorTexture.reset(rhi()->newTexture(
            QRhiTexture::RGBA8, size, 1, QRhiTexture::RenderTarget));
        if(!_intermediateState.highlightColorTexture->create()) {
            service()->reportWarning(QStringLiteral("StandardRendererImplementation: Failed to create highlight silhouette texture."));
            _intermediateState.highlightColorTexture.reset();
            return false;
        }

        // Create a D32F depth texture for the highlight pre-pass.
        // Although depth testing is disabled, some particle shaders (raycast sphere,
        // imposter-with-depth, ellipsoid, superquadric) write gl_FragDepth.
        // Metal rejects a pipeline whose fragment shader writes depth when the render
        // target has no depth attachment, so we must provide one.
        _intermediateState.highlightDepthTexture.reset(rhi()->newTexture(
            QRhiTexture::D32F, size, 1, QRhiTexture::RenderTarget));
        if(!_intermediateState.highlightDepthTexture->create()) {
            service()->reportWarning(QStringLiteral("StandardRendererImplementation: Failed to create highlight silhouette depth texture."));
            _intermediateState.highlightColorTexture.reset();
            _intermediateState.highlightDepthTexture.reset();
            return false;
        }

        // Create the highlight silhouette render target (color + depth).
        {
            QRhiTextureRenderTargetDescription desc;
            desc.setColorAttachments({ QRhiColorAttachment(_intermediateState.highlightColorTexture.get()) });
            desc.setDepthTexture(_intermediateState.highlightDepthTexture.get());
            _intermediateState.highlightTarget.reset(rhi()->newTextureRenderTarget(desc));
            _intermediateState.highlightRpd.reset(_intermediateState.highlightTarget->newCompatibleRenderPassDescriptor());
            _intermediateState.highlightTarget->setRenderPassDescriptor(_intermediateState.highlightRpd.get());
            if(!_intermediateState.highlightTarget->create()) {
                service()->reportWarning(QStringLiteral("StandardRendererImplementation: Failed to create highlight silhouette render target."));
                _intermediateState.highlightColorTexture.reset();
                _intermediateState.highlightDepthTexture.reset();
                _intermediateState.highlightTarget.reset();
                _intermediateState.highlightRpd.reset();
                return false;
            }
        }

        // Create the D32F excluded-depth texture (depth of ExcludeFromOutline SceneLayer geometry only).
        // Always created (the highlight-only path clears it); sampled by the outline init stage.
        _intermediateState.excludedDepthTexture.reset(rhi()->newTexture(
            QRhiTexture::D32F, size, 1, QRhiTexture::RenderTarget));
        if(!_intermediateState.excludedDepthTexture->create()) {
            service()->reportWarning(QStringLiteral("StandardRendererImplementation: Failed to create excluded-depth texture."));
            _intermediateState.excludedDepthTexture.reset();
            return false;
        }

        // Create the depth-only render target for the excluded-depth pre-pass.
        {
            QRhiTextureRenderTargetDescription desc;
            desc.setDepthTexture(_intermediateState.excludedDepthTexture.get());
            _intermediateState.excludedDepthTarget.reset(rhi()->newTextureRenderTarget(desc));
            _intermediateState.excludedDepthRpd.reset(_intermediateState.excludedDepthTarget->newCompatibleRenderPassDescriptor());
            _intermediateState.excludedDepthTarget->setRenderPassDescriptor(_intermediateState.excludedDepthRpd.get());
            if(!_intermediateState.excludedDepthTarget->create()) {
                service()->reportWarning(QStringLiteral("StandardRendererImplementation: Failed to create excluded-depth render target."));
                _intermediateState.excludedDepthTexture.reset();
                _intermediateState.excludedDepthTarget.reset();
                _intermediateState.excludedDepthRpd.reset();
                return false;
            }
        }

        // Initialise the compute outline pipeline if supported and outlines are enabled.
        // initialize() takes (composited scene depth, excluded-only depth).
        if(_currentOutlineSettings.enabled && rhi()->isFeatureSupported(QRhi::Compute)) {
            _intermediateState.outlinesRhi = std::make_unique<OutlinesRhi>();
            if(!_intermediateState.outlinesRhi->initialize(rhi(), size.width(), size.height(),
                                                            _intermediateState.depthTexture.get(),
                                                            _intermediateState.excludedDepthTexture.get()))
                _intermediateState.outlinesRhi.reset();
        }

        _intermediateState.size = size;
    }

    // Create/resize the PostProcessParams UBO if needed (96 bytes, std140).
    if(!_intermediateState.postProcessParamsUBO) {
        _intermediateState.postProcessParamsUBO.reset(rhi()->newBuffer(
            QRhiBuffer::Dynamic, QRhiBuffer::UniformBuffer, 96));
        if(!_intermediateState.postProcessParamsUBO->create()) {
            service()->reportWarning(QStringLiteral("StandardRendererImplementation: Failed to create post-process params UBO."));
            _intermediateState.postProcessParamsUBO.reset();
            return false;
        }
    }

    // Create the nearest sampler if it doesn't exist yet.
    if(!_intermediateState.sampler) {
        _intermediateState.sampler.reset(rhi()->newSampler(
            QRhiSampler::Nearest, QRhiSampler::Nearest, QRhiSampler::None,
            QRhiSampler::ClampToEdge, QRhiSampler::ClampToEdge));
        if(!_intermediateState.sampler->create()) {
            service()->reportWarning(QStringLiteral("StandardRendererImplementation: Failed to create post-process sampler."));
            _intermediateState.sampler.reset();
            return false;
        }
    }

    // Create or re-create the outline/highlight shader resource bindings.
    if(!_intermediateState.outlineBindings) {
        _intermediateState.outlineBindings.reset(rhi()->newShaderResourceBindings());
        _intermediateState.outlineBindings->setBindings({
            // Binding 0 (vertex+fragment): PostProcessParams UBO.
            QRhiShaderResourceBinding::uniformBuffer(
                0, QRhiShaderResourceBinding::VertexStage | QRhiShaderResourceBinding::FragmentStage,
                _intermediateState.postProcessParamsUBO.get()),
            // Binding 1 (fragment): intermediate scene color.
            QRhiShaderResourceBinding::sampledTexture(
                1, QRhiShaderResourceBinding::FragmentStage,
                _intermediateState.colorTexture.get(), _intermediateState.sampler.get()),
            // Binding 2 (fragment): highlight silhouette (RGBA8, rendered depth-test-disabled).
            QRhiShaderResourceBinding::sampledTexture(
                2, QRhiShaderResourceBinding::FragmentStage,
                _intermediateState.highlightColorTexture.get(), _intermediateState.sampler.get()),
            // Binding 3 (fragment): premultiplied RGBA8 outline layer from OutlinesRhi compute pipeline.
            // Falls back to excludedDepthTexture (D32F) when compute is unavailable; the shader checks
            // outlineEnabled before sampling, so the format mismatch is never observed.
            QRhiShaderResourceBinding::sampledTexture(
                3, QRhiShaderResourceBinding::FragmentStage,
                _intermediateState.outlinesRhi
                    ? _intermediateState.outlinesRhi->outlineTexture()
                    : _intermediateState.excludedDepthTexture.get(),
                _intermediateState.sampler.get()),
        });
        if(!_intermediateState.outlineBindings->create()) {
            service()->reportWarning(QStringLiteral("StandardRendererImplementation: Failed to create outline/highlight post-process bindings."));
            _intermediateState.outlineBindings.reset();
            return false;
        }
    }

    return true;
}

#ifdef EXPORT_BUFFERS
/******************************************************************************
* Writes a 2-D float32 array to a NumPy v1.0 .npy file.
******************************************************************************/
static void writeNpyFloat32(const QString& path, const QByteArray& data, QSize size)
{
    QDir().mkpath(QFileInfo(path).absolutePath());
    QFile f(path);
    if(!f.open(QIODevice::WriteOnly))
        return;

    // Array descriptor: dtype '<f4' (little-endian float32), row-major, shape (H, W).
    QByteArray dict = QStringLiteral(
        "{'descr': '<f4', 'fortran_order': False, 'shape': (%1, %2), }")
        .arg(size.height()).arg(size.width()).toUtf8();

    // Pad with spaces + newline so that (10 + dict.size()) % 64 == 0.
    // Preamble: '\x93NUMPY'(6) + version(2) + header_len field(2) = 10 bytes.
    while((10 + dict.size() + 1) % 64 != 0)
        dict += ' ';
    dict += '\n';

    const quint16 headerLen = static_cast<quint16>(dict.size());
    f.write("\x93NUMPY\x01\x00", 8);
    f.write(reinterpret_cast<const char*>(&headerLen), 2);
    f.write(dict);
    f.write(data);
}

/******************************************************************************
* Debug helper: reads back excludedDepthTexture and depthTexture from the GPU
* and writes them as float32 NumPy arrays to %TEMP%/ovito_depth_dump/.
* Runs synchronously (rhi()->finish()) and only fires once per process.
******************************************************************************/
void StandardRendererImplementation::dumpDepthTexturesToNpy(QRhiCommandBuffer* cb)
{
    if(!_intermediateState.excludedDepthTexture || !_intermediateState.depthTexture)
        return;

    const QString outDir = QDir::tempPath() + QStringLiteral("/ovito_depth_dump");

    struct Job { QRhiReadbackResult result; QString path; };
    Job jobs[2] = {
        { {}, outDir + QStringLiteral("/outline_depth.npy") },
        { {}, outDir + QStringLiteral("/scene_depth.npy")   },
    };
    QRhiTexture* textures[2] = {
        _intermediateState.excludedDepthTexture.get(),
        _intermediateState.depthTexture.get(),
    };

    QRhiResourceUpdateBatch* batch = rhi()->nextResourceUpdateBatch();
    for(int i = 0; i < 2; ++i)
        batch->readBackTexture(QRhiReadbackDescription(textures[i]), &jobs[i].result);
    cb->resourceUpdate(batch);
    rhi()->finish();

    for(auto& job : jobs) {
        if(job.result.data.isEmpty()) {
            qWarning() << "dumpDepthTexturesToNpy: empty readback for" << job.path;
            continue;
        }
        writeNpyFloat32(job.path, job.result.data, job.result.pixelSize);
        qDebug() << "Depth dump written:" << job.path
                 << "  size" << job.result.pixelSize
                 << "  bytes" << job.result.data.size();
    }
}
#endif

/******************************************************************************
* Builds the PostProcessParams UBO upload batch before the final render pass
* begins. D3D11 requires that resourceUpdate() is called outside any active
* render pass, so we cannot do this inside runPostProcess(). The caller passes
* the returned batch to beginPass() as its resourceUpdates argument.
******************************************************************************/
QRhiResourceUpdateBatch* StandardRendererImplementation::preparePostProcess(QRhiCommandBuffer* cb)
{
    if(!_intermediateState.colorTexture || !_intermediateState.outlineBindings
       || !_intermediateState.postProcessParamsUBO)
        return nullptr;

#ifdef EXPORT_BUFFERS
    dumpDepthTexturesToNpy(cb);
#endif

    // std140 layout, 96 bytes (must be a multiple of 16 due to the embedded vec4s).
    struct PostProcessParams {
        qint32 isYUpInNDC;
        qint32 hasHighlight;
        qint32 silhouetteWidth;
        qint32 outlineEnabled;
        float  outlineR, outlineG, outlineB, outlineA;        // vec4 at offset 16
        qint32 outlineWidth;    ///< maxOutlineWidth — loop radius
        float  minDepthDiff;
        float  maxDepthDiff;
        qint32 isPerspective;
        float  nearPlane;
        float  farPlane;
        qint32 _padDepthRange;     ///< Unused. Depth read back from a depth attachment is window-space [0,1] on every backend.
        qint32 isYUpInFramebuffer; ///< 1 if Y=0 is at bottom (OpenGL), 0 if Y=0 is at top (Metal/Vulkan/D3D)
        qint32 outlineMinWidth; ///< minOutlineWidth — pen radius at minDepthDiff (== outlineWidth in uniform mode)
        qint32 _pad0, _pad1, _pad2;
        float  highlightR, highlightG, highlightB, highlightA; // vec4 at offset 80
    };
    static_assert(sizeof(PostProcessParams) == 96);
    PostProcessParams params{};
    const bool isYUpInNDC         = rhi()->isYUpInNDC();
    const bool isYUpInFramebuffer = rhi()->isYUpInFramebuffer();
    params.isYUpInNDC          = isYUpInNDC ? 1 : 0;
    params.hasHighlight         = _hasHighlightLayer ? 1 : 0;
    params.silhouetteWidth      = 4;
    params.outlineEnabled       = _currentOutlineSettings.enabled ? 1 : 0;
    params.outlineR             = static_cast<float>(_currentOutlineSettings.customColor.r());
    params.outlineG             = static_cast<float>(_currentOutlineSettings.customColor.g());
    params.outlineB             = static_cast<float>(_currentOutlineSettings.customColor.b());
    params.outlineA             = 1.0f;
    params.outlineWidth         = static_cast<qint32>(!std::isinf(_currentOutlineSettings.maxDepthDiff) ? _currentOutlineSettings.maxOutlineWidth : _currentOutlineSettings.minOutlineWidth);
    params.minDepthDiff         = static_cast<float>(_currentOutlineSettings.minDepthDiff);
    params.maxDepthDiff         = static_cast<float>(_currentOutlineSettings.maxDepthDiff); // may be INFINITY; GLSL handles IEEE 754 inf correctly
    params.isPerspective        = _cachedIsPerspective ? 1 : 0;
    params.nearPlane            = _cachedZNear;
    params.farPlane             = _cachedZFar;
    params._padDepthRange       = 0;
    params.isYUpInFramebuffer   = isYUpInFramebuffer ? 1 : 0;
    params.outlineMinWidth      = static_cast<qint32>(_currentOutlineSettings.minOutlineWidth);
    // Highlight silhouette uses a fixed red color, independent of the outline color setting.
    params.highlightR = 1.0f; params.highlightG = 0.0f; params.highlightB = 0.0f; params.highlightA = 1.0f;
    QRhiResourceUpdateBatch* batch = rhi()->nextResourceUpdateBatch();
    batch->updateDynamicBuffer(_intermediateState.postProcessParamsUBO.get(), 0, sizeof(PostProcessParams), &params);

    // Dispatch the outline compute pipeline here, outside any render pass, after the main scene
    // pass has populated depthTexture (full composited scene depth) and the excluded-depth
    // pre-pass has populated excludedDepthTexture. The init stage compares them to flag pixels
    // where an ExcludeFromOutline object is the frontmost surface (suppressed in resolve).
    if(_currentOutlineSettings.enabled && _intermediateState.outlinesRhi) {
        OutlinesRhi* outlines = _intermediateState.outlinesRhi.get();
        // Uniform-width mode (maxDepthDiff == infinity): avoid GLSL division-by-zero in getPenSize
        // by clamping to a finite range and collapsing width to minOutlineWidth.
        const bool uniformMode = std::isinf(_currentOutlineSettings.maxDepthDiff);
        outlines->setMinDepthDiff(float(_currentOutlineSettings.minDepthDiff));
        outlines->setMaxDepthDiff(uniformMode
            ? float(_currentOutlineSettings.minDepthDiff) + 1.0F
            : float(_currentOutlineSettings.maxDepthDiff));
        outlines->setMinLineWidth(float(_currentOutlineSettings.minOutlineWidth));
        outlines->setMaxLineWidth(uniformMode
            ? float(_currentOutlineSettings.minOutlineWidth)
            : float(_currentOutlineSettings.maxOutlineWidth));
        outlines->setColor(float(_currentOutlineSettings.customColor.r()),
                           float(_currentOutlineSettings.customColor.g()),
                           float(_currentOutlineSettings.customColor.b()));
        outlines->setNearPlane(_cachedZNear);
        outlines->setFarPlane(_cachedZFar);
        outlines->setIsPerspective(_cachedIsPerspective);
        // No depth-range flag is passed: the values the outline pass samples from the scene's
        // depth attachment are window-space depth, i.e. [0,1] on every QRhi backend. See the
        // linearizeDepth() comment in outlines_init.comp before adding one back.
        // Only despike when a parent raytracer's crisp single-ray depth feeds this pass.
        outlines->setDespike(_despikeOutlineDepth);
        outlines->record(cb);
    }

    return batch;
}

/******************************************************************************
* Runs the post-process pass from the intermediate textures into the final target.
* Applies highlight silhouette dilation and (when enabled) depth-aware outlines.
******************************************************************************/
void StandardRendererImplementation::runPostProcess(QRhiCommandBuffer* cb, QRhiRenderTarget* finalTarget)
{
    if(!_intermediateState.colorTexture || !_intermediateState.outlineBindings
       || !_intermediateState.postProcessParamsUBO)
        return;

    // Retrieve (or build) the outline/highlight post-process pipeline for this final-target RPD.
    struct OutlineCacheKey {
        bool operator==(const OutlineCacheKey&) const = default;
    };
    QRhiGraphicsPipeline* pipeline = service()->ensureGraphicsPipeline(
        finalTarget->renderPassDescriptor(), OutlineCacheKey{},
        [&]() -> std::unique_ptr<QRhiGraphicsPipeline>
    {
        QShader vs = service()->loadShader(QStringLiteral(
            ":/ovito/core/rendering/standard/shaders/postprocess_outline.vert.qsb"));
        QShader fs = service()->loadShader(QStringLiteral(
            ":/ovito/core/rendering/standard/shaders/postprocess_outline.frag.qsb"));
        if(!vs.isValid() || !fs.isValid())
            return {};

        std::unique_ptr<QRhiGraphicsPipeline> p(rhi()->newGraphicsPipeline());
        p->setShaderStages({
            { QRhiShaderStage::Vertex,   vs },
            { QRhiShaderStage::Fragment, fs }
        });
        p->setVertexInputLayout({});
        p->setTopology(QRhiGraphicsPipeline::Triangles);
        p->setDepthTest(false);
        p->setDepthWrite(false);
        p->setCullMode(QRhiGraphicsPipeline::None);
        p->setTargetBlends({ {} }); // Opaque output — write composited color to the final target.
        p->setShaderResourceBindings(_intermediateState.outlineBindings.get());
        p->setRenderPassDescriptor(finalTarget->renderPassDescriptor());

        if(!p->create()) {
            service()->reportWarning(QStringLiteral("StandardRendererImplementation: Failed to create outline/highlight post-process pipeline."));
            return {};
        }
        return p;
    });

    if(!pipeline)
        return;

    cb->setGraphicsPipeline(pipeline);
    cb->setShaderResources(_intermediateState.outlineBindings.get());
    cb->setVertexInput(0, 0, nullptr);
    cb->draw(3, 1, 0, 0);
}

/******************************************************************************
* Performs the QRhi draw calls for one layer of the FrameGraph.
******************************************************************************/
void StandardRendererImplementation::renderLayerInPass(const FrameGraph::RenderLayerType& layer, QRhiCommandBuffer* cb, QRhiRenderTarget* renderTarget, bool isPickingPass)
{
    QRhiRenderPassDescriptor* rpd = renderTarget->renderPassDescriptor();

    // Render image primitives (may appear only in under or over layers).
    if(layer != FrameGraph::SceneLayer) {
        _imageRenderer.draw(cb, rpd, layer);
    }

    // Render line primitives (may appear in any layer).
    _lineRenderer.draw(cb, rpd, layer, isPickingPass);

    // Render marker primitives (may appear in any layer).
    _markerRenderer.draw(cb, rpd, layer, isPickingPass);

    // Render 3D overlay primitives (particles/cylinders) with depth testing disabled.
    if(layer == FrameGraph::OverLayer && !isPickingPass) {
        _particleRenderer.drawOverlay(cb, rpd);
        _cylinderRenderer.drawOverlay(cb, rpd);
    }

    // Render 3D primitives only in the SceneLayer.
    if(layer == FrameGraph::SceneLayer) {
        if(isPickingPass) {
            // Picking: all primitives with depth test on.
            _particleRenderer.draw(cb, rpd, false, false, true);
            _cylinderRenderer.draw(cb, rpd, false, false, true);
            _meshRenderer.draw(cb, rpd, false, false, true);
        }
        else {
            // Opaque pass — depth test on, depth write on, no blend.
            _particleRenderer.draw(cb, rpd, true, false, false);
            _cylinderRenderer.draw(cb, rpd, true, false, false);
            _meshRenderer.draw(cb, rpd, true, false, false);

            if(orderIndependentTransparency()) {
                // OIT composite: draw the pre-composited OIT accumulation over the opaque scene.
                compositeOIT(cb, rpd);
            }
            else {
                // Painter's algorithm transparent pass — depth test on, depth write on, alpha blend.
                _particleRenderer.draw(cb, rpd, false, true, false);
                _cylinderRenderer.draw(cb, rpd, false, true, false);
                _meshRenderer.draw(cb, rpd, false, true, false);
            }

            // Text billboards go last in the scene layer: they are alpha-blended (sorted
            // back to front on the CPU) and depth-tested against the opaque scene without
            // writing depth. In painter's mode, transparent geometry drawn above has already
            // written depth, so it correctly occludes the labels. In OIT mode, the depth
            // buffer holds only opaque depth, so labels always appear in front of
            // OIT-composited transparent geometry - a known approximation.
            _textBillboardRenderer.draw(cb, rpd, layer);
        }
    }
}

/******************************************************************************
* Ensures OIT render targets exist and are the correct size.
******************************************************************************/
bool StandardRendererImplementation::ensureOITResources(QSize size)
{
    if(_oitState.size == size && _oitState.accumTarget && _oitState.revealTarget)
        return true;

    // (Re-)create OIT textures.
    _oitState.accumTexture.reset(rhi()->newTexture(QRhiTexture::RGBA16F, size, 1,
        QRhiTexture::RenderTarget | QRhiTexture::UsedAsTransferSource));
    if(!_oitState.accumTexture->create()) {
        service()->reportWarning("StandardRendererImplementation: Failed to create OIT accum texture.");
        _oitState.accumTexture.reset();
        return false;
    }

    _oitState.revealTexture.reset(rhi()->newTexture(QRhiTexture::RGBA8, size, 1,
        QRhiTexture::RenderTarget | QRhiTexture::UsedAsTransferSource));
    if(!_oitState.revealTexture->create()) {
        service()->reportWarning("StandardRendererImplementation: Failed to create OIT reveal texture.");
        _oitState.revealTexture.reset();
        return false;
    }

    // Shared depth texture. Must be QRhiTexture (not QRhiRenderBuffer) so that
    // PreserveDepthStencilContents works correctly on the reveal render target.
    _oitState.depthTexture.reset(rhi()->newTexture(QRhiTexture::D32F, size, 1, QRhiTexture::RenderTarget));
    if(!_oitState.depthTexture->create()) {
        service()->reportWarning("StandardRendererImplementation: Failed to create OIT depth texture.");
        _oitState.depthTexture.reset();
        return false;
    }

    // Accumulation render target.
    QRhiTextureRenderTargetDescription accumDesc;
    accumDesc.setColorAttachments({ QRhiColorAttachment(_oitState.accumTexture.get()) });
    accumDesc.setDepthTexture(_oitState.depthTexture.get());
    _oitState.accumTarget.reset(rhi()->newTextureRenderTarget(accumDesc));
    _oitState.accumRpd.reset(_oitState.accumTarget->newCompatibleRenderPassDescriptor());
    _oitState.accumTarget->setRenderPassDescriptor(_oitState.accumRpd.get());
    if(!_oitState.accumTarget->create()) {
        service()->reportWarning("StandardRendererImplementation: Failed to create OIT accum render target.");
        _oitState.accumTarget.reset();
        return false;
    }

    // Reveal render target: load the depth written by the accum pass (opaque depth prime),
    // so transparent particles behind opaque ones are correctly occluded.
    // PreserveDepthStencilContents requires QRhiTexture (not QRhiRenderBuffer) for depth.
    QRhiTextureRenderTargetDescription revealDesc;
    revealDesc.setColorAttachments({ QRhiColorAttachment(_oitState.revealTexture.get()) });
    revealDesc.setDepthTexture(_oitState.depthTexture.get());
    _oitState.revealTarget.reset(rhi()->newTextureRenderTarget(revealDesc, QRhiTextureRenderTarget::PreserveDepthStencilContents));
    _oitState.revealRpd.reset(_oitState.revealTarget->newCompatibleRenderPassDescriptor());
    _oitState.revealTarget->setRenderPassDescriptor(_oitState.revealRpd.get());
    if(!_oitState.revealTarget->create()) {
        service()->reportWarning("StandardRendererImplementation: Failed to create OIT reveal render target.");
        _oitState.revealTarget.reset();
        return false;
    }

    // Sampler for the composite shader.
    if(!_oitState.sampler) {
        _oitState.sampler.reset(rhi()->newSampler(
            QRhiSampler::Nearest, QRhiSampler::Nearest, QRhiSampler::None,
            QRhiSampler::ClampToEdge, QRhiSampler::ClampToEdge));
        if(!_oitState.sampler->create()) {
            service()->reportWarning("StandardRendererImplementation: Failed to create OIT sampler.");
            _oitState.sampler.reset();
            return false;
        }
    }

    // Composite shader resource bindings.
    // Binding 0: SceneParams UBO (vertex stage, for isYUpInNDC used in oit_compose.vert).
    // Binding 1: accumTex, Binding 2: revealTex (fragment stage).
    // Slots 1/2 are used for the textures to avoid a binding collision with the UBO at slot 0.
    _oitState.compositeBindings.reset(rhi()->newShaderResourceBindings());
    _oitState.compositeBindings->setBindings({
        QRhiShaderResourceBinding::uniformBuffer(0,
            QRhiShaderResourceBinding::VertexStage,
            _sceneParamsUBO.get()),
        QRhiShaderResourceBinding::sampledTexture(1,
            QRhiShaderResourceBinding::FragmentStage,
            _oitState.accumTexture.get(), _oitState.sampler.get()),
        QRhiShaderResourceBinding::sampledTexture(2,
            QRhiShaderResourceBinding::FragmentStage,
            _oitState.revealTexture.get(), _oitState.sampler.get()),
    });
    if(!_oitState.compositeBindings->create()) {
        service()->reportWarning("StandardRendererImplementation: Failed to create OIT composite bindings.");
        _oitState.compositeBindings.reset();
        return false;
    }

    _oitState.size = size;
    return true;
}

/******************************************************************************
* Ensures the OIT composite pipeline is valid.
******************************************************************************/
QRhiGraphicsPipeline* StandardRendererImplementation::ensureOITCompositePipeline(QRhiRenderPassDescriptor* rpd)
{
    struct OITCompositeCacheKey {
        bool operator==(const OITCompositeCacheKey&) const = default;
    };

    return service()->ensureGraphicsPipeline(rpd, OITCompositeCacheKey{}, [&]() -> std::unique_ptr<QRhiGraphicsPipeline> {
        QShader vs = service()->loadShader(QStringLiteral(":/ovito/core/rendering/standard/shaders/oit_compose.vert.qsb"));
        QShader fs = service()->loadShader(QStringLiteral(":/ovito/core/rendering/standard/shaders/oit_compose.frag.qsb"));
        if(!vs.isValid() || !fs.isValid())
            return {};

        if(!_oitState.compositeBindings)
            return {};

        std::unique_ptr<QRhiGraphicsPipeline> pipeline(rhi()->newGraphicsPipeline());
        pipeline->setShaderStages({
            { QRhiShaderStage::Vertex,   vs },
            { QRhiShaderStage::Fragment, fs }
        });
        pipeline->setVertexInputLayout({}); // No vertex inputs.
        pipeline->setTopology(QRhiGraphicsPipeline::Triangles);
        pipeline->setDepthTest(false);
        pipeline->setDepthWrite(false);
        pipeline->setCullMode(QRhiGraphicsPipeline::None);

        // Standard alpha blend to composite OIT over opaque geometry.
        QRhiGraphicsPipeline::TargetBlend tb;
        tb.enable   = true;
        tb.srcColor = QRhiGraphicsPipeline::SrcAlpha;
        tb.dstColor = QRhiGraphicsPipeline::OneMinusSrcAlpha;
        tb.srcAlpha = QRhiGraphicsPipeline::One;
        tb.dstAlpha = QRhiGraphicsPipeline::OneMinusSrcAlpha;
        pipeline->setTargetBlends({ tb });

        pipeline->setShaderResourceBindings(_oitState.compositeBindings.get());
        pipeline->setRenderPassDescriptor(rpd);

        if(!pipeline->create()) {
            service()->reportWarning("StandardRendererImplementation: Failed to create OIT composite pipeline.");
            return {};
        }
        return pipeline;
    });
}

/******************************************************************************
* Issues the OIT composite draw call (fullscreen triangle) inside the main render pass.
******************************************************************************/
void StandardRendererImplementation::compositeOIT(QRhiCommandBuffer* cb, QRhiRenderPassDescriptor* rpd)
{
    if(!_particleRenderer.hasTransparentDrawCalls() && !_cylinderRenderer.hasTransparentDrawCalls()
       && !_meshRenderer.hasTransparentDrawCalls())
        return;

    if(!_oitState.compositeBindings || !_oitState.accumTexture || !_oitState.revealTexture)
        return;

    QRhiGraphicsPipeline* pipeline = ensureOITCompositePipeline(rpd);
    if(!pipeline)
        return;

    cb->setGraphicsPipeline(pipeline);
    cb->setShaderResources(_oitState.compositeBindings.get());
    cb->setVertexInput(0, 0, nullptr);
    cb->draw(3, 1, 0, 0); // Fullscreen triangle (3 vertices, 1 instance).
}

/******************************************************************************
* Uploads an OVITO DataBuffer to the QRhi device and returns a QRhiBuffer,
* which can be used for rendering.
* This method supports uploading only a sub-range of the buffer.
******************************************************************************/
QRhiBuffer* StandardRendererImplementation::uploadDataBuffer(const ConstDataBufferPtr& dataBuffer, size_t startIndex, size_t elementCount, QRhiResourceUpdateBatch* batch, QRhiBuffer::UsageFlags usage)
{
    return createCachedBuffer(
        RendererResourceKey<struct DataBufferCache, ConstDataBufferPtr, size_t, size_t>{dataBuffer, startIndex, elementCount}, batch, usage, [&]() -> QByteArray
    {
        OVITO_ASSERT(startIndex + elementCount <= dataBuffer->size());
        if(dataBuffer->dataType() == DataBuffer::Float32 || dataBuffer->dataType() == DataBuffer::Int8) {
            // No data type conversion needed, can use a no-copy approach.
            // We can directly wrap the buffer in a QByteArray, just have to make sure the buffer's lifetime is properly managed by the cache.
            // The cache keeps its keys alive for at least the current frame, so the buffer will not be deleted while it's still in use by the GPU.
            // Create a temporary read accessor to commit any pending changes and to get a typed pointer to the data.
            RawBufferReadAccess bufferAccess(dataBuffer);
            return QByteArray::fromRawData(
                reinterpret_cast<const char*>(bufferAccess.cdata()) + startIndex * bufferAccess.stride(),
                elementCount * bufferAccess.stride());
        }
        else if(dataBuffer->dataType() == DataBuffer::Float64) {
            // Convert from double to float data type.
            BufferReadAccess<double*> bufferAccess(dataBuffer);
            QByteArray convertedData(elementCount * bufferAccess.componentCount() * sizeof(float), Qt::Uninitialized);
            float* dst = reinterpret_cast<float*>(convertedData.data());
            auto begin = bufferAccess.cbegin() + startIndex * bufferAccess.componentCount();
            auto end = begin + elementCount * bufferAccess.componentCount();
            // Copy and convert all data elements.
            for(const double* src = begin; src != end; ++src, ++dst)
                *dst = static_cast<float>(*src);
            OVITO_ASSERT(dst == reinterpret_cast<float*>(convertedData.data() + convertedData.size()));
            return convertedData;
        }
        else {
            throw Exception(QStringLiteral("Data type %1 of data buffer not compatible with QRhiBuffer.").arg(dataBuffer->dataTypeName()));
        }
    });
}

/******************************************************************************
* Creates or retrieves a 256×1 RGBA8 colorMap texture from a PseudoColorMapping.
******************************************************************************/
std::pair<QRhiTexture*, QRhiSampler*> StandardRendererImplementation::ensureColorMapTexture(const PseudoColorMapping& mapping, QRhiResourceUpdateBatch* batch)
{
    // Bin count of the discrete color map.
    // Use discrete color mapping if the bin count is > 0
    const int numDiscreteColors = mapping.discreteColorMapBinCount();

    auto& texture = rhiCache().lookup<std::unique_ptr<QRhiTexture>>(
        RendererResourceKey<struct ColorMapTextureCache, OORef<const ColorCodingGradient>, int>{mapping.gradient(), numDiscreteColors},
        [&](std::unique_ptr<QRhiTexture>& texture) {
            // Sample the color gradient to produce a row of RGBA pixel data.
            int resolution;
            QByteArray pixelData;

            if(mapping.gradient()) {
                // Set the resolution to the number of discrete colors if specified, otherwise use 256.
                resolution = (numDiscreteColors <= 0) ? 256 : std::min(256, numDiscreteColors);
                pixelData = QByteArray(resolution * 4, Qt::Uninitialized);
                auto* pixels = reinterpret_cast<uint8_t*>(pixelData.data());
                for(int x = 0; x < resolution; x++) {
                    float t = (float)x / float(resolution - 1);
                    // Use discrete color mapping if the bin count is > 0
                    t = (numDiscreteColors <= 0) ? t : DiscreteColorMap::mapValue(t, numDiscreteColors);
                    auto c = mapping.gradient()->valueToColor(t);
                    pixels[x * 4 + 0] = (uint8_t)(255 * c.r());
                    pixels[x * 4 + 1] = (uint8_t)(255 * c.g());
                    pixels[x * 4 + 2] = (uint8_t)(255 * c.b());
                    pixels[x * 4 + 3] = (uint8_t)255;
                }
            }
            else {
                resolution = 1;
                pixelData = QByteArray(resolution * 4, Qt::Uninitialized);
                auto* pixels = reinterpret_cast<uint8_t*>(pixelData.data());
                pixels[0] = 255;
                pixels[1] = 255;
                pixels[2] = 255;
                pixels[3] = 255;
            }

            texture.reset(rhi()->newTexture(QRhiTexture::RGBA8, QSize(resolution, 1), 1));
            if(!texture->create()) {
                service()->reportWarning("StandardRendererImplementation: Failed to create colorMap texture.");
                texture.reset();
                return;
            }
            QRhiTextureSubresourceUploadDescription subDesc;
            subDesc.setData(std::move(pixelData));
            batch->uploadTexture(texture.get(), QRhiTextureUploadDescription({QRhiTextureUploadEntry(0, 0, subDesc)}));
        });

    // Use nearest filter for discrete color maps to suppress color interpolation.
    if(numDiscreteColors > 0) {
        if(!_colorMapSamplerNearest) {
            _colorMapSamplerNearest.reset(rhi()->newSampler(
                QRhiSampler::Nearest, QRhiSampler::Nearest, QRhiSampler::None,
                QRhiSampler::ClampToEdge, QRhiSampler::ClampToEdge));
            if(!_colorMapSamplerNearest->create()) {
                service()->reportWarning("StandardRendererImplementation: Failed to create nearest colorMap sampler.");
                _colorMapSamplerNearest.reset();
            }
        }
        return {texture.get(), _colorMapSamplerNearest.get()};
    }
    else {
        if(!_colorMapSamplerLinear) {
            _colorMapSamplerLinear.reset(rhi()->newSampler(
                QRhiSampler::Linear, QRhiSampler::Linear, QRhiSampler::None,
                QRhiSampler::ClampToEdge, QRhiSampler::ClampToEdge));
            if(!_colorMapSamplerLinear->create()) {
                service()->reportWarning("StandardRendererImplementation: Failed to create linear colorMap sampler.");
                _colorMapSamplerLinear.reset();
            }
        }
        return {texture.get(), _colorMapSamplerLinear.get()};
    }
}

}   // End of namespace
