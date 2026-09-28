// SPDX-FileCopyrightText: 2026 OVITO GmbH, Germany
// SPDX-License-Identifier: GPL-3.0-only OR MIT

#include <ovito/particles/gui/ParticlesGui.h>
#include <ovito/particles/modifier/modify/WrapPeriodicImagesModifier.h>
#include "WrapPeriodicImagesModifierEditor.h"

namespace Ovito {

IMPLEMENT_CREATABLE_OVITO_CLASS(WrapPeriodicImagesModifierEditor);
SET_OVITO_OBJECT_EDITOR(WrapPeriodicImagesModifier, WrapPeriodicImagesModifierEditor);

/******************************************************************************
* Sets up the UI widgets of the editor.
******************************************************************************/
void WrapPeriodicImagesModifierEditor::createUI(const RolloutInsertionParameters& rolloutParams)
{
    // Create a rollout.
    QWidget* rollout = createRollout(tr("Wrap at periodic boundaries"), rolloutParams, "manual:particles.modifiers.wrap_at_periodic_boundaries");

    // Create the rollout contents.
    QVBoxLayout* layout = new QVBoxLayout(rollout);
    layout->setContentsMargins(4,4,4,4);
    layout->setSpacing(6);

    layout->addWidget(new QLabel(tr("This modifier has no adjustable parameters")));
}

}   // End of namespace
