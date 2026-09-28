// SPDX-FileCopyrightText: 2026 OVITO GmbH, Germany
// SPDX-License-Identifier: GPL-3.0-only OR MIT

#pragma once


#include <ovito/gui/desktop/GUI.h>
#include "PropertyParameterUI.h"

namespace Ovito {

/******************************************************************************
* This UI lets the user change an integer-value property of the object
* being edited using a set of radio buttons.
******************************************************************************/
class OVITO_GUI_EXPORT IntegerRadioButtonParameterUI : public PropertyParameterUI
{
    OVITO_CLASS(IntegerRadioButtonParameterUI)
    Q_OBJECT

public:

    /// Constructor.
    void initializeObject(PropertiesEditor* parentEditor, const PropertyFieldDescriptor* propField);

    /// This returns the radio button group managed by this ParameterUI.
    QButtonGroup* buttonGroup() const { return _buttonGroup; }

    /// Creates a new radio button widget that can be selected by the user
    /// to set the property value to the given value.
    QRadioButton* addRadioButton(int value, const QString& caption = QString());

    /// This method is called when a new editable object has been assigned to the properties owner this
    /// parameter UI belongs to.
    virtual void resetUI() override;

    /// This method updates the displayed value of the property UI.
    virtual void updateUI() override;

    /// Sets the enabled state of the UI.
    virtual void setEnabled(bool enabled) override;

    /// Sets the tooltip text for the radio button widgets.
    void setToolTip(const QString& text) const {
        if(buttonGroup()) {
            for(QAbstractButton* button : buttonGroup()->buttons())
                button->setToolTip(text);
        }
    }

    /// Sets the What's This helper text for the radio button widgets.
    void setWhatsThis(const QString& text) const {
        if(buttonGroup()) {
            for(QAbstractButton* button : buttonGroup()->buttons())
                button->setWhatsThis(text);
        }
    }

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
