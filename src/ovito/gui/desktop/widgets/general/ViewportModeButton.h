// SPDX-FileCopyrightText: 2026 OVITO GmbH, Germany
// SPDX-License-Identifier: GPL-3.0-only OR MIT

#pragma once


#include <ovito/gui/desktop/GUI.h>

namespace Ovito {

/**
 * An button widget that activates a ViewportInputMode.
 */
class OVITO_GUI_EXPORT ViewportModeButton : public QPushButton
{
    Q_OBJECT

public:

    /// Constructor.
    ViewportModeButton(ViewportModeAction* action, QWidget* parent = nullptr);

protected:

    virtual void hideEvent(QHideEvent* event) override {
        // When the button becomes hidden from the user, automatically deactivate the viewport input mode.
        // This is to prevent the viewport mode from remaining active when the user switches to another command panel tab.
        if(!event->spontaneous() && isChecked())
            click();

        QPushButton::hideEvent(event);
    }
};

}   // End of namespace
