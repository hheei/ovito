// SPDX-FileCopyrightText: 2026 OVITO GmbH, Germany
// SPDX-License-Identifier: GPL-3.0-only OR MIT

#include <ovito/gui/desktop/GUI.h>
#include <ovito/gui/desktop/properties/FloatParameterUI.h>
#include <ovito/core/dataset/animation/controller/Controller.h>
#include <ovito/core/dataset/animation/AnimationSettings.h>
#include <ovito/core/utilities/units/UnitsManager.h>

namespace Ovito {

IMPLEMENT_ABSTRACT_OVITO_CLASS(FloatParameterUI);

/******************************************************************************
* Constructor.
******************************************************************************/
void FloatParameterUI::initializeObject(PropertiesEditor* parentEditor, const PropertyFieldDescriptor* propField)
{
    NumericalParameterUI::initializeObject(parentEditor, propField, &FloatParameterUnit::staticMetaObject);
}

/******************************************************************************
* Takes the value entered by the user and stores it in the property field
* this property UI is bound to.
******************************************************************************/
void FloatParameterUI::updatePropertyValue()
{
    if(editObject() && spinner()) {
        if(isReferenceFieldUI()) {
            if(Controller* ctrl = dynamic_object_cast<Controller>(parameterObject())) {
                OVITO_CHECK_OBJECT_POINTER(ctrl);;
                ctrl->setFloatValue(currentAnimationTime(), spinner()->floatValue());
            }
        }
        else if(isPropertyFieldUI()) {
            editObject()->setPropertyFieldValue(propertyField(), spinner()->floatValue());
        }
        Q_EMIT valueEntered();
    }
}

/******************************************************************************
* This method updates the displayed value of the parameter UI.
******************************************************************************/
void FloatParameterUI::updateUI()
{
    if(editObject() && spinner() && !spinner()->isDragging()) {
        handleExceptions<true>([&] {
            if(isReferenceFieldUI()) {
                if(Controller* ctrl = dynamic_object_cast<Controller>(parameterObject())) {
                    spinner()->setFloatValue(ctrl->getFloatValue(currentAnimationTime()));
                }
            }
            else {
                QVariant val(0.0);
                if(isPropertyFieldUI()) {
                    val = editObject()->getPropertyFieldValue(propertyField());
                    OVITO_ASSERT(val.isValid());
                }
                spinner()->setFloatValue(val.value<FloatType>());
            }
        });
    }
}

/******************************************************************************
* Gets the minimum value to be entered.
* This value is in native controller units.
******************************************************************************/
FloatType FloatParameterUI::minValue() const
{
    return (spinner() ? spinner()->minValue() : FLOATTYPE_MIN);
}

/******************************************************************************
* Sets the minimum value to be entered.
* This value must be specified in native controller units.
******************************************************************************/
void FloatParameterUI::setMinValue(FloatType minValue)
{
    if(spinner()) spinner()->setMinValue(minValue);
}

/******************************************************************************
* Gets the maximum value to be entered.
* This value is in native controller units.
******************************************************************************/
FloatType FloatParameterUI::maxValue() const
{
    return (spinner() ? spinner()->maxValue() : FLOATTYPE_MAX);
}

/******************************************************************************
* Sets the maximum value to be entered.
* This value must be specified in native controller units.
******************************************************************************/
void FloatParameterUI::setMaxValue(FloatType maxValue)
{
    if(spinner()) spinner()->setMaxValue(maxValue);
}

}   // End of namespace
