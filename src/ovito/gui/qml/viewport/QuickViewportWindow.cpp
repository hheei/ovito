// SPDX-FileCopyrightText: 2026 OVITO GmbH, Germany
// SPDX-License-Identifier: GPL-3.0-only OR MIT

#include <ovito/core/app/UserInterface.h>
#include <ovito/core/viewport/Viewport.h>
#include <ovito/core/rendering/SceneRenderer.h>
#include "QuickViewportItem.h"
#include "QuickViewportWindow.h"

namespace Ovito {

IMPLEMENT_CREATABLE_OVITO_CLASS(QuickViewportWindow);

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
    // TODO: Implement object picking by rendering a picking pass into the QQuickRhiItem's texture render
    // target and reading back the object IDs. This is part of the Phase 1 spike (see docs/design/UI_PLAN.md)
    // and is required before selection modes can be enabled.
    return std::nullopt;
}

/******************************************************************************
* Returns the icon image used for displaying non-fatal rendering warnings.
******************************************************************************/
QImage QuickViewportWindow::warningIcon() const
{
    return QIcon(QStringLiteral(":/guibase/mainwin/status/status_error.svg")).pixmap(22, 22).toImage();
}

}   // End of namespace
