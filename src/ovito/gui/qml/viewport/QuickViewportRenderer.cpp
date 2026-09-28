// SPDX-FileCopyrightText: 2026 OVITO GmbH, Germany
// SPDX-License-Identifier: GPL-3.0-only OR MIT

#include <ovito/core/app/UserInterface.h>
#include <ovito/core/utilities/concurrent/TaskProgress.h>
#include <ovito/core/utilities/concurrent/Promise.h>
#include "QuickViewportItem.h"
#include "QuickViewportRenderer.h"
#include "QuickViewportWindow.h"

namespace Ovito {

/******************************************************************************
* Constructor.
******************************************************************************/
QuickViewportRenderer::QuickViewportRenderer(QuickViewportItem* item) : _item(item)
{
}

/******************************************************************************
* Destructor.
******************************************************************************/
QuickViewportRenderer::~QuickViewportRenderer()
{
    // Make sure the renderer implementation and all QRhi resources it holds are released while
    // the QRhi instance of the scene graph is still alive.
    _implementation.reset();
    _frameGraph.reset();
    _rendererConfig.reset();
}

/******************************************************************************
* Returns the graphics API used by the Qt Quick scene graph.
******************************************************************************/
QRhi::Implementation QuickViewportRenderer::graphicsApi() const
{
    QRhi* rhi = QQuickRhiItemRenderer::rhi();
    return rhi ? rhi->backend() : QRhi::Null;
}

/******************************************************************************
* Loads a compiled .qsb shader from the Qt resource system.
******************************************************************************/
QShader QuickViewportRenderer::loadShader(const QString& resourcePath)
{
    QFile file(resourcePath);
    if(!file.open(QIODevice::ReadOnly)) {
        reportWarning(QObject::tr("Could not open shader resource: %1").arg(resourcePath));
        return {};
    }
    return QShader::fromSerialized(file.readAll());
}

/******************************************************************************
* Records a non-fatal warning encountered during rendering.
******************************************************************************/
void QuickViewportRenderer::reportWarning(const QString& message)
{
    _warnings.push_back(message);
    qWarning() << message;
}

/******************************************************************************
* Is called when the GPU resources of the item need to be (re)created.
******************************************************************************/
void QuickViewportRenderer::initialize(QRhiCommandBuffer* cb)
{
    Q_UNUSED(cb);

    // The color buffer or its sample count changed, which means the render pass descriptor of the
    // item is not compatible with the previously created pipelines anymore. Drop them.
    _implementation.reset();
    discardCachedResources();
}

/******************************************************************************
* Is called while the GUI thread is blocked, right before rendering.
******************************************************************************/
void QuickViewportRenderer::synchronize(QQuickRhiItem* item)
{
    OVITO_ASSERT(dynamic_cast<QuickViewportItem*>(item) != nullptr);

    // Pick up the frame graph the GUI thread has generated in the meantime. If there is none,
    // the previously rendered frame graph is rendered again (e.g. after a resize).
    if(auto* quickItem = static_cast<QuickViewportItem*>(item))
        quickItem->takePendingFrame(_frameGraph, _rendererConfig);
}

/******************************************************************************
* Is called before Qt Quick records its main render pass.
******************************************************************************/
void QuickViewportRenderer::render(QRhiCommandBuffer* cb)
{
    QRhiRenderTarget* renderTarget = this->renderTarget();
    if(!cb || !renderTarget)
        return;

    if(!_frameGraph || !_rendererConfig) {
        // Nothing to display yet. Clear the item's color buffer with the viewport background color.
        cb->beginPass(renderTarget, QColor(24, 24, 24), { 1.0f, 0 });
        cb->endPass();
        return;
    }

    bool succeeded = false;
    try {
        // Establish a task context for the rendering work, exactly as RenderThread does it for its
        // render passes. OVITO's rendering code expects an active task and a UserInterface.
        std::shared_ptr<UserInterface> ui;
        if(_item) {
            if(QuickViewportWindow* viewportWindow = _item->viewportWindow()) {
                if(viewportWindow->hasUserInterface())
                    ui = viewportWindow->ui().shared_from_this();
            }
        }
        Promise<void> promise = Promise<void>::create();
        promise.task()->setIsInteractive();
        if(ui)
            promise.task()->setUserInterface(std::move(ui));
        Task::Scope taskScope(promise.task());

        renderFrameGraph(cb, *_frameGraph, *_rendererConfig);
        succeeded = true;
    }
    catch(const Exception& ex) {
        qWarning() << "QuickViewportRenderer: exception during viewport rendering:" << ex.message();
    }
    catch(const std::exception& ex) {
        qWarning() << "QuickViewportRenderer: exception during viewport rendering:" << ex.what();
    }

    if(succeeded && _item)
        Q_EMIT _item->frameRendered();
}

/******************************************************************************
* Renders the given frame graph into the item's texture render target.
******************************************************************************/
void QuickViewportRenderer::renderFrameGraph(QRhiCommandBuffer* cb, FrameGraph& frameGraph, SceneRenderer::Configuration& rendererConfig)
{
    QRhiRenderTarget* renderTarget = this->renderTarget();
    OVITO_ASSERT(renderTarget);
    OVITO_ASSERT(cb);

    _warnings.clear();

    // Preprocess the frame graph (replace text primitives with images, compute wireframe line widths).
    frameGraph.finalizeForRendering();

    // Create or update the renderer implementation that executes the frame graph.
    _implementation = rendererConfig.createImplementationForVisual(this, std::move(_implementation));
    SceneRenderer::Implementation* implementation = _implementation.get();
    if(!implementation)
        throw Exception(QObject::tr("The scene renderer did not provide a rendering implementation."));

    const QSize pixelSize = renderTarget->pixelSize();

    // Phase 1: iterate the frame graph and build the draw calls.
    implementation->renderFrame(frameGraph, rendererConfig, pixelSize, TaskProgress::Ignore, /*isPickingPass=*/false, /*refinementIteration=*/0);

    // Check whether the renderer wants to render into an intermediate target for post-processing.
    QRhiRenderTarget* sceneTarget = renderTarget;
    if(QRhiRenderTarget* intermediate = implementation->prepareIntermediateTarget(renderTarget->renderPassDescriptor(), pixelSize))
        sceneTarget = intermediate;
    const bool useIntermediate = (sceneTarget != renderTarget);

    // Phase 2: upload vertex, texture and uniform buffer data. This must happen before beginPass().
    QRhiResourceUpdateBatch* resourceUpdates = rhi()->nextResourceUpdateBatch();
    implementation->prepareResourceUpdates(sceneTarget, resourceUpdates, pixelSize, /*isPickingPass=*/false, frameGraph);

    // Renderer-specific pre-passes (e.g. GPU depth sorting, OIT accumulation).
    implementation->performPrePasses(cb, pixelSize, /*isPickingPass=*/false, resourceUpdates);

    const QColor clearColor = static_cast<QColor>(frameGraph.clearColor());

    // Phase 3: record the main scene render pass.
    cb->beginPass(sceneTarget, clearColor, { 1.0f, 0 }, resourceUpdates);
    cb->setViewport(QRhiViewport(0, 0, float(pixelSize.width()), float(pixelSize.height())));
    // Workaround for Qt's Vulkan backend: also set the scissor rect to the full viewport size,
    // because setViewport() won't do it if no graphics pipeline is currently bound.
    cb->setScissor(QRhiScissor(0, 0, pixelSize.width(), pixelSize.height()));
    implementation->compositeInPass(cb, sceneTarget, /*isPickingPass=*/false, /*skipOverLayer=*/useIntermediate);
    cb->endPass();

    // Run the post-processing pass and draw the overlay elements into the final target.
    if(useIntermediate) {
        QRhiResourceUpdateBatch* postUpdates = implementation->preparePostProcess(cb);
        cb->beginPass(renderTarget, clearColor, { 1.0f, 0 }, postUpdates);
        cb->setViewport(QRhiViewport(0, 0, float(pixelSize.width()), float(pixelSize.height())));
        cb->setScissor(QRhiScissor(0, 0, pixelSize.width(), pixelSize.height()));
        implementation->runPostProcess(cb, renderTarget);
        implementation->renderOverLayerOnly(cb, renderTarget);
        cb->endPass();
    }
}

}   // End of namespace
