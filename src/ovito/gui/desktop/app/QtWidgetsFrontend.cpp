// SPDX-FileCopyrightText: 2026 OVITO GmbH, Germany
// SPDX-License-Identifier: GPL-3.0-only OR MIT

#include <ovito/gui/desktop/GUI.h>
#include <ovito/gui/desktop/mainwin/MainWindow.h>
#include <ovito/gui/desktop/mainwin/MainWindowUI.h>
#include "QtWidgetsFrontend.h"

namespace Ovito {

/******************************************************************************
* Returns the name under which this frontend is selected on the command line.
******************************************************************************/
QString QtWidgetsFrontend::name() const
{
    return QStringLiteral("qt-widgets");
}

/******************************************************************************
* Returns a human-readable description of this frontend.
******************************************************************************/
QString QtWidgetsFrontend::description() const
{
    return QCoreApplication::translate("QtWidgetsFrontend", "The main window that OVITO has always had, built on the Qt Widgets module.");
}

/******************************************************************************
* Creates the classic main window and its user interface object.
******************************************************************************/
MainThreadOperation QtWidgetsFrontend::createWorkbench() const
{
    OVITO_ASSERT(this_task::isMainThread());
    OVITO_ASSERT(this_task::get());

    // Create the main window widget and user interface object.
    OORef<MainWindowUI> mainWinUI = OORef<MainWindowUI>::create();

    // Show the main window.
    mainWinUI->mainWindow()->setUpdatesEnabled(false);
    mainWinUI->mainWindow()->restoreMainWindowGeometry();
    mainWinUI->mainWindow()->restoreLayout();
    mainWinUI->mainWindow()->setUpdatesEnabled(true);

    return MainThreadOperation(*mainWinUI, MainThreadOperation::Kind::Isolated);
}

}   // End of namespace
