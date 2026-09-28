// SPDX-FileCopyrightText: 2026 OVITO GmbH, Germany
// SPDX-License-Identifier: GPL-3.0-only OR MIT

#include <ovito/particles/gui/ParticlesGui.h>
#include <ovito/particles/modifier/properties/SmoothTrajectoryModifier.h>
#include <ovito/gui/desktop/properties/BooleanParameterUI.h>
#include <ovito/gui/desktop/properties/IntegerParameterUI.h>
#include <ovito/gui/desktop/properties/ObjectStatusDisplay.h>
#include "SmoothTrajectoryModifierEditor.h"

namespace Ovito {

IMPLEMENT_CREATABLE_OVITO_CLASS(SmoothTrajectoryModifierEditor);
SET_OVITO_OBJECT_EDITOR(SmoothTrajectoryModifier, SmoothTrajectoryModifierEditor);

/******************************************************************************
* Sets up the UI widgets of the editor.
******************************************************************************/
void SmoothTrajectoryModifierEditor::createUI(const RolloutInsertionParameters& rolloutParams)
{
    QWidget* rollout = createRollout(tr("Smooth trajectory"), rolloutParams, "manual:particles.modifiers.smooth_trajectory");

    // Create the rollout contents.
    QGridLayout* layout = new QGridLayout(rollout);
    layout->setContentsMargins(4,4,4,4);
    layout->setSpacing(4);
    layout->setColumnStretch(1, 1);

    // Smoothing window size parameter.
    IntegerParameterUI* smoothingWindowSizeUI = createParamUI<IntegerParameterUI>(PROPERTY_FIELD(SmoothTrajectoryModifier::smoothingWindowSize));
    layout->addWidget(smoothingWindowSizeUI->label(), 0, 0);
    layout->addLayout(smoothingWindowSizeUI->createFieldLayout(), 0, 1);

    BooleanParameterUI* useMinimumImageConventionUI = createParamUI<BooleanParameterUI>(PROPERTY_FIELD(SmoothTrajectoryModifier::useMinimumImageConvention));
    layout->addWidget(useMinimumImageConventionUI->checkBox(), 1, 0, 1, 2);

    // Status label.
    layout->setRowMinimumHeight(2, 8);
    layout->addWidget(createParamUI<ObjectStatusDisplay>()->statusWidget(), 3, 0, 1, 2);
}

}   // End of namespace
