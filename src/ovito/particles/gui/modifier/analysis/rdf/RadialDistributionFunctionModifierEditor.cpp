// SPDX-FileCopyrightText: 2026 OVITO GmbH, Germany
// SPDX-License-Identifier: GPL-3.0-only OR MIT

#include <ovito/particles/gui/ParticlesGui.h>
#include <ovito/particles/modifier/analysis/rdf/RadialDistributionFunctionModifier.h>
#include <ovito/particles/objects/Particles.h>
#include <ovito/gui/desktop/properties/IntegerParameterUI.h>
#include <ovito/gui/desktop/properties/FloatParameterUI.h>
#include <ovito/gui/desktop/properties/BooleanParameterUI.h>
#include <ovito/gui/desktop/properties/ObjectStatusDisplay.h>
#include <ovito/gui/desktop/properties/OpenDataInspectorButton.h>
#include <ovito/gui/desktop/mainwin/MainWindow.h>
#include <ovito/core/dataset/pipeline/ModificationNode.h>
#include "RadialDistributionFunctionModifierEditor.h"

namespace Ovito {

IMPLEMENT_CREATABLE_OVITO_CLASS(RadialDistributionFunctionModifierEditor);
SET_OVITO_OBJECT_EDITOR(RadialDistributionFunctionModifier, RadialDistributionFunctionModifierEditor);

/******************************************************************************
 * Sets up the UI widgets of the editor.
 ******************************************************************************/
void RadialDistributionFunctionModifierEditor::createUI(const RolloutInsertionParameters& rolloutParams)
{
    // Create a rollout.
    QWidget* rollout =
        createRollout(tr("Radial distribution function (RDF)"), rolloutParams, "manual:particles.modifiers.radial_distribution_function");

    // Create the rollout contents.
    QVBoxLayout* layout = new QVBoxLayout(rollout);
    layout->setContentsMargins(4, 4, 4, 4);
    layout->setSpacing(4);

    QGridLayout* gridlayout = new QGridLayout();
    gridlayout->setContentsMargins(0, 0, 0, 0);
    gridlayout->setColumnStretch(1, 1);

    // Cutoff parameter.
    FloatParameterUI* cutoffRadiusPUI = createParamUI<FloatParameterUI>(PROPERTY_FIELD(RadialDistributionFunctionModifier::cutoff));
    gridlayout->addWidget(cutoffRadiusPUI->label(), 0, 0);
    gridlayout->addLayout(cutoffRadiusPUI->createFieldLayout(), 0, 1);

    // Number of bins parameter.
    IntegerParameterUI* numBinsPUI = createParamUI<IntegerParameterUI>(PROPERTY_FIELD(RadialDistributionFunctionModifier::numberOfBins));
    gridlayout->addWidget(numBinsPUI->label(), 1, 0);
    gridlayout->addLayout(numBinsPUI->createFieldLayout(), 1, 1);

    // Partial RDFs option.
    BooleanParameterUI* partialRdfPUI =
        createParamUI<BooleanParameterUI>(PROPERTY_FIELD(RadialDistributionFunctionModifier::computePartialRDF));
    partialRdfPUI->checkBox()->setText(tr("Compute partial RDFs per:"));
    gridlayout->addWidget(partialRdfPUI->checkBox(), 2, 0);

    // Type property selection.
    _typePropertyUI = createParamUI<PropertyReferenceParameterUI>(PROPERTY_FIELD(RadialDistributionFunctionModifier::typeProperty),
                                                                  &Particles::OOClass(),
                                                                  PropertyReferenceParameterUI::ShowNoComponents);
    gridlayout->addWidget(_typePropertyUI->comboBox(), 2, 1);
    _typePropertyUI->setEnabled(false);
    connect(partialRdfPUI->checkBox(), &QCheckBox::toggled, _typePropertyUI, &ParameterUI::setEnabled);

    // Show only typed properties that have some element types attached to them.
    _typePropertyUI->setPropertyFilter(
        [](const PropertyContainer* container, const Property* property) { return property->isTypedProperty(); });

    // Only selected particles.
    BooleanParameterUI* onlySelectedPUI =
        createParamUI<BooleanParameterUI>(PROPERTY_FIELD(RadialDistributionFunctionModifier::onlySelected));
    gridlayout->addWidget(onlySelectedPUI->checkBox(), 3, 0, 1, 2);
    layout->addLayout(gridlayout);

    _rdfPlot = new DataTablePlotWidget();
    _rdfPlot->setMinimumHeight(200);
    _rdfPlot->setMaximumHeight(200);

    layout->addSpacing(12);
    layout->addWidget(new QLabel(tr("Radial distribution function:")));
    layout->addWidget(_rdfPlot);

    OpenDataInspectorButton* openDataInspectorBtn = new OpenDataInspectorButton(this, tr("Show in data inspector"));
    layout->addWidget(openDataInspectorBtn);

    // Status label.
    layout->addSpacing(6);
    layout->addWidget(createParamUI<ObjectStatusDisplay>()->statusWidget());

    // Update data plot whenever the modifier has calculated new results.
    connect(this, &PropertiesEditor::pipelineOutputChanged, this, &RadialDistributionFunctionModifierEditor::plotRDF);
}

/******************************************************************************
 * Updates the plot of the RDF computed by the modifier.
 ******************************************************************************/
void RadialDistributionFunctionModifierEditor::plotRDF()
{
    handleExceptions([&]() {
        // Look up the data table in the modifier's pipeline output.
        DataOORef<const DataTable> table =
            getPipelineOutput().getObjectBy<DataTable>(modificationNode(), RadialDistributionFunctionModifier::TableIdentifier);

        // Determine X plotting range.
        if(table) {
            ConstPropertyPtr x = table->getXValues();
            BufferReadAccessAndRef<FloatType> rdfXArray(x);
            BufferReadAccessAndRef<FloatType*> rdfYArray(table->y());
            double minX = 0;
            for(size_t i = 0; i < rdfYArray.size(); i++) {
                for(size_t cmpnt = 0; cmpnt < rdfYArray.componentCount(); cmpnt++) {
                    if(rdfYArray.get(i, cmpnt) != 0) {
                        minX = rdfXArray[i];
                        break;
                    }
                }
                if(minX) break;
            }
            _rdfPlot->setAxisScale(
                QwtPlot::xBottom, std::floor(minX * 9.0 / table->intervalEnd()) / 10.0 * table->intervalEnd(), table->intervalEnd());
        }
        _rdfPlot->setTable(std::move(table));
    });
}

}  // namespace Ovito
