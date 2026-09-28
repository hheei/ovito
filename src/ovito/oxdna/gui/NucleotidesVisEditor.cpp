// SPDX-FileCopyrightText: 2026 OVITO GmbH, Germany
// SPDX-License-Identifier: GPL-3.0-only OR MIT

#include <ovito/particles/gui/ParticlesGui.h>
#include <ovito/oxdna/NucleotidesVis.h>
#include <ovito/gui/desktop/properties/FloatParameterUI.h>
#include "NucleotidesVisEditor.h"

namespace Ovito {

IMPLEMENT_CREATABLE_OVITO_CLASS(NucleotidesVisEditor);
SET_OVITO_OBJECT_EDITOR(NucleotidesVis, NucleotidesVisEditor);

/******************************************************************************
* Sets up the UI widgets of the editor.
******************************************************************************/
void NucleotidesVisEditor::createUI(const RolloutInsertionParameters& rolloutParams)
{
    // Create a rollout.
    QWidget* rollout = createRollout(tr("Nucleotide display"), rolloutParams);

    // Create the rollout contents.
    QGridLayout* layout = new QGridLayout(rollout);
    layout->setContentsMargins(4,4,4,4);
    layout->setSpacing(4);
    layout->setColumnStretch(1, 1);

    // Particle radius.
    FloatParameterUI* radiusUI = createParamUI<FloatParameterUI>(PROPERTY_FIELD(ParticlesVis::defaultParticleRadius));
    layout->addWidget(new QLabel(tr("Backbone centers radius:")), 0, 0);
    layout->addLayout(radiusUI->createFieldLayout(), 0, 1);

    // Cylinder radius.
    FloatParameterUI* cylinderRadiusUI = createParamUI<FloatParameterUI>(PROPERTY_FIELD(NucleotidesVis::cylinderRadius));
    layout->addWidget(cylinderRadiusUI->label(), 1, 0);
    layout->addLayout(cylinderRadiusUI->createFieldLayout(), 1, 1);
}

}   // End of namespace
