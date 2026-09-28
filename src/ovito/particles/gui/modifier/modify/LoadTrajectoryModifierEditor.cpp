// SPDX-FileCopyrightText: 2026 OVITO GmbH, Germany
// SPDX-License-Identifier: GPL-3.0-only OR MIT

#include <ovito/particles/gui/ParticlesGui.h>
#include <ovito/particles/modifier/modify/LoadTrajectoryModifier.h>
#include <ovito/gui/desktop/properties/SubObjectParameterUI.h>
#include <ovito/gui/desktop/properties/ObjectStatusDisplay.h>
#include "LoadTrajectoryModifierEditor.h"

namespace Ovito {

IMPLEMENT_CREATABLE_OVITO_CLASS(LoadTrajectoryModifierEditor);
SET_OVITO_OBJECT_EDITOR(LoadTrajectoryModifier, LoadTrajectoryModifierEditor);

/******************************************************************************
* Sets up the UI widgets of the editor.
******************************************************************************/
void LoadTrajectoryModifierEditor::createUI(const RolloutInsertionParameters& rolloutParams)
{
    // Create a rollout.
    QWidget* rollout = createRollout(tr("Load Trajectory"), rolloutParams, "manual:particles.modifiers.load_trajectory");

    // Create the rollout contents.
    QVBoxLayout* layout = new QVBoxLayout(rollout);
    layout->setContentsMargins(4,4,4,4);
    layout->setSpacing(4);

    // Status label.
    layout->addWidget(createParamUI<ObjectStatusDisplay>()->statusWidget());

    // Open a sub-editor for the source object.
    createParamUI<SubObjectParameterUI>(PROPERTY_FIELD(LoadTrajectoryModifier::trajectorySource), RolloutInsertionParameters().setTitle(tr("Trajectory Source: %1")));
}

}   // End of namespace
