// SPDX-FileCopyrightText: 2026 OVITO GmbH, Germany
// SPDX-License-Identifier: GPL-3.0-only OR MIT

#include <ovito/gui/desktop/GUI.h>
#include <ovito/gui/desktop/properties/BooleanRadioButtonParameterUI.h>
#include <ovito/core/app/undo/UndoableOperation.h>

namespace Ovito {

IMPLEMENT_ABSTRACT_OVITO_CLASS(BooleanRadioButtonParameterUI);

/******************************************************************************
* Constructor.
******************************************************************************/
void BooleanRadioButtonParameterUI::initializeObject(PropertiesEditor* parentEditor, const PropertyFieldDescriptor* propField)
{
    PropertyParameterUI::initializeObject(parentEditor, propField);

    _buttonGroup = new QButtonGroup(this);
    connect(_buttonGroup.data(), &QButtonGroup::idClicked, this, &BooleanRadioButtonParameterUI::updatePropertyValue);

    QRadioButton* buttonNo = new QRadioButton();
    QRadioButton* buttonYes = new QRadioButton();
    _buttonGroup->addButton(buttonNo, 0);
    _buttonGroup->addButton(buttonYes, 1);
}

/******************************************************************************
* Destructor.
******************************************************************************/
BooleanRadioButtonParameterUI::~BooleanRadioButtonParameterUI()
{
    // Release GUI controls.
    delete buttonTrue();
    delete buttonFalse();
}

/******************************************************************************
* This method is called when a new editable object has been assigned to the properties owner this
* parameter UI belongs to.
******************************************************************************/
void BooleanRadioButtonParameterUI::resetUI()
{
    PropertyParameterUI::resetUI();

    if(buttonGroup()) {
        for(QAbstractButton* button : buttonGroup()->buttons())
            button->setEnabled(editObject() && isEnabled() && !editor()->isReadOnly());
    }
}

/******************************************************************************
* This method is called when a new editable object has been assigned to the properties owner this
* parameter UI belongs to.
******************************************************************************/
void BooleanRadioButtonParameterUI::updateUI()
{
    PropertyParameterUI::updateUI();

    if(buttonGroup() && editObject()) {
        QVariant val;
        if(propertyField()) {
            val = editObject()->getPropertyFieldValue(propertyField());
            OVITO_ASSERT(val.isValid());
        }
        bool state = val.toBool();
        if(state && buttonTrue())
            buttonTrue()->setChecked(true);
        else if(!state && buttonFalse())
            buttonFalse()->setChecked(true);
    }
}

/******************************************************************************
* Sets the enabled state of the UI.
******************************************************************************/
void BooleanRadioButtonParameterUI::setEnabled(bool enabled)
{
    if(enabled == isEnabled()) return;
    PropertyParameterUI::setEnabled(enabled);
    if(buttonGroup()) {
        for(QAbstractButton* button : buttonGroup()->buttons())
            button->setEnabled(editObject() != NULL && isEnabled());
    }
}

/******************************************************************************
* Takes the value entered by the user and stores it in the property field
* this property UI is bound to.
******************************************************************************/
void BooleanRadioButtonParameterUI::updatePropertyValue()
{
    if(buttonGroup() && editObject()) {
        performTransaction(tr("Change parameter value"), [&]() {
            int id = buttonGroup()->checkedId();
            if(id != -1) {
                QVariant oldval;
                if(propertyField()) {
                    oldval = editObject()->getPropertyFieldValue(propertyField());
                }
                if((bool)id != oldval.toBool()) {
                    if(propertyField()) {
                        editObject()->setPropertyFieldValue(propertyField(), (bool)id);
                    }
                    Q_EMIT valueEntered();
                }
            }
        });
    }
}

}   // End of namespace
