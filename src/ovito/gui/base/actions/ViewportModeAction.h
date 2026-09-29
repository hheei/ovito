// SPDX-FileCopyrightText: 2026 OVITO GmbH, Germany
// SPDX-License-Identifier: GPL-3.0-only OR MIT

#pragma once


#include <ovito/gui/base/GUIBase.h>
#include <ovito/gui/base/actions/Command.h>

namespace Ovito {

/**
 * \brief The QAction that presents a ViewportModeCommand in a QtWidgets user interface.
 *
 * This class is only a view: the associated command owns the state of the input mode and implements
 * the activation logic. Menus, toolbars and buttons of the classic frontend use the QAction, while
 * the Qt Quick workbench uses the command directly.
 */
class OVITO_GUIBASE_EXPORT ViewportModeAction : public QAction
{
    Q_OBJECT

public:

    /// \brief Initializes the action object.
    ViewportModeAction(UserInterface& ui, const QString& text, QObject* parent, OORef<ViewportInputMode> inputMode, const QColor& highlightColor = QColor());

    /// Returns the highlight color for the button controls.
    const QColor& highlightColor() const { return _highlightColor; }

    /// Returns the command that owns the state of this action.
    ViewportModeCommand* command() const { return _command; }

public Q_SLOTS:

    /// \brief Activates the viewport input mode.
    void activateMode() {
        _command->activateMode();
    }

    /// \brief Deactivates the viewport input mode.
    void deactivateMode() {
        _command->deactivateMode();
    }

private:

    /// The command that activates the input mode and owns the check state.
    ViewportModeCommand* _command;

    /// The highlight color for the button controls.
    QColor _highlightColor;
};

}   // End of namespace
