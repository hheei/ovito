// SPDX-FileCopyrightText: 2026 OVITO GmbH, Germany
// SPDX-License-Identifier: GPL-3.0-only OR MIT

#include <ovito/core/app/UserInterface.h>
#include <ovito/core/viewport/Viewport.h>
#include <ovito/core/rendering/SceneRenderer.h>
#include <ovito/core/utilities/concurrent/Future.h>
#include <ovito/core/utilities/concurrent/Promise.h>
#include "QuickViewportItem.h"
#include "QuickViewportWindow.h"

#include <QTimer>

namespace Ovito {

IMPLEMENT_CREATABLE_OVITO_CLASS(QuickViewportWindow);

/******************************************************************************
* Constructor.
******************************************************************************/
QuickViewportWindow::QuickViewportWindow()
{
    // Refresh the picking buffer once the viewport has settled, so that a hover that follows an interaction (or a
    // change of the scene) is answered from the buffer of the current view instead of the previous one.
    _pickingPrewarmTimer.setSingleShot(true);
    _pickingPrewarmTimer.setInterval(150);
    connect(&_pickingPrewarmTimer, &QTimer::timeout, this, &QuickViewportWindow::pickingPrewarmTimeout);

    connect(&_pickingBufferWatcher, &FutureWatcher<Future<ObjectPickingBuffer>>::completed, this, &QuickViewportWindow::pickingBufferReady);
    connect(&_pickingBufferWatcher, &FutureWatcher<Future<ObjectPickingBuffer>>::error, this, &QuickViewportWindow::pickingBufferFailed);
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
    // Stop the pre-warm and cancel a picking pass that is still in flight.
    _pickingPrewarmTimer.stop();
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
        // The rendered contents of the viewport have changed, so a previously rendered picking buffer no longer
        // describes what is on the screen (see isPickingBufferCurrent()).
        _pickingBufferGeneration++;

        // The new view supersedes a picking pass that is still in flight, and it is the view a later hover has to be
        // answered from. The pass that takes care of it is started once the viewport stops rendering (see
        // pickingPrewarmTimeout() for why that is not done right away).
        _pickingPrewarmTimer.start();

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
    //
    // The pass is started here and by the pre-warm below - nowhere else. (Both go through the same guarded path, and
    // a pass always belongs to the scene it was generated from: the buffer it produces is queried long after the pass
    // has been read back, so it must not depend on the render service that produced it. See ObjectPickingMap.)
    if(!isPickingBufferCurrent())
        refreshPickingBuffer();

    if(!_pickingBuffer.isValid())
        return std::nullopt;

    // A picking buffer only describes the viewport contents it was rendered from. If the viewport has been
    // resized since (a window resize or a layout pass), the buffer belongs to a different geometry and the
    // position would be looked up in the wrong place, so no result is returned until the pass for the current
    // size has been rendered.
    if(_pickingBuffer.bufferSize() != viewportWindowDeviceSize())
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

    // Re-render the buffer only after the previous pass has finished. This limits the picking
    // workload to one pass at a time, no matter how many pick requests arrive in between.
    if(_pickingBufferWatcher.hasFuture())
        return;

    _pickingBufferPendingGeneration = _pickingBufferGeneration;
    _pickingBufferWatcher.setFuture(renderPickingBuffer());
}

/******************************************************************************
* Stores the result of the asynchronous picking pass.
******************************************************************************/
void QuickViewportWindow::pickingBufferReady()
{
    _pickingBuffer = _pickingBufferWatcher.result();
    _pickingFailureReported = false;

    // The buffer describes the viewport contents the pass was rendered from. If the viewport rendered something else
    // while the pass was in flight, the buffer stays behind the view and another pass is needed - the pre-warm takes
    // care of that as soon as the view has settled again. A pass that produced no usable buffer at all (a hidden
    // viewport, for example) is not retried here: that would repeat itself every 150 ms for an unchanged view, and a
    // pick or the next frame graph starts a fresh attempt anyway.
    _pickingBufferRenderedGeneration = _pickingBufferPendingGeneration;
    if(_pickingBuffer.isValid() && !isPickingBufferCurrent())
        _pickingPrewarmTimer.start();
}

/******************************************************************************
* Refreshes the picking buffer once the viewport has settled (see the header).
******************************************************************************/
void QuickViewportWindow::pickingPrewarmTimeout()
{
    // A hidden viewport cannot be picked, and rendering an offscreen pass for it would only repeat itself.
    if(isVisible() && !isPickingBufferCurrent())
        refreshPickingBuffer();
}

/******************************************************************************
* Indicates whether the cached picking buffer still describes the current contents of the viewport.
******************************************************************************/
bool QuickViewportWindow::isPickingBufferCurrent() const
{
    return _pickingBuffer.isValid()
        && _pickingBufferRenderedGeneration == _pickingBufferGeneration
        && _pickingBuffer.bufferSize() == viewportWindowDeviceSize();
}

/******************************************************************************
* Reports a picking pass that terminated with an error instead of producing a buffer.
******************************************************************************/
void QuickViewportWindow::pickingBufferFailed(const Exception& exception)
{
    // Without this the failure would only show up as a viewport that never has a picking buffer, i.e. as
    // "picking does not work", with the reason nowhere in the log. Report it once per series of failures;
    // a subsequent successful pass clears the flag, and a later pick request starts a new attempt.
    if(!_pickingFailureReported) {
        _pickingFailureReported = true;
        qWarning() << "QuickViewportWindow: the picking pass failed:" << exception.message();
    }

    // Keep the buffer stale: a later pick or the pre-warm retries the pass instead of answering from a buffer the
    // failed pass did not replace.
    _pickingBufferRenderedGeneration = 0;
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

    // The picking pass is rendered at the resolution of the interactive frame, because the object under a
    // given pixel is determined by the projection and the viewport size. An empty size means that the item
    // has not been laid out yet.
    if(viewportWindowDeviceSize().isEmpty())
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

    // The frame graph carries the projection computed from the viewport geometry at the time it was generated.
    // If that geometry changed while the frame graph was being built (a window resize or a layout pass), the
    // projection does not belong to the size the picking buffer is rendered at, and every pick result derived
    // from the buffer would be shifted. Discard such a pass; the next pick request starts a new one.
    const QSize currentSize = viewportWindowDeviceSize();
    if(currentSize.isEmpty() ||
       std::abs(frameGraph->projectionParams().aspectRatio - FloatType(currentSize.height()) / currentSize.width()) > FloatType(1e-6))
        co_return ObjectPickingBuffer();

    // The picking target keeps the GPU resources of the last pass and is reused while the viewport size does not
    // change; it is created together with the buffers on the first pass. It also keeps the render thread (and the
    // graphics device) alive for as long as the viewport window exists.
    if(!_pickingTarget)
        _pickingTarget.emplace(ui(), OffscreenRenderTarget::Kind::PickingOnly);

    co_return co_await FutureAwaiter(ObjectExecutor(this),
        _pickingTarget->renderPicking(std::move(frameGraph), *sceneRenderer(), currentSize));
}

/******************************************************************************
* Returns the icon image used for displaying non-fatal rendering warnings.
******************************************************************************/
QImage QuickViewportWindow::warningIcon() const
{
    return QIcon(QStringLiteral(":/guibase/mainwin/status/status_error.svg")).pixmap(22, 22).toImage();
}

}   // End of namespace
