// SPDX-FileCopyrightText: 2026 OVITO GmbH, Germany
// SPDX-License-Identifier: GPL-3.0-only OR MIT

#include <ovito/particles/gui/ParticlesGui.h>
#include <ovito/particles/modifier/modify/UnwrapTrajectoriesModifier.h>
#include <ovito/gui/desktop/properties/ObjectStatusDisplay.h>
#include "UnwrapTrajectoriesModifierEditor.h"

namespace Ovito {

IMPLEMENT_CREATABLE_OVITO_CLASS(UnwrapTrajectoriesModifierEditor);
SET_OVITO_OBJECT_EDITOR(UnwrapTrajectoriesModifier, UnwrapTrajectoriesModifierEditor);

/******************************************************************************
* Sets up the UI widgets of the editor.
******************************************************************************/
void UnwrapTrajectoriesModifierEditor::createUI(const RolloutInsertionParameters& rolloutParams)
{
    // Create a rollout.
    QWidget* rollout = createRollout(tr("Unwrap trajectories"), rolloutParams, "manual:particles.modifiers.unwrap_trajectories");

    // Create the rollout contents.
    QVBoxLayout* layout = new QVBoxLayout(rollout);
    layout->setContentsMargins(4,4,4,4);
    layout->setSpacing(6);

    // Status label.
    layout->addWidget(createParamUI<ObjectStatusDisplay>()->statusWidget());
    layout->addSpacing(6);
}

}   // End of namespace
