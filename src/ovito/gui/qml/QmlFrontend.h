// SPDX-FileCopyrightText: 2026 OVITO GmbH, Germany
// SPDX-License-Identifier: GPL-3.0-only OR MIT

//
// Standard precompiled header file included by all source files in this module
//

#ifndef __OVITO_GUI_QML_
#define __OVITO_GUI_QML_

#include <ovito/gui/base/GUIBase.h>

/******************************************************************************
* Qt Quick classes.
******************************************************************************/
#include <QtQuick>
#include <QtQuick/qquickrhiitem.h>

/******************************************************************************
* Forward declaration of classes.
******************************************************************************/
namespace Ovito
{
    class QuickViewportItem;
    class QuickViewportWindow;
    class QuickViewportRenderer;
    class QmlMainWindowUI;
    class QmlViewportController;
}

#endif // __OVITO_GUI_QML_
