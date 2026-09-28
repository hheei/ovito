// SPDX-FileCopyrightText: 2026 OVITO GmbH, Germany
// SPDX-License-Identifier: GPL-3.0-only OR MIT

#include <ovito/particles/gui/ParticlesGui.h>
#include <ovito/particles/modifier/analysis/centrosymmetry/CentroSymmetryModifier.h>
#include <ovito/gui/desktop/properties/BooleanParameterUI.h>
#include <ovito/gui/desktop/properties/IntegerParameterUI.h>
#include <ovito/gui/desktop/properties/IntegerRadioButtonParameterUI.h>
#include <ovito/gui/desktop/properties/ObjectStatusDisplay.h>
#include <ovito/core/dataset/pipeline/ModificationNode.h>
#include <qwt/qwt_plot_zoneitem.h>
#include "CentroSymmetryModifierEditor.h"

namespace Ovito {

IMPLEMENT_CREATABLE_OVITO_CLASS(CentroSymmetryModifierEditor);
SET_OVITO_OBJECT_EDITOR(CentroSymmetryModifier, CentroSymmetryModifierEditor);

/******************************************************************************
* Sets up the UI widgets of the editor.
******************************************************************************/
void CentroSymmetryModifierEditor::createUI(const RolloutInsertionParameters& rolloutParams)
{
    // Create a rollout.
    QWidget* rollout = createRollout(tr("Centrosymmetry parameter"), rolloutParams, "manual:particles.modifiers.centrosymmetry");

    // Create the rollout contents.
    QVBoxLayout* layout1 = new QVBoxLayout(rollout);
    layout1->setContentsMargins(4,4,4,4);
    layout1->setSpacing(4);

    QGridLayout* layout2 = new QGridLayout();
    layout2->setContentsMargins(0,0,0,0);
    layout2->setSpacing(6);
    layout2->setColumnStretch(1, 1);
    layout1->addLayout(layout2);

    // Num neighbors parameter.
    IntegerParameterUI* numNeighborsPUI = createParamUI<IntegerParameterUI>(PROPERTY_FIELD(CentroSymmetryModifier::numNeighbors));
    layout2->addWidget(numNeighborsPUI->label(), 0, 0);
    layout2->addLayout(numNeighborsPUI->createFieldLayout(), 0, 1);

    // Info label
    //QLabel* infoLabel = new QLabel(tr("This parameter specifies the number of nearest neighbors in the underlying lattice of atoms. For FCC and BCC lattices, set this to 12 and 8 respectively. More generally, it must be a positive, even integer."));
    //infoLabel->setWordWrap(true);
    //layout2->addWidget(infoLabel);

    QGridLayout* layout3 = new QGridLayout();
    layout3->setContentsMargins(0,0,0,0);
    layout3->setColumnMinimumWidth(0, 20);
    layout1->addLayout(layout3);

    // Method selector.
    IntegerRadioButtonParameterUI* modeUI = createParamUI<IntegerRadioButtonParameterUI>(PROPERTY_FIELD(CentroSymmetryModifier::mode));
    QRadioButton* conventionalModeBtn = modeUI->addRadioButton(CentroSymmetryModifier::ConventionalMode, tr("Conventional CSP"));
    QRadioButton* matchingModeBtn = modeUI->addRadioButton(CentroSymmetryModifier::MatchingMode, tr("Minimum-weight matching CSP"));
    layout3->addWidget(new QLabel(tr("Algorithm:")), 0, 0, 1, 2);
    layout3->addWidget(conventionalModeBtn, 1, 1);
    layout3->addWidget(matchingModeBtn, 2, 1);

    // Use only selected particles.
    BooleanParameterUI* onlySelectedParticlesUI = createParamUI<BooleanParameterUI>(PROPERTY_FIELD(CentroSymmetryModifier::onlySelectedParticles));
    layout1->addSpacing(6);
    layout1->addWidget(onlySelectedParticlesUI->checkBox());

    // Create plot widget for CSP distribution.
    _cspPlotWidget = new DataTablePlotWidget();
    _cspPlotWidget->setMinimumHeight(200);
    _cspPlotWidget->setMaximumHeight(200);
    layout1->addSpacing(10);
    layout1->addWidget(_cspPlotWidget);

    // Update data plot whenever the modifier has calculated new results.
    connect(this, &PropertiesEditor::pipelineOutputChanged, this, &CentroSymmetryModifierEditor::plotHistogram);

    // Status label.
    layout1->addSpacing(10);
    layout1->addWidget(createParamUI<ObjectStatusDisplay>()->statusWidget());
}

/******************************************************************************
* Replots the histogram computed by the modifier.
******************************************************************************/
void CentroSymmetryModifierEditor::plotHistogram()
{
    handleExceptions([&]() {
        // Request the modifier's pipeline output.
        const PipelineFlowState& state = getPipelineOutput();

        if(state) {
            // Look up the data table in the modifier's pipeline output.
            _cspPlotWidget->setTable(state.getObjectBy<DataTable>(modificationNode(), QStringLiteral("csp-centrosymmetry")));
        }
        else {
            _cspPlotWidget->setTable({});
        }
    });
}

}   // End of namespace
