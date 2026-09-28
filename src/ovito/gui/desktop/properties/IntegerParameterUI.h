// SPDX-FileCopyrightText: 2026 OVITO GmbH, Germany
// SPDX-License-Identifier: GPL-3.0-only OR MIT

#pragma once


#include <ovito/gui/desktop/GUI.h>
#include "NumericalParameterUI.h"

namespace Ovito {

/******************************************************************************
* A parameter UI for integer properties.
******************************************************************************/
class OVITO_GUI_EXPORT IntegerParameterUI : public NumericalParameterUI
{
    OVITO_CLASS(IntegerParameterUI)
    Q_OBJECT

public:

    /// Constructor.
    void initializeObject(PropertiesEditor* parentEditor, const PropertyFieldDescriptor* propField);

    /// Gets the minimum value to be entered.
    /// This value is in native controller units.
    int minValue() const;

    /// Sets the minimum value to be entered.
    /// This value must be specified in native controller units.
    void setMinValue(int minValue);

    /// Gets the maximum value to be entered.
    /// This value is in native controller units.
    int maxValue() const;

    /// Sets the maximum value to be entered.
    /// This value must be specified in native controller units.
    void setMaxValue(int maxValue);

    /// This method updates the displayed value of the parameter UI.
    virtual void updateUI() override;

    /// Takes the value entered by the user and stores it in the property field
    /// this property UI is bound to.
    virtual void updatePropertyValue() override;

public:

    Q_PROPERTY(int minValue READ minValue WRITE setMinValue)
    Q_PROPERTY(int maxValue READ maxValue WRITE setMaxValue)
};

}   // End of namespace
