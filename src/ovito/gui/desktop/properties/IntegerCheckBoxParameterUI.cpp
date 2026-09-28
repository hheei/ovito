// SPDX-FileCopyrightText: 2026 OVITO GmbH, Germany
// SPDX-License-Identifier: GPL-3.0-only OR MIT

#include <ovito/gui/desktop/GUI.h>
#include <ovito/gui/desktop/properties/IntegerCheckBoxParameterUI.h>
#include <ovito/core/dataset/animation/controller/Controller.h>
#include <ovito/core/dataset/animation/AnimationSettings.h>
#include <ovito/core/app/undo/UndoableOperation.h>
#include <ovito/core/dataset/DataSetContainer.h>

namespace Ovito {

IMPLEMENT_ABSTRACT_OVITO_CLASS(IntegerCheckBoxParameterUI);

/******************************************************************************
* Constructor.
******************************************************************************/
void IntegerCheckBoxParameterUI::initializeObject(PropertiesEditor* parentEditor, const PropertyFieldDescriptor* propField, int uncheckedValue, int checkedValue)
{
    OVITO_ASSERT(uncheckedValue != checkedValue);

    PropertyParameterUI::initializeObject(parentEditor, propField);

    _uncheckedValue = uncheckedValue;
    _checkedValue = checkedValue;

    // Create UI widget.
    _checkBox = new QCheckBox(propField->displayName());
    connect(_checkBox.data(), &QCheckBox::clicked, this, &IntegerCheckBoxParameterUI::updatePropertyValue);
}

/******************************************************************************
* Destructor.
******************************************************************************/
IntegerCheckBoxParameterUI::~IntegerCheckBoxParameterUI()
{
    // Release widget.
    delete checkBox();
}

/******************************************************************************
* This method is called when a new editable object has been assigned to the properties owner this
* parameter UI belongs to.
******************************************************************************/
void IntegerCheckBoxParameterUI::resetUI()
{
    PropertyParameterUI::resetUI();

    if(checkBox()) {
        if(isReferenceFieldUI())
            checkBox()->setEnabled(parameterObject() && isEnabled() && !editor()->isReadOnly());
        else
            checkBox()->setEnabled(editObject() && isEnabled() && !editor()->isReadOnly());
    }

    if(isReferenceFieldUI() && editObject()) {
        // Update the displayed value when the animation time has changed.
        connect(&datasetContainer(), &DataSetContainer::currentFrameChanged, this, &IntegerCheckBoxParameterUI::updateUI, Qt::UniqueConnection);
    }
}

/******************************************************************************
* This method is called when a new editable object has been assigned to the properties owner this
* parameter UI belongs to.
******************************************************************************/
void IntegerCheckBoxParameterUI::updateUI()
{
    PropertyParameterUI::updateUI();

    if(checkBox() && editObject()) {
        int value = _uncheckedValue;
        if(isReferenceFieldUI()) {
            if(Controller* ctrl = dynamic_object_cast<Controller>(parameterObject())) {
                value = ctrl->getIntValue(currentAnimationTime());
            }
        }
        else if(isPropertyFieldUI()) {
            QVariant val = editObject()->getPropertyFieldValue(propertyField());
            OVITO_ASSERT(val.isValid());
            value = val.toInt();
        }
        checkBox()->setChecked(value == _checkedValue);
    }
}

/******************************************************************************
* Sets the enabled state of the UI.
******************************************************************************/
void IntegerCheckBoxParameterUI::setEnabled(bool enabled)
{
    if(enabled == isEnabled()) return;
    PropertyParameterUI::setEnabled(enabled);
    if(checkBox()) {
        if(isReferenceFieldUI())
            checkBox()->setEnabled(parameterObject() && isEnabled());
        else
            checkBox()->setEnabled(editObject() && isEnabled());
    }
}

/******************************************************************************
* Takes the value entered by the user and stores it in the property field
* this property UI is bound to.
******************************************************************************/
void IntegerCheckBoxParameterUI::updatePropertyValue()
{
    if(checkBox() && editObject()) {
        performTransaction(tr("Change parameter value"), [&]() {
            int value = checkBox()->isChecked() ? _checkedValue : _uncheckedValue;
            if(isReferenceFieldUI()) {
                if(Controller* ctrl = dynamic_object_cast<Controller>(parameterObject())) {
                    ctrl->setIntValue(currentAnimationTime(), value);
                    updateUI();
                }
            }
            else if(isPropertyFieldUI()) {
                editObject()->setPropertyFieldValue(propertyField(), value);
            }
            Q_EMIT valueEntered();
        });
    }
}

}   // End of namespace
