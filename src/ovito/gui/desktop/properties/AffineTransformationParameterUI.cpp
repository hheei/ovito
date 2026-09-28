// SPDX-FileCopyrightText: 2026 OVITO GmbH, Germany
// SPDX-License-Identifier: GPL-3.0-only OR MIT

#include <ovito/gui/desktop/GUI.h>
#include <ovito/gui/desktop/properties/AffineTransformationParameterUI.h>

namespace Ovito {

IMPLEMENT_ABSTRACT_OVITO_CLASS(AffineTransformationParameterUI);

/******************************************************************************
* Constructor.
******************************************************************************/
void AffineTransformationParameterUI::initializeObject(PropertiesEditor* parentEditor, const PropertyFieldDescriptor* propField, size_t row, size_t column)
{
    OVITO_ASSERT_MSG(row >= 0 && row < 3, "AffineTransformationParameterUI constructor", "The row must be in the range 0-2.");
    OVITO_ASSERT_MSG(column >= 0 && column < 4, "AffineTransformationParameterUI constructor", "The column must be in the range 0-3.");

    FloatParameterUI::initializeObject(parentEditor, propField);

    _row = row;
    _column = column;
}

/******************************************************************************
* Takes the value entered by the user and stores it in the parameter object
* this parameter UI is bound to.
******************************************************************************/
void AffineTransformationParameterUI::updatePropertyValue()
{
    if(editObject() && spinner()) {
        performTransaction(tr("Change parameter value"), [&]() {
            if(isPropertyFieldUI()) {
                QVariant currentValue = editObject()->getPropertyFieldValue(propertyField());
                if(currentValue.canConvert<AffineTransformation>()) {
                    AffineTransformation val = currentValue.value<AffineTransformation>();
                    val(_row, _column) = spinner()->floatValue();
                    currentValue.setValue(val);
                }
                else if(currentValue.canConvert<Matrix3>()) {
                    Matrix3 val = currentValue.value<Matrix3>();
                    OVITO_ASSERT_MSG(_column >= 0 && _column < 3, "AffineTransformationParameterUI",
                                     "The column must be in the range 0-2 when used with Matrix3.");
                    val(_row, _column) = spinner()->floatValue();
                    currentValue.setValue(val);
                }
                editObject()->setPropertyFieldValue(propertyField(), currentValue);
            }
            Q_EMIT valueEntered();
        });
    }
}

/******************************************************************************
* This method updates the displayed value of the parameter UI.
******************************************************************************/
void AffineTransformationParameterUI::updateUI()
{
    if(editObject() && spinner() && !spinner()->isDragging()) {
        QVariant val;
        if(isPropertyFieldUI()) {
            val = editObject()->getPropertyFieldValue(propertyField());
            OVITO_ASSERT(val.isValid() && (val.canConvert<AffineTransformation>() || val.canConvert<Matrix3>()));
        }
        else return;

        if(val.canConvert<AffineTransformation>()) {
            spinner()->setFloatValue(val.value<AffineTransformation>()(_row, _column));
        }
        else if(val.canConvert<Matrix3>()) {
            OVITO_ASSERT_MSG(_column >= 0 && _column < 3, "AffineTransformationParameterUI",
                             "The column must be in the range 0-2 when used with Matrix3.");
            spinner()->setFloatValue(val.value<Matrix3>()(_row, _column));
        }
    }
}

}   // End of namespace
