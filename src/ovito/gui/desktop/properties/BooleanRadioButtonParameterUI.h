// SPDX-FileCopyrightText: 2026 OVITO GmbH, Germany
// SPDX-License-Identifier: GPL-3.0-only OR MIT

#pragma once


#include <ovito/gui/desktop/GUI.h>
#include "PropertyParameterUI.h"

namespace Ovito {

/******************************************************************************
* This UI allows the user to change a boolean-value property of the object being edited
* using two radio buttons.
******************************************************************************/
class OVITO_GUI_EXPORT BooleanRadioButtonParameterUI : public PropertyParameterUI
{
    OVITO_CLASS(BooleanRadioButtonParameterUI)
    Q_OBJECT

public:

    /// Constructor.
    void initializeObject(PropertiesEditor* parentEditor, const PropertyFieldDescriptor* propField);

    /// Destructor.
    ~BooleanRadioButtonParameterUI();

    /// This returns the radio button group managed by this ParameterUI.
    QButtonGroup* buttonGroup() const { return _buttonGroup; }

    /// This returns the radio button for the "False" state.
    QRadioButton* buttonFalse() const { return buttonGroup() ? (QRadioButton*)buttonGroup()->button(0) : nullptr; }

    /// This returns the radio button for the "True" state.
    QRadioButton* buttonTrue() const { return buttonGroup() ? (QRadioButton*)buttonGroup()->button(1) : nullptr; }

    /// This method is called when a new editable object has been assigned to the properties owner this
    /// parameter UI belongs to.
    virtual void resetUI() override;

    /// This method updates the displayed value of the property UI.
    virtual void updateUI() override;

    /// Sets the enabled state of the UI.
    virtual void setEnabled(bool enabled) override;

public:

    Q_PROPERTY(QButtonGroup buttonGroup READ buttonGroup)

public Q_SLOTS:

    /// Takes the value entered by the user and stores it in the property field
    /// this property UI is bound to.
    void updatePropertyValue();

protected:

    /// The radio button group.
    QPointer<QButtonGroup> _buttonGroup;
};

}   // End of namespace
