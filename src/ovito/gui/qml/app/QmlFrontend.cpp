// SPDX-FileCopyrightText: 2026 OVITO GmbH, Germany
// SPDX-License-Identifier: GPL-3.0-only OR MIT

#include <ovito/gui/qml/QmlFrontend.h>
#include <ovito/gui/qml/mainwin/QmlMainWindowUI.h>
#include <QtQuickControls2/QQuickStyle>
#include "QmlFrontend.h"

namespace Ovito {

/******************************************************************************
* Returns the name under which this frontend is selected on the command line.
******************************************************************************/
QString QmlFrontend::name() const
{
    return QStringLiteral("qml");
}

/******************************************************************************
* Returns a human-readable description of this frontend.
******************************************************************************/
QString QmlFrontend::description() const
{
    return QCoreApplication::translate("QmlFrontend", "The workbench built with Qt Quick, which is under development (see docs/design/UI_PLAN.md).");
}

/******************************************************************************
* Creates the Qt Quick workbench and its user interface object.
******************************************************************************/
MainThreadOperation QmlFrontend::createWorkbench() const
{
    OVITO_ASSERT(this_task::isMainThread());
    OVITO_ASSERT(this_task::get());

    // The shell uses the platform-independent Basic style of Qt Quick Controls and colors it from its own theme, so that
    // the workbench looks the same everywhere instead of inheriting the look of the platform's widget style. This must
    // happen before the QML scene is created.
    QQuickStyle::setStyle(QStringLiteral("Basic"));

    // Create the Qt Quick workbench, which opens its window right away.
    OORef<QmlMainWindowUI> workbench = OORef<QmlMainWindowUI>::create();
    workbench->initializeWindow();

    return MainThreadOperation(*workbench, MainThreadOperation::Kind::Isolated);
}

}   // End of namespace
