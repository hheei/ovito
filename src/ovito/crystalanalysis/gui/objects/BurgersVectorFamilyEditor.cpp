// SPDX-FileCopyrightText: 2026 OVITO GmbH, Germany
// SPDX-License-Identifier: GPL-3.0-only OR MIT

#include <ovito/crystalanalysis/CrystalAnalysis.h>
#include <ovito/crystalanalysis/objects/BurgersVectorFamily.h>
#include <ovito/gui/desktop/properties/ColorParameterUI.h>
#include <ovito/gui/desktop/properties/StringParameterUI.h>
#include "BurgersVectorFamilyEditor.h"

namespace Ovito {

IMPLEMENT_CREATABLE_OVITO_CLASS(BurgersVectorFamilyEditor);
SET_OVITO_OBJECT_EDITOR(BurgersVectorFamily, BurgersVectorFamilyEditor);

/******************************************************************************
* Sets up the UI widgets of the editor.
******************************************************************************/
void BurgersVectorFamilyEditor::createUI(const RolloutInsertionParameters& rolloutParams)
{
    // Create a rollout.
    QWidget* rollout = createRollout(tr("Burgers vector family"), rolloutParams);

    // Create the rollout contents.
    QGridLayout* layout1 = new QGridLayout(rollout);
    layout1->setContentsMargins(4,4,4,4);
    layout1->setSpacing(4);
    layout1->setColumnStretch(1, 1);

    // Text box for the name of type.
    StringParameterUI* namePUI = createParamUI<StringParameterUI>(PROPERTY_FIELD(BurgersVectorFamily::name));
    layout1->addWidget(new QLabel(tr("Name:")), 0, 0);
    layout1->addWidget(namePUI->textBox(), 0, 1);

    // Color parameter.
    ColorParameterUI* colorPUI = createParamUI<ColorParameterUI>(PROPERTY_FIELD(BurgersVectorFamily::color));
    layout1->addWidget(colorPUI->label(), 1, 0);
    layout1->addWidget(colorPUI->colorPicker(), 1, 1);
}

}   // End of namespace
