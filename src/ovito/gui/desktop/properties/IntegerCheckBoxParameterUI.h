// SPDX-FileCopyrightText: 2026 OVITO GmbH, Germany
// SPDX-License-Identifier: GPL-3.0-only OR MIT

#pragma once


#include <ovito/gui/desktop/GUI.h>
#include "PropertyParameterUI.h"

namespace Ovito {

/******************************************************************************
* This UI element lets the user change an integer-value property of the object
* being edited using a check box.
******************************************************************************/
class OVITO_GUI_EXPORT IntegerCheckBoxParameterUI : public PropertyParameterUI
{
    OVITO_CLASS(IntegerCheckBoxParameterUI)
    Q_OBJECT

public:

    /// Constructor.
    void initializeObject(PropertiesEditor* parentEditor, const PropertyFieldDescriptor* propField, int uncheckedValue, int checkedValue);

    /// Destructor.
    ~IntegerCheckBoxParameterUI();

    /// This returns the checkbox managed by this parameter UI.
    QCheckBox* checkBox() const { return _checkBox; }

    /// Changes the parameter value that represents the unchecked state.
    void setUncheckedValue(int uncheckedValue) {
        if(_uncheckedValue != uncheckedValue) {
            _uncheckedValue = uncheckedValue;
            updateUI();
        }
    }

    /// Changes the parameter value that represents the checked state.
    void setCheckedValue(int checkedValue) {
        if(_checkedValue != checkedValue) {
            _checkedValue = checkedValue;
            updateUI();
        }
    }

    /// This method is called when a new editable object has been assigned to the properties owner this
    /// parameter UI belongs to.
    virtual void resetUI() override;

    /// This method updates the displayed value of the property UI.
    virtual void updateUI() override;

    /// Sets the enabled state of the UI.
    virtual void setEnabled(bool enabled) override;

    /// Sets the tooltip text for the check box.
    void setToolTip(const QString& text) const { if(checkBox()) checkBox()->setToolTip(text); }

    /// Sets the What's This helper text for the check box.
    void setWhatsThis(const QString& text) const { if(checkBox()) checkBox()->setWhatsThis(text); }

public:

    Q_PROPERTY(QCheckBox checkBox READ checkBox)

public Q_SLOTS:

    /// Takes the value entered by the user and stores it in the property field
    /// this property UI is bound to.
    void updatePropertyValue();

protected:

    /// The check box of the UI component.
    QPointer<QCheckBox> _checkBox;

    /// The parameter value that represents the unchecked state.
    int _uncheckedValue = 0;

    /// The parameter value that represents the checked state.
    int _checkedValue = 1;
};

}   // End of namespace
