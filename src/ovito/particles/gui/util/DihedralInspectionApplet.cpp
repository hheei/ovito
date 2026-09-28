// SPDX-FileCopyrightText: 2026 OVITO GmbH, Germany
// SPDX-License-Identifier: GPL-3.0-only OR MIT

#include <ovito/particles/gui/ParticlesGui.h>
#include <ovito/gui/desktop/mainwin/MainWindow.h>
#include <ovito/gui/desktop/widgets/general/AutocompleteLineEdit.h>
#include "DihedralInspectionApplet.h"

namespace Ovito {

IMPLEMENT_CREATABLE_OVITO_CLASS(DihedralInspectionApplet);
OVITO_CLASSINFO(DihedralInspectionApplet, "DisplayName", "Dihedrals");

/******************************************************************************
* Lets the applet create the UI widget that is to be placed into the data
* inspector panel.
******************************************************************************/
QWidget* DihedralInspectionApplet::createWidget()
{
    createBaseWidgets();

    QWidget* panel = new QWidget();
    QGridLayout* layout = new QGridLayout(panel);
    layout->setContentsMargins(0,0,0,0);
    layout->setSpacing(0);

    layout->addWidget(filterExpressionEdit(), 0, 0);
    layout->addWidget(countDisplayLabel(), 0, 1);
    layout->addWidget(tableView(), 1, 0, 1, 2);
    layout->setRowStretch(1, 1);
    layout->setColumnStretch(0, 1);
    tableView()->setAccessibleName(tr("Dihedral property table"));

    return panel;
}

}   // End of namespace
