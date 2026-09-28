// SPDX-FileCopyrightText: 2026 OVITO GmbH, Germany
// SPDX-License-Identifier: GPL-3.0-only OR MIT

#include <ovito/particles/gui/ParticlesGui.h>
#include <ovito/particles/gui/modifier/analysis/StructureListParameterUI.h>
#include <ovito/particles/modifier/analysis/chill_plus/ChillPlusModifier.h>
#include <ovito/gui/desktop/properties/FloatParameterUI.h>
#include <ovito/gui/desktop/properties/BooleanParameterUI.h>
#include <ovito/gui/desktop/properties/ObjectStatusDisplay.h>
#include "ChillPlusModifierEditor.h"

namespace Ovito {

IMPLEMENT_CREATABLE_OVITO_CLASS(ChillPlusModifierEditor);
SET_OVITO_OBJECT_EDITOR(ChillPlusModifier, ChillPlusModifierEditor);

/******************************************************************************
* Sets up the UI widgets of the editor.
******************************************************************************/
void ChillPlusModifierEditor::createUI(const RolloutInsertionParameters& rolloutParams)
{
    // Create a rollout.
    QWidget* rollout = createRollout(tr("Chill+"), rolloutParams, "manual:particles.modifiers.chill_plus");

    // Create the rollout contents.
    QVBoxLayout* layout1 = new QVBoxLayout(rollout);
    layout1->setContentsMargins(4,4,4,4);
    layout1->setSpacing(4);

    QGridLayout* gridlayout = new QGridLayout();

    // Cutoff
    FloatParameterUI* cutoffRadiusPUI = createParamUI<FloatParameterUI>(PROPERTY_FIELD(ChillPlusModifier::cutoff));
    gridlayout->addWidget(cutoffRadiusPUI->label(), 0, 0);
    gridlayout->addLayout(cutoffRadiusPUI->createFieldLayout(), 0, 1);
    layout1->addLayout(gridlayout);

    // Use only selected particles.
    BooleanParameterUI* onlySelectedParticlesUI = createParamUI<BooleanParameterUI>(PROPERTY_FIELD(StructureIdentificationModifier::onlySelectedParticles));
    layout1->addWidget(onlySelectedParticlesUI->checkBox());

    // Color by type
    BooleanParameterUI* colorByTypeUI = createParamUI<BooleanParameterUI>(PROPERTY_FIELD(StructureIdentificationModifier::colorByType));
    layout1->addWidget(colorByTypeUI->checkBox());

    // Status label.
    layout1->addSpacing(10);
    layout1->addWidget(createParamUI<ObjectStatusDisplay>()->statusWidget());

    StructureListParameterUI* structureTypesPUI = createParamUI<StructureListParameterUI>(true);
    layout1->addSpacing(10);
    layout1->addWidget(new QLabel(tr("Structure types:")));
    layout1->addWidget(structureTypesPUI->tableWidget());
    layout1->addWidget(structureTypesPUI->createNotesLabel());
}

}   // End of namespace
