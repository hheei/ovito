// SPDX-FileCopyrightText: 2026 OVITO GmbH, Germany
// SPDX-License-Identifier: GPL-3.0-only OR MIT

#include <ovito/particles/gui/ParticlesGui.h>
#include <ovito/particles/modifier/analysis/diamond/IdentifyDiamondModifier.h>
#include <ovito/particles/gui/modifier/analysis/StructureListParameterUI.h>
#include <ovito/gui/desktop/properties/BooleanParameterUI.h>
#include <ovito/gui/desktop/properties/ObjectStatusDisplay.h>
#include "IdentifyDiamondModifierEditor.h"

namespace Ovito {

IMPLEMENT_CREATABLE_OVITO_CLASS(IdentifyDiamondModifierEditor);
SET_OVITO_OBJECT_EDITOR(IdentifyDiamondModifier, IdentifyDiamondModifierEditor);

/******************************************************************************
* Sets up the UI widgets of the editor.
******************************************************************************/
void IdentifyDiamondModifierEditor::createUI(const RolloutInsertionParameters& rolloutParams)
{
    // Create a rollout.
    QWidget* rollout = createRollout(tr("Identify diamond structure"), rolloutParams, "manual:particles.modifiers.identify_diamond_structure");

    // Create the rollout contents.
    QVBoxLayout* layout1 = new QVBoxLayout(rollout);
    layout1->setContentsMargins(4,4,4,4);
    layout1->setSpacing(6);

    // Use only selected particles.
    BooleanParameterUI* onlySelectedParticlesUI = createParamUI<BooleanParameterUI>(PROPERTY_FIELD(StructureIdentificationModifier::onlySelectedParticles));
    layout1->addWidget(onlySelectedParticlesUI->checkBox());

    // Color by type
    BooleanParameterUI* colorByTypeUI = createParamUI<BooleanParameterUI>(PROPERTY_FIELD(StructureIdentificationModifier::colorByType));
    layout1->addWidget(colorByTypeUI->checkBox());

    // Status label.
    layout1->addWidget(createParamUI<ObjectStatusDisplay>()->statusWidget());

    StructureListParameterUI* structureTypesPUI = createParamUI<StructureListParameterUI>(this);
    layout1->addSpacing(10);
    layout1->addWidget(new QLabel(tr("Structure types:")));
    layout1->addWidget(structureTypesPUI->tableWidget());
    layout1->addWidget(structureTypesPUI->createNotesLabel());
}

}   // End of namespace
