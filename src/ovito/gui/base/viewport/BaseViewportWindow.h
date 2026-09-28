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
