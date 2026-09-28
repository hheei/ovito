// SPDX-FileCopyrightText: 2026 OVITO GmbH, Germany
// SPDX-License-Identifier: GPL-3.0-only OR MIT

#include <ovito/gui/desktop/GUI.h>
#include <ovito/gui/desktop/properties/IntegerParameterUI.h>
#include <ovito/core/dataset/animation/controller/Controller.h>
#include <ovito/core/dataset/animation/AnimationSettings.h>
#include <ovito/core/utilities/units/UnitsManager.h>

namespace Ovito {

IMPLEMENT_ABSTRACT_OVITO_CLASS(IntegerParameterUI);

/******************************************************************************
* Constructor.
******************************************************************************/
void IntegerParameterUI::initializeObject(PropertiesEditor* parentEditor, const PropertyFieldDescriptor* propField)
{
    NumericalParameterUI::initializeObject(parentEditor, propField, &IntegerParameterUnit::staticMetaObject);
}

/******************************************************************************
* Takes the value entered by the user and stores it in the property field
* this property UI is bound to.
******************************************************************************/
void IntegerParameterUI::updatePropertyValue()
{
    if(editObject() && spinner()) {
        if(isReferenceFieldUI()) {
            if(Controller* ctrl = dynamic_object_cast<Controller>(parameterObject()))
                ctrl->setIntValue(currentAnimationTime(), spinner()->intValue());
        }
        else if(isPropertyFieldUI()) {
            editObject()->setPropertyFieldValue(propertyField(), spinner()->intValue());
        }
        Q_EMIT valueEntered();
    }
}

/******************************************************************************
* This method updates the displayed value of the parameter UI.
******************************************************************************/
void IntegerParameterUI::updateUI()
{
    if(editObject() && spinner() && !spinner()->isDragging()) {
        handleExceptions<true>([&] {
            if(isReferenceFieldUI()) {
                if(Controller* ctrl = dynamic_object_cast<Controller>(parameterObject())) {
                    spinner()->setIntValue(ctrl->getIntValue(currentAnimationTime()));
                }
            }
            else {
                QVariant val(0);
                if(isPropertyFieldUI()) {
                    val = editObject()->getPropertyFieldValue(propertyField());
                    OVITO_ASSERT(val.isValid());
                }
                spinner()->setIntValue(val.toInt());
            }
        });
    }
}

/******************************************************************************
* Gets the minimum value to be entered.
* This value is in native controller units.
******************************************************************************/
int IntegerParameterUI::minValue() const
{
    return (spinner() ? (int)spinner()->minValue() : std::numeric_limits<int>::lowest());
}

/******************************************************************************
* Sets the minimum value to be entered.
* This value must be specified in native controller units.
******************************************************************************/
void IntegerParameterUI::setMinValue(int minValue)
{
    if(spinner()) spinner()->setMinValue(minValue);
}

/******************************************************************************
* Gets the maximum value to be entered.
* This value is in native controller units.
******************************************************************************/
int IntegerParameterUI::maxValue() const
{
    return (spinner() ? (int)spinner()->maxValue() : std::numeric_limits<int>::max());
}

/******************************************************************************
* Sets the maximum value to be entered.
* This value must be specified in native controller units.
******************************************************************************/
void IntegerParameterUI::setMaxValue(int maxValue)
{
    if(spinner()) spinner()->setMaxValue(maxValue);
}

}   // End of namespace
