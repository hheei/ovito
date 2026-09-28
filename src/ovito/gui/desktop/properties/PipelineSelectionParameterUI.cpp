// SPDX-FileCopyrightText: 2026 OVITO GmbH, Germany
// SPDX-License-Identifier: GPL-3.0-only OR MIT

#include <ovito/gui/desktop/GUI.h>
#include <ovito/gui/desktop/properties/PropertiesEditor.h>
#include <ovito/gui/desktop/widgets/general/PopupUpdateComboBox.h>
#include "PipelineSelectionParameterUI.h"

namespace Ovito {

IMPLEMENT_ABSTRACT_OVITO_CLASS(PipelineSelectionParameterUI);

/******************************************************************************
* Constructor.
******************************************************************************/
void PipelineSelectionParameterUI::initializeObject(PropertiesEditor* parentEditor, const PropertyFieldDescriptor* propField)
{
    PropertyParameterUI::initializeObject(parentEditor, propField);

    OVITO_ASSERT(isReferenceFieldUI() && !propertyField()->isVector());

    PopupUpdateComboBox* comboBox = new PopupUpdateComboBox();
    connect(comboBox, qOverload<int>(&QComboBox::activated), this, &PipelineSelectionParameterUI::updatePropertyValue);
    connect(comboBox, &PopupUpdateComboBox::dropDownActivated, this, &PipelineSelectionParameterUI::updateUI);
    _comboBox = comboBox;
    _comboBox->setAccessibleName(propertyField()->displayName());
}

/******************************************************************************
* Destructor.
******************************************************************************/
PipelineSelectionParameterUI::~PipelineSelectionParameterUI()
{
    delete comboBox();
}

/******************************************************************************
* This method is called when a new editable object has been assigned to the properties owner this
* parameter UI belongs to.
******************************************************************************/
void PipelineSelectionParameterUI::resetUI()
{
    PropertyParameterUI::resetUI();

    if(comboBox())
        comboBox()->setEnabled(editObject() && isEnabled() && !editor()->isReadOnly());
}

/******************************************************************************
* This method is called when a reference target changes.
******************************************************************************/
bool PipelineSelectionParameterUI::referenceEvent(RefTarget* source, const ReferenceEvent& event)
{
    if(source == parameterObject() && event.type() == ReferenceEvent::TitleChanged) {
        updateUI();
    }
    return PropertyParameterUI::referenceEvent(source, event);
}

/******************************************************************************
* This method is called when a new editable object has been assigned to the
* properties owner this parameter UI belongs to.
******************************************************************************/
void PipelineSelectionParameterUI::updateUI()
{
    PropertyParameterUI::updateUI();

    if(comboBox()) {
        comboBox()->clear();
        if(editObject()) {
            editor()->visitScenePipelines([&](SceneNode* sceneNode) {
                comboBox()->addItem(sceneNode->objectTitle(), QVariant::fromValue(sceneNode->pipeline()));
                return true;
            });
        }
        if(comboBox()->count() == 0 || !parameterObject())
            comboBox()->addItem(tr("‹none›"), QVariant::fromValue<Pipeline*>(nullptr));
        comboBox()->setCurrentIndex(comboBox()->findData(QVariant::fromValue(dynamic_object_cast<Pipeline>(parameterObject()))));
    }
}

/******************************************************************************
* Takes the value entered by the user and stores it in the property field
* this property UI is bound to.
******************************************************************************/
void PipelineSelectionParameterUI::updatePropertyValue()
{
    if(editObject()) {
        OVITO_ASSERT(isReferenceFieldUI() && !propertyField()->isVector());
        performTransaction(tr("Select pipeline"), [&]() {
            editObject()->setReferenceFieldTarget(propertyField(), comboBox()->currentData().value<Pipeline*>());
            Q_EMIT valueEntered();
        });
    }
}

/******************************************************************************
* Sets the enabled state of the UI.
******************************************************************************/
void PipelineSelectionParameterUI::setEnabled(bool enabled)
{
    if(enabled == isEnabled())
        return;
    PropertyParameterUI::setEnabled(enabled);
    if(comboBox())
        comboBox()->setEnabled(editObject() && isEnabled());
}

}   // End of namespace
