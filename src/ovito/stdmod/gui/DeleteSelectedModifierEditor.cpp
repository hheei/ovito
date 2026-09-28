// SPDX-FileCopyrightText: 2026 OVITO GmbH, Germany
// SPDX-License-Identifier: GPL-3.0-only OR MIT

#include <ovito/stdmod/gui/StdModGui.h>
#include <ovito/gui/desktop/properties/ModifierDelegateFixedListParameterUI.h>
#include <ovito/gui/desktop/properties/ObjectStatusDisplay.h>
#include <ovito/stdmod/modifiers/DeleteSelectedModifier.h>
#include "DeleteSelectedModifierEditor.h"

namespace Ovito {

IMPLEMENT_CREATABLE_OVITO_CLASS(DeleteSelectedModifierEditor);
SET_OVITO_OBJECT_EDITOR(DeleteSelectedModifier, DeleteSelectedModifierEditor);

/******************************************************************************
* Sets up the UI widgets of the editor.
******************************************************************************/
void DeleteSelectedModifierEditor::createUI(const RolloutInsertionParameters& rolloutParams)
{
    // Create a rollout.
    QWidget* rollout = createRollout(tr("Delete selected"), rolloutParams, "manual:particles.modifiers.delete_selected");

    // Create the rollout contents.
    QVBoxLayout* layout = new QVBoxLayout(rollout);
    layout->setContentsMargins(4,4,4,4);
    layout->setSpacing(8);

    ModifierDelegateFixedListParameterUI* delegatesPUI = createParamUI<ModifierDelegateFixedListParameterUI>();
    layout->addWidget(delegatesPUI->containerWidget());

    // Status label.
    layout->addWidget(createParamUI<ObjectStatusDisplay>()->statusWidget());
}

}   // End of namespace
