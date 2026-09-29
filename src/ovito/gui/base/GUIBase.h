// SPDX-FileCopyrightText: 2026 OVITO GmbH, Germany
// SPDX-License-Identifier: GPL-3.0-only OR MIT

//
// Standard precompiled header file included by all source files in this module
//

#ifndef __OVITO_GUI_BASE_
#define __OVITO_GUI_BASE_

#include <ovito/core/Core.h>

/******************************************************************************
* Qt framework classes.
******************************************************************************/
#include <QResource>
#include <QtDebug>
#include <QtGui>
#include <QAction>

/******************************************************************************
* Forward declaration of classes.
******************************************************************************/
namespace Ovito
{
    class UserInterface;
    class BaseViewportWindow;
    class ActionManager;
    class Command;
    class ViewportInputManager;
    class ViewportInputMode;
    class ViewportGizmo;
    class UtilityObject;
}

#endif // __OVITO_GUI_BASE_
