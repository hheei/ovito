// SPDX-FileCopyrightText: 2026 OVITO GmbH, Germany
// SPDX-License-Identifier: GPL-3.0-only OR MIT

#include <ovito/core/app/UserInterface.h>
#include <ovito/core/viewport/Viewport.h>
#include <ovito/core/rendering/SceneRenderer.h>
#include <ovito/core/utilities/concurrent/Future.h>
#include <ovito/core/utilities/concurrent/Promise.h>
#include "QuickViewportItem.h"
#include "QuickViewportWindow.h"

namespace Ovito {

IMPLEMENT_CREATABLE_OVITO_CLASS(QuickViewportWindow);

/******************************************************************************
* Constructor.
******************************************************************************/
QuickViewportWindow::QuickViewportWindow()
{
    connect(&_pickingBufferWatcher, &FutureWatcher<Future<ObjectPickingBuffer>>::completed, this, &QuickViewportWindow::pickingBufferReady);
}

/******************************************************************************
* Associates this window with a viewport and the QML item it renders into.
******************************************************************************/
void QuickViewportWindow::initializeWindow(Viewport* viewport, UserInterface& userInterface, QuickViewportItem* item)
{
    OVITO_ASSERT(viewport);
    OVITO_ASSERT(item);
    OVITO_ASSERT(!_item);

    _item = item;

    // Associate this window with the viewport. This also sets up the scene preparation
    // that evaluates the pipelines for interactive rendering.
    setViewport(viewport, userInterface);

    // Release the renderer resources before the application shuts down, e.g. due to a Python script error.
    connect(QCoreApplication::instance(), &QObject::destroyed, this, &QuickViewportWindow::releaseResources);
}

/******************************************************************************
* This method is called after the reference counter of this object has reached zero
* and before the object is being finally deleted.
******************************************************************************/
void QuickViewportWindow::aboutToBeDeleted()
{
    _item = nullptr;
    BaseViewportWindow::aboutToBeDeleted();
}

/******************************************************************************
* Releases the renderer resources held by this viewport window.
******************************************************************************/
void QuickViewportWindow::releaseResources()
{
    // Stop a picking pass that is still in flight.
    _pickingBufferWatcher.requestCancelation();

    if(_item)
        _item->releaseFrameGraph();

    BaseViewportWindow::releaseResources();
}

/******************************************************************************
* Hands the frame graph of the next frame over to the QML item for rendering.
******************************************************************************/
void QuickViewportWindow::renderFrameGraph(OORef<FrameGraph> frameGraph)
{
    if(_item && sceneRenderer() && frameGraph) {
        // The rendered contents of the viewport have changed, so a previously rendered picking buffer
        // no longer describes what is on the screen.
        _pickingBufferStale = true;

        // Note: the configuration must be created before the frame graph is passed on, because the
        // order of evaluation of the function arguments is unspecified.
        std::unique_ptr<SceneRenderer::Configuration> rendererConfig = sceneRenderer()->createConfiguration(*frameGraph);
        _item->submitFrameGraph(std::move(frameGraph), std::move(rendererConfig));
    }
    else {
        // Nothing to render. Tell the caller that this frame is complete so that animation
        // playback and task progress reporting are not blocked.
        Q_EMIT frameCompleted();
    }
}

/******************************************************************************
* Handles show events.
******************************************************************************/
void QuickViewportWindow::handleShowEvent()
{
    QShowEvent event;
    showEvent(&event);
}

/******************************************************************************
* Handles hide events.
******************************************************************************/
void QuickViewportWindow::handleHideEvent()
{
    QHideEvent event;
    hideEvent(&event);
}

/******************************************************************************
* Is called when the item is resized.
******************************************************************************/
void QuickViewportWindow::handleResize()
{
    QResizeEvent event(viewportWindowDeviceIndependentSize(), viewportWindowDeviceIndependentSize());
    resizeEvent(&event);
}

/******************************************************************************
* Indicates whether the item is currently shown and renderable.
******************************************************************************/
bool QuickViewportWindow::isVisible() const
{
    return _item && _item->isVisible() && _item->window() && _item->window()->isVisible();
}

/******************************************************************************
* Returns the current size of the viewport window (in device pixels).
******************************************************************************/
QSize QuickViewportWindow::viewportWindowDeviceSize() const
{
    if(!_item)
        return {};
    return QSize(qRound(_item->width() * devicePixelRatio()), qRound(_item->height() * devicePixelRatio()));
}

/******************************************************************************
* Returns the current size of the viewport window (in device-independent pixels).
******************************************************************************/
QSize QuickViewportWindow::viewportWindowDeviceIndependentSize() const
{
    if(!_item)
        return {};
    return QSize(qRound(_item->width()), qRound(_item->height()));
}

/******************************************************************************
* Returns the device pixel ratio of the viewport window's canvas.
******************************************************************************/
qreal QuickViewportWindow::devicePixelRatio() const
{
    if(!_item || !_item->window())
        return 1.0;
    return _item->window()->effectiveDevicePixelRatio();
}

/******************************************************************************
* Determines the object located under the given mouse cursor position.
******************************************************************************/
std::optional<ViewportWindow::PickResult> QuickViewportWindow::pick(const QPointF& pos)
{
    // The picking buffer is rendered asynchronously by the render thread. If it is out of date, a new pass
    // is started here and no result can be returned yet. Blocking the GUI thread until the pass has been
    // rendered (the way the classic frontend waits for its render thread) is deliberately avoided: it would
    // stall input handling and the Qt Quick scene graph, both of which share this thread.
    if(_pickingBufferStale)
        refreshPickingBuffer();

    if(!_pickingBuffer.isValid())
        return std::nullopt;

    // Convert the cursor position from logical to picking buffer (device pixel) coordinates.
    return _pickingBuffer.pick(pos * devicePixelRatio(), pickRadius);
}

/******************************************************************************
* Starts rendering a new picking buffer if no other picking pass is in flight.
******************************************************************************/
void QuickViewportWindow::refreshPickingBuffer()
{
    OVITO_ASSERT(this_task::isMainThread());

    // The view is up to date as far as this method is concerned; the buffer is replaced when the pass completes.
    _pickingBufferStale = false;

    // Re-render the buffer only after the previous pass has finished. This limits the picking
    // workload to one pass at a time, no matter how many pick requests arrive in between.
    if(_pickingBufferWatcher.hasFuture())
        return;

    _pickingBufferWatcher.setFuture(renderPickingBuffer());
}

/******************************************************************************
* Stores the result of the asynchronous picking pass.
******************************************************************************/
void QuickViewportWindow::pickingBufferReady()
{
    _pickingBuffer = _pickingBufferWatcher.result();

    // The buffer describes the viewport contents the pass was rendered from, which may already have
    // been superseded. In that case the next pick request starts another pass.
    _pickingBufferStale = _pickingBufferStale || !_pickingBuffer.isValid();
}

/******************************************************************************
* Renders a picking pass for the current viewport contents on the render thread.
******************************************************************************/
Future<ObjectPickingBuffer> QuickViewportWindow::renderPickingBuffer()
{
    OVITO_ASSERT(this_task::isMainThread());

    // Suspend until control returns to the event loop.
    co_await ExecutorAwaiter(DeferredObjectExecutor(this));

    // Skip if the viewport window is currently hidden. The cached buffer remains valid then; a hidden
    // viewport cannot be picked anyway.
    if(!isVisible() || !viewport() || !sceneRenderer() || !hasUserInterface())
        co_return ObjectPickingBuffer();

    // The picking pass must be rendered at the resolution of the interactive frame, because the object
    // under a given pixel is determined by the projection and the viewport size.
    const QSize bufferSize = viewportWindowDeviceSize();
    if(bufferSize.isEmpty())
        co_return ObjectPickingBuffer();

    // Associate this task with the user interface and mark it as interactive. The picking pass is rendered
    // asynchronously by the render thread, and the task it is submitted from must carry the context of the
    // application - the same context the classic frontend establishes for its offscreen viewport rendering
    // (see WidgetViewportWindow::grabViewportImage) and what ViewportWindow::generateFrameGraph() sets up.
    this_task::get()->setUserInterface(ui().shared_from_this());
    this_task::get()->setIsInteractive();

    // Generate a dedicated frame graph for the picking pass. Handing the frame graph of the interactive
    // frame to the render thread instead would make two renderers share it across threads.
    OORef<FrameGraph> frameGraph = co_await FutureAwaiter(ObjectExecutor(this), generateFrameGraph());
    if(!frameGraph)
        co_return ObjectPickingBuffer();

    std::shared_ptr<RenderThread> renderThread = ui().renderThread();
    if(!renderThread)
        co_return ObjectPickingBuffer();

    // (Re-)create the offscreen render target that receives the picking buffers when the size changed.
    if(!_pickingTarget || _pickingTargetSize != bufferSize) {
        _pickingTarget.emplace(renderThread->createOffscreenTarget(bufferSize, /*forPickingOnly=*/true));
        _pickingTargetSize = bufferSize;
    }

    auto rendererConfig = sceneRenderer()->createConfiguration(*frameGraph);
    co_return co_await FutureAwaiter(ObjectExecutor(this),
        _pickingTarget->renderPickingFrame(std::move(frameGraph), std::move(rendererConfig)));
}

/******************************************************************************
* Returns the icon image used for displaying non-fatal rendering warnings.
******************************************************************************/
QImage QuickViewportWindow::warningIcon() const
{
    return QIcon(QStringLiteral(":/guibase/mainwin/status/status_error.svg")).pixmap(22, 22).toImage();
}

}   // End of namespace
