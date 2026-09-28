// SPDX-FileCopyrightText: 2026 OVITO GmbH, Germany
// SPDX-License-Identifier: GPL-3.0-only OR MIT

#include <ovito/stdobj/gui/StdObjGui.h>
#include <ovito/stdobj/simcell/SimulationCellVis.h>
#include <ovito/gui/desktop/properties/FloatParameterUI.h>
#include <ovito/gui/desktop/properties/ColorParameterUI.h>
#include <ovito/gui/desktop/properties/BooleanParameterUI.h>
#include "SimulationCellVisEditor.h"

namespace Ovito {

IMPLEMENT_CREATABLE_OVITO_CLASS(SimulationCellVisEditor);
SET_OVITO_OBJECT_EDITOR(SimulationCellVis, SimulationCellVisEditor);

/******************************************************************************
* Sets up the UI widgets of the editor.
******************************************************************************/
void SimulationCellVisEditor::createUI(const RolloutInsertionParameters& rolloutParams)
{
    // Create a rollout.
    QWidget* rollout = createRollout(QString(), rolloutParams, "manual:visual_elements.simulation_cell");

    // Create the rollout contents.
    QGridLayout* layout = new QGridLayout(rollout);
    layout->setContentsMargins(4,4,4,4);
    layout->setSpacing(4);
    layout->setColumnStretch(1, 1);

    // Render cell
    BooleanParameterUI* renderCellUI = createParamUI<BooleanParameterUI>(PROPERTY_FIELD(SimulationCellVis::renderCellEnabled));
    layout->addWidget(renderCellUI->checkBox(), 0, 0, 1, 2);

    // Line width
    FloatParameterUI* lineWidthUI = createParamUI<FloatParameterUI>(PROPERTY_FIELD(SimulationCellVis::cellLineWidth));
    layout->addWidget(lineWidthUI->label(), 1, 0);
    layout->addLayout(lineWidthUI->createFieldLayout(), 1, 1);

    // Line color
    ColorParameterUI* lineColorUI = createParamUI<ColorParameterUI>(PROPERTY_FIELD(SimulationCellVis::cellColor));
    layout->addWidget(lineColorUI->label(), 2, 0);
    layout->addWidget(lineColorUI->colorPicker(), 2, 1);
}

}   // End of namespace
