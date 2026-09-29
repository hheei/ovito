// SPDX-FileCopyrightText: 2026 OVITO GmbH, Germany
// SPDX-License-Identifier: GPL-3.0-only OR MIT

#include <ovito/gui/desktop/GUI.h>
#include <ovito/gui/desktop/mainwin/MainWindow.h>
#include <ovito/gui/base/actions/ActionManager.h>
#include "CommandPanel.h"
#include "RenderCommandPage.h"
#include "ModifyCommandPage.h"
#include "OverlayCommandPage.h"
#include "UtilityCommandPage.h"

namespace Ovito {

/******************************************************************************
* The constructor of the command panel class.
******************************************************************************/
CommandPanel::CommandPanel(MainWindowUI& userInterface, QWidget* parent) : QWidget(parent)
{
    QVBoxLayout* layout = new QVBoxLayout(this);
    layout->setContentsMargins(0,0,0,0);

    // Create tab widget
    _tabWidget = new QTabWidget(this);
    layout->addWidget(_tabWidget, 1);

    // Create the tabs.
    _tabWidget->setDocumentMode(true);
    _tabWidget->addTab(_modifyPage = new ModifyCommandPage(userInterface, _tabWidget), QIcon::fromTheme("command_panel_tab_modify"), QString());
    _tabWidget->addTab(_renderPage = new RenderCommandPage(userInterface, _tabWidget), QIcon::fromTheme("command_panel_tab_render"), QString());
    _tabWidget->addTab(_overlayPage = new OverlayCommandPage(userInterface, _tabWidget), QIcon::fromTheme("command_panel_tab_overlays"), QString());
    _tabWidget->addTab(_utilityPage = new UtilityCommandPage(userInterface, _tabWidget), QIcon::fromTheme("command_panel_tab_utilities"), QString());
    _tabWidget->setTabToolTip(0, tr("Pipelines"));
    _tabWidget->setTabToolTip(1, tr("Rendering"));
    _tabWidget->setTabToolTip(2, tr("Viewport layers"));
    _tabWidget->setTabToolTip(3, tr("Utilities"));
    // These tabs only show an icon, so their accessible name would otherwise be empty
    // (invisible to screen readers). Reuse the tooltip text as the accessible name.
    for(int i = 0; i < _tabWidget->count(); ++i)
        _tabWidget->tabBar()->setAccessibleTabName(i, _tabWidget->tabToolTip(i));
    setCurrentPage(MainWindow::MODIFY_PAGE);

    Command* showModifyPageCommand = userInterface.actionManager()->createCommand(ACTION_COMMAND_PANEL_MODIFY, tr("Pipeline editor"), {}, tr("Switches to the pipeline editing tab."));
    connect(showModifyPageCommand, &Command::triggered, this, [this]() { setCurrentPage(MainWindow::MODIFY_PAGE); });

    Command* showRenderPageCommand = userInterface.actionManager()->createCommand(ACTION_COMMAND_PANEL_RENDER, tr("Render settings"), {}, tr("Switches to the image & animation rendering tab."));
    connect(showRenderPageCommand, &Command::triggered, this, [this]() { setCurrentPage(MainWindow::RENDER_PAGE); });

    Command* showOverlayPageCommand = userInterface.actionManager()->createCommand(ACTION_COMMAND_PANEL_OVERLAYS, tr("Viewport layers"), {}, tr("Switches to the viewport layers tab."));
    connect(showOverlayPageCommand, &Command::triggered, this, [this]() { setCurrentPage(MainWindow::OVERLAY_PAGE); });

    Command* showUtilityPageCommand = userInterface.actionManager()->createCommand(ACTION_COMMAND_PANEL_UTILITIES, tr("Utilities"), {}, tr("Switches to the utilities tab."));
    connect(showUtilityPageCommand, &Command::triggered, this, [this]() { setCurrentPage(MainWindow::UTILITY_PAGE); });
}

/******************************************************************************
* Loads the layout of the widgets from the settings store.
******************************************************************************/
void CommandPanel::restoreLayout()
{
    _modifyPage->restoreLayout();
    _renderPage->restoreLayout();
    _overlayPage->restoreLayout();
}

/******************************************************************************
* Saves the layout of the widgets to the settings store.
******************************************************************************/
void CommandPanel::saveLayout()
{
    _modifyPage->saveLayout();
    _renderPage->saveLayout();
    _overlayPage->saveLayout();
}

}   // End of namespace
