// SPDX-FileCopyrightText: 2026 OVITO GmbH, Germany
// SPDX-License-Identifier: GPL-3.0-only OR MIT

#include <ovito/stdobj/gui/StdObjGui.h>
#include <ovito/stdobj/StdObj.h>
#include <ovito/gui/base/actions/ViewportModeAction.h>
#include <ovito/gui/desktop/mainwin/MainWindow.h>
#include <ovito/gui/desktop/widgets/general/AutocompleteLineEdit.h>
#include <ovito/gui/desktop/mainwin/data_inspector/DataInspectorPanel.h>
#include "LinesInspectionApplet.h"

namespace Ovito {

IMPLEMENT_CREATABLE_OVITO_CLASS(LinesInspectionApplet);
OVITO_CLASSINFO(LinesInspectionApplet, "DisplayName", "Lines");

/******************************************************************************
 * Lets the applet create the UI widget that is to be placed into the data
 * inspector panel.
 ******************************************************************************/
QWidget* LinesInspectionApplet::createWidget()
{
    createBaseWidgets();

    QSplitter* splitter = new QSplitter();
    // Side panel to select between different line objects
    splitter->addWidget(objectSelectionWidget());

    QWidget* panel = new QWidget();
    QGridLayout* layout = new QGridLayout(panel);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);

    // Filter Expression Box
    layout->addWidget(filterExpressionEdit(), 0, 1);
    // Data table view
    layout->addWidget(tableView(), 1, 0, 1, 2);
    tableView()->setAccessibleName(tr("Line property table"));

    layout->setRowStretch(1, 1);
    splitter->addWidget(panel);
    splitter->setStretchFactor(0, 1);
    splitter->setStretchFactor(1, 3);

    return splitter;
}

}  // namespace Ovito
