// SPDX-FileCopyrightText: 2026 OVITO GmbH, Germany
// SPDX-License-Identifier: GPL-3.0-only OR MIT

#include <ovito/particles/gui/ParticlesGui.h>
#include <ovito/particles/modifier/visualization/AmbientOcclusionModifier.h>
#include <ovito/gui/desktop/properties/BooleanParameterUI.h>
#include <ovito/gui/desktop/properties/IntegerParameterUI.h>
#include <ovito/gui/desktop/properties/FloatParameterUI.h>
#include <ovito/gui/desktop/properties/ObjectStatusDisplay.h>
#include "AmbientOcclusionModifierEditor.h"

namespace Ovito {

IMPLEMENT_CREATABLE_OVITO_CLASS(AmbientOcclusionModifierEditor);
SET_OVITO_OBJECT_EDITOR(AmbientOcclusionModifier, AmbientOcclusionModifierEditor);

/******************************************************************************
* Sets up the UI widgets of the editor.
******************************************************************************/
void AmbientOcclusionModifierEditor::createUI(const RolloutInsertionParameters& rolloutParams)
{
    // Create a rollout.
    QWidget* rollout = createRollout(tr("Ambient occlusion"), rolloutParams, "manual:particles.modifiers.ambient_occlusion");

    // Create the rollout contents.
    QVBoxLayout* layout1 = new QVBoxLayout(rollout);
    layout1->setContentsMargins(4,4,4,4);
    layout1->setSpacing(4);

    QGridLayout* layout2 = new QGridLayout();
    layout2->setContentsMargins(0,0,0,0);
    layout2->setSpacing(4);
    layout2->setColumnStretch(1, 1);
    layout1->addLayout(layout2);

    // Intensity parameter.
    FloatParameterUI* intensityPUI = createParamUI<FloatParameterUI>(PROPERTY_FIELD(AmbientOcclusionModifier::intensity));
    layout2->addWidget(intensityPUI->label(), 0, 0);
    layout2->addLayout(intensityPUI->createFieldLayout(), 0, 1);

    // Sampling level parameter.
    IntegerParameterUI* samplingCountPUI = createParamUI<IntegerParameterUI>(PROPERTY_FIELD(AmbientOcclusionModifier::samplingCount));
    layout2->addWidget(samplingCountPUI->label(), 1, 0);
    layout2->addLayout(samplingCountPUI->createFieldLayout(), 1, 1);

    // Buffer resolution parameter.
    IntegerParameterUI* bufferResPUI = createParamUI<IntegerParameterUI>(PROPERTY_FIELD(AmbientOcclusionModifier::bufferResolution));
    layout2->addWidget(bufferResPUI->label(), 2, 0);
    layout2->addLayout(bufferResPUI->createFieldLayout(), 2, 1);

    // Status label.
    layout1->addSpacing(10);
    layout1->addWidget(createParamUI<ObjectStatusDisplay>()->statusWidget());
}

}   // End of namespace
