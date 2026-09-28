// SPDX-FileCopyrightText: 2026 OVITO GmbH, Germany
// SPDX-License-Identifier: GPL-3.0-only OR MIT

#include <ovito/gui/desktop/GUI.h>
#include <ovito/gui/desktop/properties/VariantComboBoxParameterUI.h>
#include <ovito/core/app/undo/UndoableOperation.h>

namespace Ovito {

IMPLEMENT_ABSTRACT_OVITO_CLASS(VariantComboBoxParameterUI);

/******************************************************************************
* Constructor.
******************************************************************************/
void VariantComboBoxParameterUI::initializeObject(PropertiesEditor* parentEditor, const PropertyFieldDescriptor* propField)
{
    PropertyParameterUI::initializeObject(parentEditor, propField);

    _comboBox = new QComboBox();
    _comboBox->setAccessibleName(propertyField()->displayName());
    connect(comboBox(), qOverload<int>(&QComboBox::activated), this, &VariantComboBoxParameterUI::updatePropertyValue);
}

/******************************************************************************
* Destructor.
******************************************************************************/
VariantComboBoxParameterUI::~VariantComboBoxParameterUI()
{
    // Release GUI controls.
    delete comboBox();
}

/******************************************************************************
* This method is called when a new editable object has been assigned to the properties owner this
* parameter UI belongs to.
******************************************************************************/
void VariantComboBoxParameterUI::resetUI()
{
    PropertyParameterUI::resetUI();

    if(comboBox())
        comboBox()->setEnabled(editObject() != NULL && isEnabled() && !editor()->isReadOnly());
}

/******************************************************************************
* This method is called when a new editable object has been assigned to the properties owner this
* parameter UI belongs to.
******************************************************************************/
void VariantComboBoxParameterUI::updateUI()
{
    PropertyParameterUI::updateUI();

    if(comboBox() && editObject()) {
        QVariant val;
        if(isPropertyFieldUI()) {
            val = editObject()->getPropertyFieldValue(propertyField());
            OVITO_ASSERT(val.isValid());
        }
        else {
            return;
        }
        comboBox()->setCurrentIndex(comboBox()->findData(val));
        if(comboBox()->isEditable())
            comboBox()->setEditText(val.toString());
    }
}

/******************************************************************************
* Sets the enabled state of the UI.
******************************************************************************/
void VariantComboBoxParameterUI::setEnabled(bool enabled)
{
    if(enabled == isEnabled())
        return;
    PropertyParameterUI::setEnabled(enabled);
    if(comboBox())
        comboBox()->setEnabled(editObject() != NULL && isEnabled());
}

/******************************************************************************
* Takes the value entered by the user and stores it in the property field
* this property UI is bound to.
******************************************************************************/
void VariantComboBoxParameterUI::updatePropertyValue()
{
    if(comboBox() && editObject() && comboBox()->currentIndex() >= 0) {
        performTransaction(tr("Change parameter"), [this]() {
            QVariant newValue;
            if(comboBox()->isEditable())
                newValue = comboBox()->currentText();
            else
                newValue = comboBox()->itemData(comboBox()->currentIndex());

            if(isPropertyFieldUI()) {
                editObject()->setPropertyFieldValue(propertyField(), newValue);
            }

            Q_EMIT valueEntered();
        });
    }
}

}   // End of namespace
