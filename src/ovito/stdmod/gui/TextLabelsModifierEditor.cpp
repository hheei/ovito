// SPDX-FileCopyrightText: 2026 OVITO GmbH, Germany
// SPDX-License-Identifier: GPL-3.0-only OR MIT

#include <ovito/stdmod/gui/StdModGui.h>
#include <ovito/gui/desktop/properties/BooleanParameterUI.h>
#include <ovito/gui/desktop/properties/StringParameterUI.h>
#include <ovito/gui/desktop/properties/ObjectStatusDisplay.h>
#include <ovito/gui/desktop/properties/DataObjectReferenceParameterUI.h>
#include <ovito/gui/desktop/properties/SubObjectParameterUI.h>
#include <ovito/stdmod/modifiers/TextLabelsModifier.h>
#include <ovito/stdobj/table/DataTable.h>
#include "TextLabelsModifierEditor.h"

namespace Ovito {

IMPLEMENT_CREATABLE_OVITO_CLASS(TextLabelsModifierEditor);
SET_OVITO_OBJECT_EDITOR(TextLabelsModifier, TextLabelsModifierEditor);

/******************************************************************************
* Sets up the UI widgets of the editor.
******************************************************************************/
void TextLabelsModifierEditor::createUI(const RolloutInsertionParameters& rolloutParams)
{
    QWidget* rollout = createRollout(tr("Add text labels"), rolloutParams, "manual:particles.modifiers.add_text_labels");

    // Create the rollout contents.
    QVBoxLayout* layout = new QVBoxLayout(rollout);
    layout->setContentsMargins(4,4,4,4);
    layout->setSpacing(2);

    // Offer only those containers that can supply anchor positions for the labels.
    DataObjectReferenceParameterUI* pclassUI = createParamUI<DataObjectReferenceParameterUI>(PROPERTY_FIELD(GenericPropertyModifier::subject), PropertyContainer::OOClass());
    layout->addWidget(new QLabel(tr("Operate on:")));
    layout->addWidget(pclassUI->comboBox());
    pclassUI->setObjectFilter<PropertyContainer>([](const PropertyContainer* container) {
        return container->getOOMetaClass().supportsTextLabels();
    });

    // A data table provides no anchor positions of its own. Let the user pick the column holding
    // the 3d coordinates of the labels. The selection box is hidden for all other container types.
    _positionPropertyUI = createParamUI<PropertyReferenceParameterUI>(
        PROPERTY_FIELD(TextLabelsModifier::positionProperty), nullptr, PropertyReferenceParameterUI::ShowNoComponents);
    _positionPropertyUI->setContainerField(PROPERTY_FIELD(GenericPropertyModifier::subject));
    _positionPropertyLabel = new QLabel(tr("Positions:"));
    layout->addSpacing(4);
    layout->addWidget(_positionPropertyLabel);
    layout->addWidget(_positionPropertyUI->comboBox());

    // Only 3-component floating-point properties can be used as anchor points.
    _positionPropertyUI->setPropertyFilter(
        [](const PropertyContainer* container, const Property* property) { return TextLabelsModifier::isValidPositionProperty(property); });

    connect(this, &PropertiesEditor::contentsChanged, this, [this](RefTarget* editObject) {
        // The anchor positions are configurable only for data tables.
        TextLabelsModifier* modifier = static_object_cast<TextLabelsModifier>(editObject);
        bool visible = (modifier && modifier->subject().dataClass() == &DataTable::OOClass());
        _positionPropertyLabel->setVisible(visible);
        _positionPropertyUI->comboBox()->setVisible(visible);
        container()->updateRolloutsLater();
    });

    // A vector property may be selected as a whole, in which case all of its components go into the label.
    _sourcePropertyUI = createParamUI<PropertyReferenceParameterUI>(PROPERTY_FIELD(TextLabelsModifier::sourceProperty), nullptr,
                                                                   PropertyReferenceParameterUI::ShowComponentsAndVectorProperties);
    _sourcePropertyUI->setContainerField(PROPERTY_FIELD(GenericPropertyModifier::subject));
    layout->addSpacing(4);
    layout->addWidget(new QLabel(tr("Property:")));
    layout->addWidget(_sourcePropertyUI->comboBox());

    // Properties without a data type cannot be converted into label texts.
    _sourcePropertyUI->setPropertyFilter([](const PropertyContainer* container, const Property* property) {
        return property->dataType() != QMetaType::Void;
    });

    layout->addSpacing(6);
    QGridLayout* gridLayout = new QGridLayout();
    gridLayout->setContentsMargins(0,0,0,0);
    gridLayout->setColumnStretch(1, 1);
    gridLayout->setSpacing(2);
    gridLayout->setHorizontalSpacing(4);
    layout->addLayout(gridLayout);

    StringParameterUI* valueFormatStringPUI = createParamUI<StringParameterUI>(PROPERTY_FIELD(TextLabelsModifier::valueFormatString));
    gridLayout->addWidget(new QLabel(tr("Format:")), 0, 0);
    gridLayout->addWidget(valueFormatStringPUI->textBox(), 0, 1);

    layout->addSpacing(6);

    // Only selected elements.
    BooleanParameterUI* onlySelectedPUI = createParamUI<BooleanParameterUI>(PROPERTY_FIELD(TextLabelsModifier::onlySelected));
    layout->addWidget(onlySelectedPUI->checkBox());

    // Status label.
    layout->addSpacing(10);
    layout->addWidget(createParamUI<ObjectStatusDisplay>()->statusWidget());

    // Open a sub-editor for the vis element owned by the modifier.
    createParamUI<SubObjectParameterUI>(PROPERTY_FIELD(TextLabelsModifier::textLabelsVis), rolloutParams.after(rollout));
}

}   // End of namespace
