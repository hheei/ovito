// SPDX-FileCopyrightText: 2026 OVITO GmbH, Germany
// SPDX-License-Identifier: GPL-3.0-only OR MIT

#include <ovito/gui/base/GUIBase.h>
#include <ovito/gui/base/actions/ViewportModeAction.h>
#include "ViewportModeButton.h"

namespace Ovito {

/******************************************************************************
* Constructor.
******************************************************************************/
ViewportModeButton::ViewportModeButton(ViewportModeAction* action, QWidget* parent) : QPushButton(action->icon(), action->text(), parent)
{
    setCheckable(true);
    setChecked(action->isChecked());
    setToolTip(action->toolTip());

#ifndef Q_OS_MACOS
    if(action->highlightColor().isValid())
        setStyleSheet("QPushButton:checked { background-color: " + action->highlightColor().name() + " }");
    else
        setStyleSheet("QPushButton:checked { background-color: moccasin; }");
#endif

    connect(action, &ViewportModeAction::toggled, this, &QPushButton::setChecked);
    connect(this, &QPushButton::clicked, action, &ViewportModeAction::trigger);
}

}   // End of namespace
