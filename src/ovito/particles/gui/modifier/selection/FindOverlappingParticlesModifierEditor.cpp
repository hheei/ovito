////////////////////////////////////////////////////////////////////////////////////////
//
//  Copyright 2026 OVITO GmbH, Germany
//
//  This file is part of OVITO (Open Visualization Tool).
//
//  OVITO is free software; you can redistribute it and/or modify it either under the
//  terms of the GNU General Public License version 3 as published by the Free Software
//  Foundation (the "GPL") or, at your option, under the terms of the MIT License.
//  If you do not alter this notice, a recipient may use your version of this
//  file under either the GPL or the MIT License.
//
//  You should have received a copy of the GPL along with this program in a
//  file LICENSE.GPL.txt.  You should have received a copy of the MIT License along
//  with this program in a file LICENSE.MIT.txt
//
//  This software is distributed on an "AS IS" basis, WITHOUT WARRANTY OF ANY KIND,
//  either express or implied. See the GPL or the MIT License for the specific language
//  governing rights and limitations.
//
////////////////////////////////////////////////////////////////////////////////////////

#include <ovito/particles/gui/ParticlesGui.h>
#include <ovito/gui/desktop/properties/FloatParameterUI.h>
#include <ovito/gui/desktop/properties/BooleanParameterUI.h>
#include <ovito/gui/desktop/properties/ObjectStatusDisplay.h>
#include <ovito/particles/modifier/selection/FindOverlappingParticlesModifier.h>
#include "FindOverlappingParticlesModifierEditor.h"

namespace Ovito {

IMPLEMENT_CREATABLE_OVITO_CLASS(FindOverlappingParticlesModifierEditor);
SET_OVITO_OBJECT_EDITOR(FindOverlappingParticlesModifier, FindOverlappingParticlesModifierEditor);

/******************************************************************************
 * Sets up the UI widgets of the editor.
 ******************************************************************************/
void FindOverlappingParticlesModifierEditor::createUI(const RolloutInsertionParameters& rolloutParams)
{
    // Create a rollout.
    QWidget* rollout =
        createRollout(tr("Find overlapping particles"), rolloutParams, "manual:particles.modifiers.find_overlapping_particles");

    // Create the rollout contents.
    QVBoxLayout* layout = new QVBoxLayout(rollout);
    layout->setContentsMargins(4, 4, 4, 4);
    layout->setSpacing(6);

    // Overlap distance parameter.
    auto* overlapDistancePUI = createParamUI<FloatParameterUI>(PROPERTY_FIELD(FindOverlappingParticlesModifier::overlapDistance));
    QHBoxLayout* sublayout = new QHBoxLayout();
    sublayout->setContentsMargins(0, 0, 0, 0);
    sublayout->addWidget(overlapDistancePUI->label());
    sublayout->addLayout(overlapDistancePUI->createFieldLayout(), 1);
    layout->addLayout(sublayout);

    // Apply to selection parameter.
    auto* booleanPUI = createParamUI<BooleanParameterUI>(PROPERTY_FIELD(FindOverlappingParticlesModifier::applyToSelection));
    layout->addWidget(booleanPUI->checkBox());

    // Keep one unselected parameter.
    booleanPUI = createParamUI<BooleanParameterUI>(PROPERTY_FIELD(FindOverlappingParticlesModifier::keepOne));
    layout->addWidget(booleanPUI->checkBox());

    // Move to mean position parameter.
    booleanPUI = createParamUI<BooleanParameterUI>(PROPERTY_FIELD(FindOverlappingParticlesModifier::moveToMeanPosition));
    layout->addWidget(booleanPUI->checkBox());

    // Status label.
    layout->addSpacing(10);
    layout->addWidget(createParamUI<ObjectStatusDisplay>()->statusWidget());
}

}  // namespace Ovito
