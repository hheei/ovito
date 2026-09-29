// SPDX-FileCopyrightText: 2026 OVITO GmbH, Germany
// SPDX-License-Identifier: GPL-3.0-only OR MIT

#include <ovito/gui/base/GUIBase.h>
#include <ovito/core/app/UserInterface.h>
#include <ovito/gui/base/actions/Command.h>
#include "ViewportModeAction.h"

namespace Ovito {

/******************************************************************************
* Initializes the action object.
******************************************************************************/
ViewportModeAction::ViewportModeAction(UserInterface& ui, const QString& text, QObject* parent, OORef<ViewportInputMode> inputMode, const QColor& highlightColor) :
    QAction(text, parent),
    _highlightColor(highlightColor)
{
    OVITO_CHECK_POINTER(ui.viewportInputManager());

    // The command implements the behaviour; this action only presents it.
    _command = new ViewportModeCommand(ui, {}, text, std::move(inputMode), highlightColor, {}, {}, {}, this);

    setCheckable(true);
    connect(_command, &Command::changed, this, [this]() {
        setText(_command->text());
        setEnabled(_command->isEnabled());
        setChecked(_command->isChecked());
    });
    connect(this, &QAction::triggered, _command, &Command::trigger);
    connect(this, &QAction::toggled, _command, &Command::setChecked);
    setChecked(_command->isChecked());
}

}   // End of namespace
