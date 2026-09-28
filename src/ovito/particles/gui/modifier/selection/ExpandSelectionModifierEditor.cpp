// SPDX-FileCopyrightText: 2026 OVITO GmbH, Germany
// SPDX-License-Identifier: GPL-3.0-only OR MIT

#include <ovito/particles/gui/ParticlesGui.h>
#include <ovito/particles/modifier/selection/ExpandSelectionModifier.h>
#include <ovito/gui/desktop/properties/IntegerRadioButtonParameterUI.h>
#include <ovito/gui/desktop/properties/FloatParameterUI.h>
#include <ovito/gui/desktop/properties/IntegerParameterUI.h>
#include <ovito/gui/desktop/properties/ObjectStatusDisplay.h>
#include "ExpandSelectionModifierEditor.h"

namespace Ovito {

IMPLEMENT_CREATABLE_OVITO_CLASS(ExpandSelectionModifierEditor);
SET_OVITO_OBJECT_EDITOR(ExpandSelectionModifier, ExpandSelectionModifierEditor);

/******************************************************************************
* Sets up the UI widgets of the editor.
******************************************************************************/
void ExpandSelectionModifierEditor::createUI(const RolloutInsertionParameters& rolloutParams)
{
    // Create a rollout.
    QWidget* rollout = createRollout(tr("Expand selection"), rolloutParams, "manual:particles.modifiers.expand_selection");

    // Create the rollout contents.
    auto* layout = new QVBoxLayout(rollout);
    layout->setContentsMargins(4,4,4,4);
    layout->setSpacing(6);

    auto* modeGroupBox = new QGroupBox(tr("Expansion mode"));
    layout->addWidget(modeGroupBox);

    auto* groupBoxLayout = new QVBoxLayout(modeGroupBox);
    groupBoxLayout->setContentsMargins(4, 4, 4, 4);

    QLabel* label = new QLabel(tr("Expand current selection to include particles that are..."));
    label->setWordWrap(true);
    groupBoxLayout->addWidget(label);

    IntegerRadioButtonParameterUI* modePUI = createParamUI<IntegerRadioButtonParameterUI>(PROPERTY_FIELD(ExpandSelectionModifier::mode));
    QRadioButton* cutoffModeBtn = modePUI->addRadioButton(ExpandSelectionModifier::CutoffRange, tr("... within the range:"));
    // groupBoxLayout->addSpacing(10);
    groupBoxLayout->addWidget(cutoffModeBtn);

    // Cutoff parameter.
    FloatParameterUI* cutoffRadiusPUI = createParamUI<FloatParameterUI>(PROPERTY_FIELD(ExpandSelectionModifier::cutoffRange));
    QHBoxLayout* sublayout = new QHBoxLayout();
    sublayout->setContentsMargins(0,0,0,0);
    sublayout->addSpacing(20);
    sublayout->addWidget(cutoffRadiusPUI->label());
    sublayout->addLayout(cutoffRadiusPUI->createFieldLayout(), 1);
    groupBoxLayout->addLayout(sublayout);
    cutoffRadiusPUI->setEnabled(false);
    connect(cutoffModeBtn, &QRadioButton::toggled, cutoffRadiusPUI, &FloatParameterUI::setEnabled);

    QRadioButton* nearestNeighborsModeBtn = modePUI->addRadioButton(ExpandSelectionModifier::NearestNeighbors, tr("... among the N nearest neighbors:"));
    // groupBoxLayout->addSpacing(10);
    groupBoxLayout->addWidget(nearestNeighborsModeBtn);

    // Number of nearest neighbors.
    IntegerParameterUI* numNearestNeighborsPUI = createParamUI<IntegerParameterUI>(PROPERTY_FIELD(ExpandSelectionModifier::numNearestNeighbors));
    sublayout = new QHBoxLayout();
    sublayout->setContentsMargins(0,0,0,0);
    sublayout->addSpacing(20);
    sublayout->addWidget(numNearestNeighborsPUI->label());
    sublayout->addLayout(numNearestNeighborsPUI->createFieldLayout(), 1);
    groupBoxLayout->addLayout(sublayout);
    numNearestNeighborsPUI->setEnabled(false);
    connect(nearestNeighborsModeBtn, &QRadioButton::toggled, numNearestNeighborsPUI, &FloatParameterUI::setEnabled);

    QRadioButton* bondModeBtn = modePUI->addRadioButton(ExpandSelectionModifier::BondedNeighbors, tr("... bonded to a selected particle."));
    // groupBoxLayout->addSpacing(10);
    groupBoxLayout->addWidget(bondModeBtn);

    // Create a radio button for the "Molecule" mode.
    QRadioButton* moleculeModeBtn = modePUI->addRadioButton(ExpandSelectionModifier::Molecule, tr("... in the same molecule."));
    groupBoxLayout->addSpacing(8);
    groupBoxLayout->addWidget(moleculeModeBtn);

    auto* settingsGroupBox = new QGroupBox(tr("General settings"));
    settingsGroupBox->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Maximum);
    layout->addWidget(settingsGroupBox);

    groupBoxLayout = new QVBoxLayout(settingsGroupBox);
    groupBoxLayout->setContentsMargins(4, 4, 4, 4);
    groupBoxLayout->setSpacing(0);

    IntegerParameterUI* numIterationsPUI = createParamUI<IntegerParameterUI>(PROPERTY_FIELD(ExpandSelectionModifier::numberOfIterations));
    sublayout = new QHBoxLayout();
    sublayout->setContentsMargins(0,0,0,0);
    sublayout->addWidget(numIterationsPUI->label());
    sublayout->addLayout(numIterationsPUI->createFieldLayout(), 1);
    groupBoxLayout->addLayout(sublayout);

    // Disable the number of iterations parameter when the molecule mode is selected as it is unused.
    connect(moleculeModeBtn, &QRadioButton::toggled, settingsGroupBox, &QGroupBox::setDisabled);

    // Status label.
    layout->addSpacing(4);
    layout->addWidget(createParamUI<ObjectStatusDisplay>()->statusWidget());
}

}   // End of namespace
