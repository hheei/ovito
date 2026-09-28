
// SPDX-FileCopyrightText: 2026 OVITO GmbH, Germany
// SPDX-License-Identifier: GPL-3.0-only OR MIT

#pragma once

#include <ovito/gui/desktop/GUI.h>

namespace Ovito {

/**
 * A drop-down menu for selecting an action.
 */
class OVITO_GUI_EXPORT MenuToolButton : public QToolButton
{
    Q_OBJECT

public:
    /// \brief Constructs the ToolButtonMenu.
    /// \param parent The parent widget for the ToolButtonMenu.
    MenuToolButton(QWidget* parent = nullptr);

    /// Creates a new action and adds it to the ToolButtonMenu.
    /// \param icon Icon for the action.
    /// \param label Human readable label for the action.
    /// \return The pointer to the QAction that was added to the menu.
    QAction* createAction(const QIcon& icon, const QString& label);

    /// \brief Creates a new separator in the menu.
    void createMenuSeparator();

private:

    QPointer<QMenu> _menu = nullptr;
};

}  // namespace Ovito