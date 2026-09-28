// SPDX-FileCopyrightText: 2026 OVITO GmbH, Germany
// SPDX-License-Identifier: GPL-3.0-only OR MIT

#include <ovito/core/app/UserInterface.h>
#include <ovito/core/viewport/Viewport.h>
#include "QuickViewportItem.h"
#include "QuickViewportRenderer.h"
#include "QuickViewportWindow.h"
#include "QuickRendererService.h"

namespace Ovito {

/******************************************************************************
* Constructor.
******************************************************************************/
QuickViewportItem::QuickViewportItem(QQuickItem* parent) : QQuickRhiItem(parent)
{
    // Receive mouse and keyboard input from the user.
    setAcceptedMouseButtons(Qt::AllButtons);
    setAcceptHoverEvents(true);
    setFlag(QQuickItem::ItemIsFocusScope, true);
    setActiveFocusOnTab(true);
}

/******************************************************************************
* Destructor.
******************************************************************************/
QuickViewportItem::~QuickViewportItem()
{
    if(_viewportWindow)
        _viewportWindow->releaseResources();
}

/******************************************************************************
* Associates this item with a viewport of the current dataset.
******************************************************************************/
void QuickViewportItem::initializeWindow(Viewport* viewport, UserInterface& userInterface, SceneRenderer* renderer)
{
    OVITO_ASSERT(!_viewportWindow);
    OVITO_ASSERT(renderer);

    _viewportWindow = OORef<QuickViewportWindow>::create();

    // Assign the renderer before the viewport is shown, because showing the window triggers the
    // generation of the first frame graph, which requires a renderer to be present.
    _viewportWindow->setSceneRenderer(renderer);

    _viewportWindow->initializeWindow(viewport, userInterface, this);

    // Report the completion of a rendered frame back to the viewport machinery, which uses this
    // notification to finish tasks and to continue animation playback.
    connect(this, &QuickViewportItem::frameRendered, _viewportWindow.get(), [this]() {
        if(_viewportWindow) {
            _viewportWindow->handleShowEvent();
            Q_EMIT _viewportWindow->frameCompleted();
        }
    });

    // Tell the viewport to render as soon as it becomes visible.
    if(isVisible()) {
        _shown = true;
        _viewportWindow->handleShowEvent();
    }
    _viewportWindow->handleResize();
}

/******************************************************************************
* Hands a newly generated frame graph over to the render thread.
******************************************************************************/
void QuickViewportItem::submitFrameGraph(OORef<FrameGraph> frameGraph, std::unique_ptr<SceneRenderer::Configuration> rendererConfig)
{
    OVITO_ASSERT(this_task::isMainThread());

    _pendingFrameGraph = std::move(frameGraph);
    _pendingRendererConfig = std::move(rendererConfig);

    // Schedule a scene graph update. The pending frame graph is picked up by the renderer during
    // the next synchronization phase.
    update();
}

/******************************************************************************
* Discards the frame graph waiting to be rendered.
******************************************************************************/
void QuickViewportItem::releaseFrameGraph()
{
    _pendingFrameGraph.reset();
    _pendingRendererConfig.reset();
}

/******************************************************************************
* Takes the pending frame graph and renderer configuration (if any).
******************************************************************************/
void QuickViewportItem::takePendingFrame(OORef<FrameGraph>& frameGraph, std::unique_ptr<SceneRenderer::Configuration>& rendererConfig)
{
    if(_pendingFrameGraph) {
        frameGraph = std::move(_pendingFrameGraph);
        _pendingFrameGraph.reset();
        rendererConfig = std::move(_pendingRendererConfig);
    }
}

/******************************************************************************
* Creates the renderer object that implements the rendering logic on the Qt Quick render thread.
******************************************************************************/
QQuickRhiItemRenderer* QuickViewportItem::createRenderer()
{
    return new QuickViewportRenderer(this);
}

/******************************************************************************
* Is called when the geometry of the item has changed.
******************************************************************************/
/******************************************************************************
* Returns the render service of the given window, creating it on first use.
******************************************************************************/
QuickRendererService* QuickViewportItem::ensureRenderService(QQuickWindow* window)
{
    if(!window)
        return nullptr;

    // The service is a child of the window, so that it exists exactly as long as the window (and with it the
    // QRhi instance) does. It is found again rather than created a second time, because every viewport item of
    // the window uses the same one.
    if(auto* service = window->findChild<QuickRendererService*>(QStringLiteral("ovitoRendererService"), Qt::FindDirectChildrenOnly))
        return service;
    return new QuickRendererService(window);
}

/******************************************************************************
* Is called when the geometry of the item has changed.
******************************************************************************/
void QuickViewportItem::geometryChange(const QRectF& newGeometry, const QRectF& oldGeometry)
{
    QQuickRhiItem::geometryChange(newGeometry, oldGeometry);

    if(newGeometry.size() != oldGeometry.size() && _viewportWindow)
        _viewportWindow->handleResize();
}

/******************************************************************************
* Is called when a property of the item has changed.
******************************************************************************/
void QuickViewportItem::itemChange(ItemChange change, const ItemChangeData& value)
{
    QQuickRhiItem::itemChange(change, value);

    if(change == ItemSceneChange) {
        // The GPU resources of a window are shared by all of its viewport items and are owned by the window, so
        // the service is picked up when the item enters a window (and dropped when it leaves it).
        _renderService = ensureRenderService(value.window);
    }
    else if(change == ItemVisibleHasChanged && _viewportWindow) {
        const bool visible = value.boolValue && window() != nullptr;
        if(visible && !_shown) {
            _shown = true;
            _viewportWindow->handleShowEvent();
        }
        else if(!visible && _shown) {
            _shown = false;
            _viewportWindow->handleHideEvent();
        }
    }
}

/******************************************************************************
* Is called when the scene graph resources of this item are released.
******************************************************************************/
void QuickViewportItem::releaseResources()
{
    // The renderer (and with it all GPU resources of this viewport) is destroyed by the scene graph
    // on the render thread. Here we only need to make sure the viewport machinery does not continue
    // to generate frame graphs for a surface that no longer exists.
    releaseFrameGraph();
    if(_viewportWindow)
        _viewportWindow->releaseResources();

    QQuickRhiItem::releaseResources();
}

/******************************************************************************
* Handles double click events.
******************************************************************************/
void QuickViewportItem::mouseDoubleClickEvent(QMouseEvent* event)
{
    if(_viewportWindow) {
        setFocus(true);
        _viewportWindow->handleMouseDoubleClick(event);
    }
}

/******************************************************************************
* Handles mouse press events.
******************************************************************************/
void QuickViewportItem::mousePressEvent(QMouseEvent* event)
{
    if(_viewportWindow) {
        setFocus(true);
        _viewportWindow->handleMousePress(event);
        if(event->isAccepted())
            grabMouse();
    }
}

/******************************************************************************
* Handles mouse release events.
******************************************************************************/
void QuickViewportItem::mouseReleaseEvent(QMouseEvent* event)
{
    if(_viewportWindow) {
        _viewportWindow->handleMouseRelease(event);
        if(event->buttons() == Qt::NoButton)
            ungrabMouse();
    }
}

/******************************************************************************
* Handles mouse move events.
******************************************************************************/
void QuickViewportItem::mouseMoveEvent(QMouseEvent* event)
{
    if(_viewportWindow)
        _viewportWindow->handleMouseMove(event);
}

/******************************************************************************
* Handles mouse wheel events.
******************************************************************************/
void QuickViewportItem::wheelEvent(QWheelEvent* event)
{
    if(_viewportWindow)
        _viewportWindow->handleWheel(event);
}

/******************************************************************************
* Handles key-press events.
******************************************************************************/
void QuickViewportItem::keyPressEvent(QKeyEvent* event)
{
    if(_viewportWindow)
        _viewportWindow->handleKeyPress(event);
    if(!event->isAccepted())
        QQuickRhiItem::keyPressEvent(event);
}

/******************************************************************************
* Is called when the item looses the input focus.
******************************************************************************/
void QuickViewportItem::focusOutEvent(QFocusEvent* event)
{
    if(_viewportWindow)
        _viewportWindow->handleFocusOut(event);
}

/******************************************************************************
* Is called when the mouse cursor leaves the item.
******************************************************************************/
void QuickViewportItem::hoverLeaveEvent(QHoverEvent* event)
{
    if(_viewportWindow)
        _viewportWindow->handleLeave(event);
}

}   // End of namespace
