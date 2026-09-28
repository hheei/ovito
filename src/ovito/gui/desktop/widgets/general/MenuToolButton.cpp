// SPDX-FileCopyrightText: 2026 OVITO GmbH, Germany
// SPDX-License-Identifier: GPL-3.0-only OR MIT


#include "MenuToolButton.h"

namespace Ovito {

/******************************************************************************
* Constructor.
******************************************************************************/
MenuToolButton::MenuToolButton(QWidget* parent) : QToolButton(parent), _menu(new QMenu(this))
{
    OVITO_ASSERT(_menu != nullptr);
    setStyleSheet(
        "QToolButton { padding: 0px; margin: 0px; border: none; background-color: transparent; } "
        "QToolButton::menu-indicator { image: none; } ");
    setPopupMode(QToolButton::InstantPopup);
    setIcon(QIcon::fromTheme("edit_pipeline_menu"));
    setMenu(_menu);
}

/******************************************************************************
* Creates a new action and adds it to the ToolButtonMenu.
******************************************************************************/
QAction* MenuToolButton::createAction(const QIcon& icon, const QString& label)
{
    return _menu->addAction(icon, label);
}

/******************************************************************************
* Creates a new separator in the menu.
******************************************************************************/
void MenuToolButton::createMenuSeparator()
{
    _menu->addSeparator();
}

}  // namespace Ovito