// SPDX-FileCopyrightText: 2026 OVITO GmbH, Germany
// SPDX-License-Identifier: GPL-3.0-only OR MIT

#include <ovito/gui/desktop/GUI.h>
#include <ovito/gui/desktop/properties/IntegerRadioButtonParameterUI.h>
#include <ovito/core/dataset/animation/controller/Controller.h>
#include <ovito/core/dataset/animation/AnimationSettings.h>
#include <ovito/core/app/undo/UndoableOperation.h>
#include <ovito/core/dataset/DataSetContainer.h>

namespace Ovito {

IMPLEMENT_ABSTRACT_OVITO_CLASS(IntegerRadioButtonParameterUI);

/******************************************************************************
* Constructor.
******************************************************************************/
void IntegerRadioButtonParameterUI::initializeObject(PropertiesEditor* parentEditor, const PropertyFieldDescriptor* propField)
{
    PropertyParameterUI::initializeObject(parentEditor, propField);

    _buttonGroup = new QButtonGroup(this);
    connect(_buttonGroup.data(), &QButtonGroup::idClicked, this, &IntegerRadioButtonParameterUI::updatePropertyValue);
}

/******************************************************************************
* Creates a new radio button widget that can be selected by the user
* to set the property value to the given value.
******************************************************************************/
QRadioButton* IntegerRadioButtonParameterUI::addRadioButton(int value, const QString& caption)
{
    QRadioButton* button = new QRadioButton(caption);
    if(buttonGroup()) {
        button->setEnabled(editObject() && isEnabled() && !editor()->isReadOnly());
        buttonGroup()->addButton(button, value);
    }
    return button;
}

/******************************************************************************
* This method is called when a new editable object has been assigned to the properties owner this
* parameter UI belongs to.
******************************************************************************/
void IntegerRadioButtonParameterUI::resetUI()
{
    PropertyParameterUI::resetUI();

    if(buttonGroup()) {
        for(QAbstractButton* button : buttonGroup()->buttons()) {
            if(isReferenceFieldUI())
                button->setEnabled(parameterObject() && isEnabled() && !editor()->isReadOnly());
            else
                button->setEnabled(editObject() && isEnabled() && !editor()->isReadOnly());
        }
    }

    if(isReferenceFieldUI() && editObject()) {
        // Update the displayed value when the animation time has changed.
        connect(&datasetContainer(), &DataSetContainer::currentFrameChanged, this, &IntegerRadioButtonParameterUI::updateUI, Qt::UniqueConnection);
    }
}

/******************************************************************************
* This method is called when a new editable object has been assigned to the properties owner this
* parameter UI belongs to.
******************************************************************************/
void IntegerRadioButtonParameterUI::updateUI()
{
    PropertyParameterUI::updateUI();

    if(buttonGroup() && editObject()) {
        int id = buttonGroup()->checkedId();
        if(isReferenceFieldUI()) {
            if(Controller* ctrl = dynamic_object_cast<Controller>(parameterObject())) {
                id = ctrl->getIntValue(currentAnimationTime());
            }
        }
        else if(isPropertyFieldUI()) {
            QVariant val = editObject()->getPropertyFieldValue(propertyField());
            OVITO_ASSERT(val.isValid());
            id = val.toInt();
        }
        QAbstractButton* btn = buttonGroup()->button(id);
        if(btn != NULL)
            btn->setChecked(true);
        else {
            btn = buttonGroup()->checkedButton();
            if(btn) btn->setChecked(false);
        }
    }
}

/******************************************************************************
* Sets the enabled state of the UI.
******************************************************************************/
void IntegerRadioButtonParameterUI::setEnabled(bool enabled)
{
    if(enabled == isEnabled()) return;
    PropertyParameterUI::setEnabled(enabled);
    if(buttonGroup()) {
        for(QAbstractButton* button : buttonGroup()->buttons()) {
            if(isReferenceFieldUI())
                button->setEnabled(parameterObject() && isEnabled());
            else
                button->setEnabled(editObject() && isEnabled());
        }
    }
}

/******************************************************************************
* Takes the value entered by the user and stores it in the property field
* this property UI is bound to.
******************************************************************************/
void IntegerRadioButtonParameterUI::updatePropertyValue()
{
    if(buttonGroup() && editObject()) {
        int id = buttonGroup()->checkedId();
        if(id != -1) {
            performTransaction(tr("Change parameter"), [this, id]() {
                if(isReferenceFieldUI()) {
                    if(Controller* ctrl = dynamic_object_cast<Controller>(parameterObject())) {
                        ctrl->setIntValue(currentAnimationTime(), id);
                        updateUI();
                    }
                }
                else if(isPropertyFieldUI()) {
                    editObject()->setPropertyFieldValue(propertyField(), id);
                }
                Q_EMIT valueEntered();
            });
        }
    }
}

}   // End of namespace
