// SPDX-FileCopyrightText: 2026 OVITO GmbH, Germany
// SPDX-License-Identifier: GPL-3.0-only OR MIT

#pragma once


#include <ovito/gui/desktop/GUI.h>
#include "FloatParameterUI.h"

namespace Ovito {

/******************************************************************************
 * A parameter UI for AffineTransformation or Matrix3 type properties.
 * This ParameterUI lets the user edit the individual matrix components.
 ******************************************************************************/
class OVITO_GUI_EXPORT AffineTransformationParameterUI : public FloatParameterUI
{
    OVITO_CLASS(AffineTransformationParameterUI)
    Q_OBJECT

public:

    /// Constructor.
    void initializeObject(PropertiesEditor* parentEditor, const PropertyFieldDescriptor* propField, size_t row, size_t column);

    /// This method updates the displayed value of the parameter UI.
    virtual void updateUI() override;

    /// Takes the value entered by the user and stores it in the property field
    /// this property UI is bound to.
    virtual void updatePropertyValue() override;

private:

    /// The matrix component to control.
    size_t _row;
    size_t _column;
};

}   // End of namespace
