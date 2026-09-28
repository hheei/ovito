// SPDX-FileCopyrightText: 2026 OVITO GmbH, Germany
// SPDX-License-Identifier: GPL-3.0-only OR MIT

#include <ovito/particles/gui/ParticlesGui.h>
#include <ovito/particles/modifier/analysis/cluster/ClusterAnalysisModifier.h>
#include <ovito/gui/desktop/properties/BooleanParameterUI.h>
#include <ovito/gui/desktop/properties/FloatParameterUI.h>
#include <ovito/gui/desktop/properties/IntegerRadioButtonParameterUI.h>
#include <ovito/gui/desktop/properties/ObjectStatusDisplay.h>
#include <ovito/gui/desktop/properties/OpenDataInspectorButton.h>
#include <ovito/gui/desktop/mainwin/MainWindow.h>
#include "ClusterAnalysisModifierEditor.h"

namespace Ovito {

IMPLEMENT_CREATABLE_OVITO_CLASS(ClusterAnalysisModifierEditor);
SET_OVITO_OBJECT_EDITOR(ClusterAnalysisModifier, ClusterAnalysisModifierEditor);

/******************************************************************************
* Sets up the UI widgets of the editor.
******************************************************************************/
void ClusterAnalysisModifierEditor::createUI(const RolloutInsertionParameters& rolloutParams)
{
    // Create a rollout.
    QWidget* rollout = createRollout(tr("Cluster analysis"), rolloutParams, "manual:particles.modifiers.cluster_analysis");

    // Create the rollout contents.
    QVBoxLayout* layout = new QVBoxLayout(rollout);
    layout->setContentsMargins(4,4,4,4);
    layout->setSpacing(4);

    QGridLayout* gridlayout = new QGridLayout();
    gridlayout->setContentsMargins(4,4,4,4);
    gridlayout->setVerticalSpacing(6);
    gridlayout->setColumnStretch(2, 1);
    gridlayout->setColumnMinimumWidth(0, 10);
    gridlayout->setRowMinimumHeight(3, 6);

    gridlayout->addWidget(new QLabel(tr("Connectivity criterion:")), 0, 0, 1, 3);

    IntegerRadioButtonParameterUI* neighborModePUI = createParamUI<IntegerRadioButtonParameterUI>(PROPERTY_FIELD(ClusterAnalysisModifier::neighborMode));
    QRadioButton* cutoffModeBtn = neighborModePUI->addRadioButton(ClusterAnalysisModifier::CutoffRange, tr("Cutoff distance:"));
    gridlayout->addWidget(cutoffModeBtn, 1, 1);
    QRadioButton* bondModeBtn = neighborModePUI->addRadioButton(ClusterAnalysisModifier::Bonding, tr("Bonds"));
    gridlayout->addWidget(bondModeBtn, 2, 1, 1, 2);

    // Cutoff parameter.
    FloatParameterUI* cutoffRadiusPUI = createParamUI<FloatParameterUI>(PROPERTY_FIELD(ClusterAnalysisModifier::cutoff));
    gridlayout->addLayout(cutoffRadiusPUI->createFieldLayout(), 1, 2);
    cutoffRadiusPUI->setEnabled(false);
    connect(cutoffModeBtn, &QRadioButton::toggled, cutoffRadiusPUI, &FloatParameterUI::setEnabled);

    // Sort by size.
    BooleanParameterUI* sortBySizeUI = createParamUI<BooleanParameterUI>(PROPERTY_FIELD(ClusterAnalysisModifier::sortBySize));
    gridlayout->addWidget(sortBySizeUI->checkBox(), 4, 0, 1, 3);

    // Compute centers of mass.
    BooleanParameterUI* computeCentersOfMassUI = createParamUI<BooleanParameterUI>(PROPERTY_FIELD(ClusterAnalysisModifier::computeCentersOfMass));
    gridlayout->addWidget(computeCentersOfMassUI->checkBox(), 5, 0, 1, 3);

    // Compute centers of mass.
    BooleanParameterUI* computeRadiusOfGyrationUI = createParamUI<BooleanParameterUI>(PROPERTY_FIELD(ClusterAnalysisModifier::computeRadiusOfGyration));
    gridlayout->addWidget(computeRadiusOfGyrationUI->checkBox(), 6, 0, 1, 3);

    // Unwrap particle coordinates.
    BooleanParameterUI* unwrapParticleCoordinatesUI = createParamUI<BooleanParameterUI>(PROPERTY_FIELD(ClusterAnalysisModifier::unwrapParticleCoordinates));
    gridlayout->addWidget(unwrapParticleCoordinatesUI->checkBox(), 7, 0, 1, 3);

    // Color particles by cluster.
    BooleanParameterUI* colorParticlesByClusterUI = createParamUI<BooleanParameterUI>(PROPERTY_FIELD(ClusterAnalysisModifier::colorParticlesByCluster));
    gridlayout->addWidget(colorParticlesByClusterUI->checkBox(), 8, 0, 1, 3);

    // Use only selected particles.
    BooleanParameterUI* onlySelectedParticlesUI = createParamUI<BooleanParameterUI>(PROPERTY_FIELD(ClusterAnalysisModifier::onlySelectedParticles));
    gridlayout->addWidget(onlySelectedParticlesUI->checkBox(), 9, 0, 1, 3);

    layout->addLayout(gridlayout);

    // Status label.
    layout->addSpacing(6);
    layout->addWidget(createParamUI<ObjectStatusDisplay>()->statusWidget());

    OpenDataInspectorButton* openDataInspectorBtn = new OpenDataInspectorButton(this, tr("Show list of clusters"), QStringLiteral("clusters"), 1); // Note: Mode hint "1" is used to switch to the data table view.
    layout->addWidget(openDataInspectorBtn);
}

}   // End of namespace
