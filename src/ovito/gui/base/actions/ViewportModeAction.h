// SPDX-FileCopyrightText: 2026 OVITO GmbH, Germany
// SPDX-License-Identifier: GPL-3.0-only OR MIT

#pragma once


#include <ovito/gui/base/GUIBase.h>

namespace Ovito {

/**
 * An Qt action that activates a ViewportInputMode.
 */
class OVITO_GUIBASE_EXPORT ViewportModeAction : public QAction, public UserInterfaceComponent<UserInterface>
{
    Q_OBJECT

public:

    /// \brief Initializes the action object.
    ViewportModeAction(UserInterface& ui, const QString& text, QObject* parent, OORef<ViewportInputMode> inputMode, const QColor& highlightColor = QColor());

    /// Returns the highlight color for the button controls.
    const QColor& highlightColor() const { return _highlightColor; }

public Q_SLOTS:

    /// \brief Activates the viewport input mode.
    void activateMode() {
        onActionToggled(true);
    }

    /// \brief Deactivates the viewport input mode.
    void deactivateMode() {
        onActionToggled(false);
    }

protected Q_SLOTS:

    /// Is called when the user or the program have triggered the action's state.
    void onActionToggled(bool checked);

    /// Is called when the user has triggered the action's state.
    void onActionTriggered(bool checked);

private:

    /// The viewport input mode activated by this action.
    OORef<ViewportInputMode> _inputMode;

    /// The highlight color for the button controls.
    QColor _highlightColor;
};

}   // End of namespace
