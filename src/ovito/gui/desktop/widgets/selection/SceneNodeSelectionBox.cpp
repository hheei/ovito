// SPDX-FileCopyrightText: 2026 OVITO GmbH, Germany
// SPDX-License-Identifier: GPL-3.0-only OR MIT

#include <ovito/gui/desktop/GUI.h>
#include <ovito/gui/desktop/mainwin/MainWindow.h>
#include <ovito/gui/desktop/widgets/general/ActionsItemDelegate.h>
#include "SceneNodeSelectionBox.h"
#include "SceneNodesListModel.h"

namespace Ovito {

/******************************************************************************
* Constructs the widget.
******************************************************************************/
SceneNodeSelectionBox::SceneNodeSelectionBox(MainWindowUI& ui, QWidget* parent) : QComboBox(parent), UserInterfaceComponent<MainWindowUI>(ui)
{
    setInsertPolicy(QComboBox::NoInsert);
    setEditable(false);
#ifndef Q_OS_MACOS
    setMinimumContentsLength(40);
#else
    setMinimumContentsLength(32);
#endif
    setSizeAdjustPolicy(QComboBox::AdjustToMinimumContentsLengthWithIcon);
    setToolTip(tr("Pipeline selector"));
    setIconSize(QSize(24, 24));

    // Set the list model, which tracks the list of pipelines in the scene.
    setModel(new SceneNodesListModel(ui, this));

    // Wire the combobox selection to the list model.
    connect(this, qOverload<int>(&QComboBox::activated), static_cast<SceneNodesListModel*>(model()), &SceneNodesListModel::activateItem);
    connect(static_cast<SceneNodesListModel*>(model()), &SceneNodesListModel::selectionChangeRequested, this, &QComboBox::setCurrentIndex);

    // Configure the view.
    view()->setTextElideMode(Qt::ElideRight);

    // Install custom item delegate to add action buttons to the combo box.
    ActionsItemDelegate* delegate = new ActionsItemDelegate(this, SceneNodesListModel::InfoRole, SceneNodesListModel::ActionsRole);
    setItemDelegate(delegate);
}

}   // End of namespace
