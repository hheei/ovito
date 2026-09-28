// SPDX-FileCopyrightText: 2026 OVITO GmbH, Germany
// SPDX-License-Identifier: GPL-3.0-only OR MIT

#include <ovito/particles/gui/ParticlesGui.h>
#include <ovito/particles/gui/modifier/analysis/StructureListParameterUI.h>
#include <ovito/particles/modifier/analysis/ackland_jones/AcklandJonesModifier.h>
#include <ovito/gui/desktop/properties/BooleanParameterUI.h>
#include <ovito/gui/desktop/properties/ObjectStatusDisplay.h>
#include "AcklandJonesModifierEditor.h"

namespace Ovito {

IMPLEMENT_CREATABLE_OVITO_CLASS(AcklandJonesModifierEditor);
SET_OVITO_OBJECT_EDITOR(AcklandJonesModifier, AcklandJonesModifierEditor);

/******************************************************************************
* Sets up the UI widgets of the editor.
******************************************************************************/
void AcklandJonesModifierEditor::createUI(const RolloutInsertionParameters& rolloutParams)
{
    // Create a rollout.
    QWidget* rollout = createRollout(tr("Ackland-Jones analysis"), rolloutParams, "manual:particles.modifiers.bond_angle_analysis");

    // Create the rollout contents.
    QVBoxLayout* layout1 = new QVBoxLayout(rollout);
    layout1->setContentsMargins(4,4,4,4);
    layout1->setSpacing(4);

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
