// SPDX-FileCopyrightText: 2026 OVITO GmbH, Germany
// SPDX-License-Identifier: GPL-3.0-only OR MIT

#include <ovito/gui/base/GUIBase.h>
#include <ovito/gui/base/viewport/ViewportInputManager.h>
#include <ovito/gui/base/viewport/ViewportInputMode.h>
#include <ovito/core/app/UserInterface.h>
#include <ovito/core/viewport/Viewport.h>
#include <ovito/core/dataset/DataSet.h>
#include <ovito/core/dataset/DataSetContainer.h>
#include "BaseViewportWindow.h"

namespace Ovito {

IMPLEMENT_ABSTRACT_OVITO_CLASS(BaseViewportWindow);

/******************************************************************************
* Returns the list of gizmos to render in the viewport.
******************************************************************************/
std::vector<ViewportGizmo*> BaseViewportWindow::viewportGizmos()
{
    std::vector<ViewportGizmo*> gizmoList;

    // Global viewport gizmos, which are shown in all viewports.
    if(ViewportInputManager* man = viewportInputManager())
        gizmoList.insert(gizmoList.end(), man->viewportGizmos().begin(), man->viewportGizmos().end());

    // Specific viewport gizmos, which are only shown in this viewport.
    gizmoList.insert(gizmoList.end(), viewport()->viewportGizmos().begin(), viewport()->viewportGizmos().end());

    return gizmoList;
}

/******************************************************************************
* Handles show events.
******************************************************************************/
void BaseViewportWindow::showEvent(QShowEvent* event)
{
    // Resume updating the scene.
    scenePreparation().setAutoRestart(true);

    // Schedule a rendering pass if the window becomes visible and an update request has been scheduled while it was hidden.
    if(!event->spontaneous())
        handleUpdateRequests();
}

/******************************************************************************
* Handles hide events.
******************************************************************************/
void BaseViewportWindow::hideEvent(QHideEvent* event)
{
    // Stop updating the scene.
    scenePreparation().setAutoRestart(false);

    // Release all renderer resources when the window becomes hidden.
    releaseResources();

    // Emit signal.
    Q_EMIT viewportWindowHidden();
}

/******************************************************************************
* Handles double click events.
******************************************************************************/
void BaseViewportWindow::mouseDoubleClickEvent(QMouseEvent* event)
{
    if(viewportInputManager()) {
        if(ViewportInputMode* mode = viewportInputManager()->activeMode()) {
            handleExceptions<true>([&] {
                mode->mouseDoubleClickEvent(this, event);
            });
        }
    }
}

/******************************************************************************
* Handles mouse press events.
******************************************************************************/
void BaseViewportWindow::mousePressEvent(QMouseEvent* event)
{
    if(!viewportInputManager())
        return;

    // Make this viewport the active one.
    if(ViewportConfiguration* viewportConfig = datasetContainer().activeViewportConfig()) {
        handleExceptions<true>([&] {
            viewportConfig->setActiveViewport(viewport());
        });
    }

    // Intercept mouse clicks on the viewport caption.
    if(contextMenuArea().contains(ViewportInputMode::getMousePosition(event))) {
        viewportInputManager()->requestContextMenu(this, event->pos());
        return;
    }

    if(ViewportInputMode* mode = viewportInputManager()->activeMode()) {
        handleExceptions<true>([&] {
            mode->mousePressEvent(this, event);
        });
    }
}

/******************************************************************************
* Handles mouse release events.
******************************************************************************/
void BaseViewportWindow::mouseReleaseEvent(QMouseEvent* event)
{
    if(viewportInputManager()) {
        if(ViewportInputMode* mode = viewportInputManager()->activeMode()) {
            handleExceptions<true>([&] {
                mode->mouseReleaseEvent(this, event);
            });
        }
    }
}

/******************************************************************************
* Handles mouse move events.
******************************************************************************/
void BaseViewportWindow::mouseMoveEvent(QMouseEvent* event)
{
    if(contextMenuArea().contains(ViewportInputMode::getMousePosition(event)) && event->buttons() == Qt::NoButton) {
        setCursorInContextMenuArea(true);
    }
    else if(!contextMenuArea().contains(ViewportInputMode::getMousePosition(event))) {
        setCursorInContextMenuArea(false);
    }

    if(viewportInputManager()) {
        if(ViewportInputMode* mode = viewportInputManager()->activeMode()) {
            handleExceptions<true>([&] {
                mode->mouseMoveEvent(this, event);
            });
        }
    }
}

/******************************************************************************
* Handles mouse wheel events.
******************************************************************************/
void BaseViewportWindow::wheelEvent(QWheelEvent* event)
{
    if(viewportInputManager()) {
        if(ViewportInputMode* mode = viewportInputManager()->activeMode()) {
            handleExceptions<true>([&] {
                mode->wheelEvent(this, event);
            });
        }
    }
}

/******************************************************************************
* Is called when the mouse cursor leaves the widget.
******************************************************************************/
void BaseViewportWindow::leaveEvent(QEvent* event)
{
    setCursorInContextMenuArea(false);
    ui().clearStatusBarMessage();
}

/******************************************************************************
* Is called when the widget looses the input focus.
******************************************************************************/
void BaseViewportWindow::focusOutEvent(QFocusEvent* event)
{
    if(viewportInputManager()) {
        if(ViewportInputMode* mode = viewportInputManager()->activeMode()) {
            handleExceptions<true>([&] {
                mode->focusOutEvent(this, event);
            });
        }
    }
}

/******************************************************************************
* Is called when the widget is resized.
******************************************************************************/
void BaseViewportWindow::resizeEvent(QResizeEvent* event)
{
    requestRerender(false);
}

/******************************************************************************
* Handles key-press events.
******************************************************************************/
void BaseViewportWindow::keyPressEvent(QKeyEvent* event)
{
    if(viewportInputManager()) {
        if(ViewportInputMode* mode = viewportInputManager()->activeMode()) {
            handleExceptions<true>([&] {
                if(mode->keyPressEvent(this, event))
                    return; // Do not propagate handled key events to base class.
            });
        }
    }
}

/******************************************************************************
* Handles requests to show the viewport's context menu.
******************************************************************************/
void BaseViewportWindow::contextMenuEvent(QContextMenuEvent* event)
{
    // Context menu requests triggered by a mouse click are already handled by mousePressEvent(),
    // which only reacts to clicks on the viewport caption area and otherwise leaves the right
    // mouse button free for camera navigation (see NavigationModes). Here we only handle
    // keyboard-triggered requests (Menu key / Shift+F10) and requests coming from assistive
    // technologies (e.g. VoiceOver's "show menu" action), which are not tied to a click location.
    if(event->reason() != QContextMenuEvent::Mouse && viewportInputManager()) {
        viewportInputManager()->requestContextMenu(this, event->pos());
        event->accept();
    }
}

}   // End of namespace
