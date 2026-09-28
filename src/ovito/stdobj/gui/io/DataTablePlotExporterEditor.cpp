// SPDX-FileCopyrightText: 2026 OVITO GmbH, Germany
// SPDX-License-Identifier: GPL-3.0-only OR MIT

#include <ovito/stdobj/gui/StdObjGui.h>
#include <ovito/stdobj/gui/io/DataTablePlotExporter.h>
#include <ovito/gui/desktop/properties/FloatParameterUI.h>
#include <ovito/gui/desktop/properties/IntegerParameterUI.h>
#include "DataTablePlotExporterEditor.h"

namespace Ovito {

IMPLEMENT_CREATABLE_OVITO_CLASS(DataTablePlotExporterEditor);
SET_OVITO_OBJECT_EDITOR(DataTablePlotExporter, DataTablePlotExporterEditor);

/******************************************************************************
* Sets up the UI widgets of the editor.
******************************************************************************/
void DataTablePlotExporterEditor::createUI(const RolloutInsertionParameters& rolloutParams)
{
    // Create a rollout.
    QWidget* rollout = createRollout(tr("Plot options"), rolloutParams);

    // Create the rollout contents.
    QGridLayout* layout = new QGridLayout(rollout);
    layout->setContentsMargins(4,4,4,4);
    layout->setSpacing(4);
    layout->setColumnStretch(1,1);
    layout->setColumnStretch(4,1);
    layout->setColumnMinimumWidth(2,10);

    FloatParameterUI* plotWidthUI = createParamUI<FloatParameterUI>(PROPERTY_FIELD(DataTablePlotExporter::plotWidth));
    layout->addWidget(plotWidthUI->label(), 0, 0);
    layout->addLayout(plotWidthUI->createFieldLayout(), 0, 1);

    FloatParameterUI* plotHeightUI = createParamUI<FloatParameterUI>(PROPERTY_FIELD(DataTablePlotExporter::plotHeight));
    layout->addWidget(plotHeightUI->label(), 1, 0);
    layout->addLayout(plotHeightUI->createFieldLayout(), 1, 1);

    IntegerParameterUI* dpiUI = createParamUI<IntegerParameterUI>(PROPERTY_FIELD(DataTablePlotExporter::plotDPI));
    layout->addWidget(dpiUI->label(), 0, 3);
    layout->addLayout(dpiUI->createFieldLayout(), 0, 4);
}

}   // End of namespace
