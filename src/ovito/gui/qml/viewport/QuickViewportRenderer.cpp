// SPDX-FileCopyrightText: 2026 OVITO GmbH, Germany
// SPDX-License-Identifier: GPL-3.0-only OR MIT

#include <ovito/core/app/UserInterface.h>
#include <ovito/core/utilities/concurrent/TaskProgress.h>
#include <ovito/core/utilities/concurrent/Promise.h>
#include "QuickViewportItem.h"
#include "QuickViewportRenderer.h"
#include "QuickViewportWindow.h"
#include "QuickRendererService.h"

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
    if(_service)
        _service->unregisterRenderer(this);

    // Make sure the renderer implementation and all QRhi resources it holds are released while
    // the QRhi instance of the scene graph is still alive.
    _implementations.reset();
    _frameGraph.reset();
    _rendererConfig.reset();
}

/******************************************************************************
* Releases the renderer implementations of this render target.
******************************************************************************/
void QuickViewportRenderer::releaseGraphicsResources()
{
    // The renderer implementation holds resource frames of the shared resource cache, and releasing them frees
    // the GPU buffers they reference. Dropping the implementation also means that the next frame creates one for
    // the new QRhi instance.
    _implementations.reset();
}

/******************************************************************************
* Is called when the GPU resources of the item need to be (re)created.
******************************************************************************/
void QuickViewportRenderer::initialize(QRhiCommandBuffer* cb)
{
    Q_UNUSED(cb);

    // The color buffer or its sample count changed, which means the render pass descriptor of the
    // item is not compatible with the previously created pipelines anymore. Drop the implementations;
    // the shared pipeline cache of the service takes care of the pipelines themselves, which are only
    // handed out for a matching render pass descriptor.
    _implementations.reset();
}

/******************************************************************************
* Is called while the GUI thread is blocked, right before rendering.
******************************************************************************/
void QuickViewportRenderer::synchronize(QQuickRhiItem* item)
{
    OVITO_ASSERT(dynamic_cast<QuickViewportItem*>(item) != nullptr);
    auto* quickItem = static_cast<QuickViewportItem*>(item);

    // The GUI thread is blocked here, so the state of the item can be read safely. This is also where the
    // renderer learns which GPU resources it uses: the ones of the window the item belongs to.
    if(QuickRendererService* service = quickItem->renderService()) {
        if(service != _service) {
            if(_service)
                _service->unregisterRenderer(this);
            _service = service;
            _service->registerRenderer(this);
        }
        // The service is shared by the window's items, but its QRhi instance only becomes available once the
        // scene graph is initialized, and it is handed to a renderer rather than to the window.
        _service->setRhi(QQuickRhiItemRenderer::rhi());
    }

    if(QuickViewportWindow* viewportWindow = quickItem->viewportWindow(); viewportWindow && viewportWindow->hasUserInterface())
        _userInterface = viewportWindow->ui().shared_from_this();

    // Pick up the frame graph the GUI thread has generated in the meantime. If there is none,
    // the previously rendered frame graph is rendered again (e.g. after a resize).
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

    // Without a service there is no QRhi instance to render with, i.e. the scene graph could not provide the
    // graphics resources of the window.
    if(!_service || !_service->rhi()) {
        cb->beginPass(renderTarget, QColor(24, 24, 24), { 1.0f, 0 });
        cb->endPass();
        return;
    }

    if(!_frameGraph || !_rendererConfig) {
        // Nothing to display yet. Clear the item's color buffer with the viewport background color.
        cb->beginPass(renderTarget, QColor(24, 24, 24), { 1.0f, 0 });
        cb->endPass();
        return;
    }

    bool succeeded = false;
    try {
        // Establish a task context for the rendering work, exactly as RenderThread does it for its render
        // passes. OVITO's rendering code expects an active task and a UserInterface.
        Promise<void> promise = Promise<void>::create();
        promise.task()->setIsInteractive();
        if(_userInterface)
            promise.task()->setUserInterface(_userInterface);
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
    OVITO_ASSERT(_service);

    // The pass sequence is the one OVITO uses for all its render targets; it is implemented once, in the core,
    // so that the Qt Quick renderer cannot drift apart from the classic render thread. Nothing is specific to
    // this target: the item has no warning indicator and no watermark, and the frame graph is rendered into the
    // texture render target of the item rather than into a swap chain or an offscreen target. The pass runs
    // inside the QRhi frame Qt Quick has already started, which is why the caller here is render().
    FrameGraphRenderPass::execute(FrameGraphRenderPass::Arguments{
        .service = *_service.data(),
        .cb = *cb,
        .renderTarget = *renderTarget,
        .frameGraph = frameGraph,
        .configuration = rendererConfig,
        .implementations = _implementations,
    });
}

}   // End of namespace
