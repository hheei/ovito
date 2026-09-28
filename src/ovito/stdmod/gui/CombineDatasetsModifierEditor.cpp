// SPDX-FileCopyrightText: 2026 OVITO GmbH, Germany
// SPDX-License-Identifier: GPL-3.0-only OR MIT

#include <ovito/stdmod/gui/StdModGui.h>
#include <ovito/stdmod/modifiers/CombineDatasetsModifier.h>
#include <ovito/gui/desktop/properties/ObjectStatusDisplay.h>
#include <ovito/gui/desktop/properties/SubObjectParameterUI.h>
#include "CombineDatasetsModifierEditor.h"

namespace Ovito {

IMPLEMENT_CREATABLE_OVITO_CLASS(CombineDatasetsModifierEditor);
SET_OVITO_OBJECT_EDITOR(CombineDatasetsModifier, CombineDatasetsModifierEditor);

/******************************************************************************
* Sets up the UI widgets of the editor.
******************************************************************************/
void CombineDatasetsModifierEditor::createUI(const RolloutInsertionParameters& rolloutParams)
{
    // Create a rollout.
    QWidget* rollout = createRollout(tr("Combine Datasets"), rolloutParams, "manual:particles.modifiers.combine_particle_sets");

    // Create the rollout contents.
    QVBoxLayout* layout = new QVBoxLayout(rollout);
    layout->setContentsMargins(4,4,4,4);
    layout->setSpacing(4);

    // Status label.
    layout->addSpacing(6);
    layout->addWidget(createParamUI<ObjectStatusDisplay>()->statusWidget());

    // Open a sub-editor for the source object.
    createParamUI<SubObjectParameterUI>(PROPERTY_FIELD(CombineDatasetsModifier::secondaryDataSource), RolloutInsertionParameters().setTitle(tr("Secondary Source: %1")));
}

}   // End of namespace
