// SPDX-FileCopyrightText: 2026 OVITO GmbH, Germany
// SPDX-License-Identifier: GPL-3.0-only OR MIT

#pragma once


#include <ovito/gui/base/GUIBase.h>
#include <ovito/core/viewport/ViewportWindow.h>

namespace Ovito {

/**
 * \brief Generic base class for viewport windows that implements mouse input handling.
 */
class OVITO_GUIBASE_EXPORT BaseViewportWindow : public ViewportWindow
{
    Q_OBJECT
    OVITO_CLASS(BaseViewportWindow)

public:

    /// Returns the list of gizmos to render in the viewport.
    virtual std::vector<ViewportGizmo*> viewportGizmos() override;

protected:

    /// Is called when the viewport becomes visible.
    void showEvent(QShowEvent* event);

    /// Is called when the viewport becomes hidden.
    void hideEvent(QHideEvent* event);

    /// Is called when the mouse cursor leaves the widget.
    void leaveEvent(QEvent* event);

    /// Handles double click events.
    void mouseDoubleClickEvent(QMouseEvent* event);

    /// Handles mouse press events.
    void mousePressEvent(QMouseEvent* event);

    /// Handles mouse release events.
    void mouseReleaseEvent(QMouseEvent* event);

    /// Handles mouse move events.
    void mouseMoveEvent(QMouseEvent* event);

    /// Handles mouse wheel events.
    void wheelEvent(QWheelEvent* event);

    /// Is called when the widget looses the input focus.
    void focusOutEvent(QFocusEvent* event);

    /// Is called when the widget is resized.
    void resizeEvent(QResizeEvent* event);

    /// Handles key-press events.
    void keyPressEvent(QKeyEvent* event);

    /// Handles requests to show the viewport's context menu that did not originate from a
    /// mouse click on the viewport caption (see mousePressEvent()) - i.e. the Menu/Shift+F10
    /// keyboard shortcut and assistive technologies (e.g. VoiceOver's "show menu" action).
    void contextMenuEvent(QContextMenuEvent* event);
};

}   // End of namespace
