// SPDX-FileCopyrightText: 2026 OVITO GmbH, Germany
// SPDX-License-Identifier: GPL-3.0-only OR MIT

#include <ovito/stdmod/gui/StdModGui.h>
#include <ovito/gui/desktop/properties/DataObjectReferenceParameterUI.h>
#include "InvertSelectionModifierEditor.h"

namespace Ovito {

IMPLEMENT_CREATABLE_OVITO_CLASS(InvertSelectionModifierEditor);
SET_OVITO_OBJECT_EDITOR(InvertSelectionModifier, InvertSelectionModifierEditor);

/******************************************************************************
* Sets up the UI widgets of the editor.
******************************************************************************/
void InvertSelectionModifierEditor::createUI(const RolloutInsertionParameters& rolloutParams)
{
    QWidget* rollout = createRollout(tr("Invert selection"), rolloutParams, "manual:particles.modifiers.invert_selection");

    QVBoxLayout* layout = new QVBoxLayout(rollout);
    layout->setContentsMargins(8,8,8,8);
    layout->setSpacing(4);

    DataObjectReferenceParameterUI* pclassUI = createParamUI<DataObjectReferenceParameterUI>(PROPERTY_FIELD(GenericPropertyModifier::subject), PropertyContainer::OOClass());
    layout->addWidget(new QLabel(tr("Operate on:")));
    layout->addWidget(pclassUI->comboBox());

    // List only property container that support element selection.
    pclassUI->setObjectFilter<PropertyContainer>([](const PropertyContainer* container) {
        return container->getOOMetaClass().isValidStandardPropertyId(Property::GenericSelectionProperty);
    });
}

}   // End of namespace
