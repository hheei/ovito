// SPDX-FileCopyrightText: 2026 OVITO GmbH, Germany
// SPDX-License-Identifier: GPL-3.0-only OR MIT

#pragma once


#include <ovito/gui/desktop/GUI.h>
#include <ovito/gui/base/viewport/ViewportInputMode.h>
#include <ovito/core/oo/RefTarget.h>
#include <ovito/core/dataset/pipeline/PipelineStatus.h>

namespace Ovito {

/**
 * Viewport mouse input mode, which allows the user to interactively move a viewport overlay
 * using the mouse.
 */
class OVITO_GUI_EXPORT MoveOverlayInputMode : public ViewportInputMode
{
public:

    /// Constructor.
    void initializeObject(PropertiesEditor* editor);

    /// Called when the viewport input handler becomes the current one.
    virtual void activated(bool temporary) override;

    /// Called when the viewport input handler no longer is the current one.
    virtual void deactivated(bool temporary) override;

    /// Handles the mouse down events for a Viewport.
    virtual void mousePressEvent(ViewportWindow* vpwin, QMouseEvent* event) override;

    /// Handles the mouse move events for a Viewport.
    virtual void mouseMoveEvent(ViewportWindow* vpwin, QMouseEvent* event) override;

    /// Handles the mouse up events for a Viewport.
    virtual void mouseReleaseEvent(ViewportWindow* vpwin, QMouseEvent* event) override;

    /// Returns the current viewport we are working in.
    Viewport* viewport() const { return _viewport; }

private:

    /// The current viewport we are working in.
    Viewport* _viewport = nullptr;

    /// The properties editor of the viewport overlay being moved.
    PropertiesEditor* _editor;

    /// Mouse position at first click.
    QPointF _startPoint;

    /// The current mouse position.
    QPointF _currentPoint;

    /// The cursor shown when the overlay can be moved.
    QCursor _moveCursor = QCursor(QPixmap(QStringLiteral(":/guibase/cursor/editing/cursor_mode_move.png")));

    /// The cursor shown when in the wrong viewport.
    QCursor _forbiddenCursor = Qt::ForbiddenCursor;

    /// To undo changes while dragging the mouse.
    UndoableTransaction _undoTransaction;
};

}   // End of namespace
