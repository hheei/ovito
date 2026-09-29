// SPDX-FileCopyrightText: 2026 OVITO GmbH, Germany
// SPDX-License-Identifier: GPL-3.0-only OR MIT

#include <ovito/gui/base/GUIBase.h>
#include "GuiTaskScope.h"

namespace Ovito {

/******************************************************************************
* Opens a task context for the given user interface.
******************************************************************************/
GuiTaskScope::GuiTaskScope(UserInterface& userInterface, bool isInteractive) :
    _operation(userInterface, MainThreadOperation::Bound, isInteractive)
{
}

/******************************************************************************
* Closes the task context.
******************************************************************************/
GuiTaskScope::~GuiTaskScope() = default;

}   // End of namespace
