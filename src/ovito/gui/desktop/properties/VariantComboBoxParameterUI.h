// SPDX-FileCopyrightText: 2026 OVITO GmbH, Germany
// SPDX-License-Identifier: GPL-3.0-only OR MIT

#pragma once


#include <ovito/gui/desktop/GUI.h>
#include "PropertyParameterUI.h"

namespace Ovito {

/******************************************************************************
* This UI lets the user change a property using a combo box widget.
******************************************************************************/
class OVITO_GUI_EXPORT VariantComboBoxParameterUI : public PropertyParameterUI
{
    OVITO_CLASS(VariantComboBoxParameterUI)
    Q_OBJECT

public:

    /// Constructor.
    void initializeObject(PropertiesEditor* parentEditor, const PropertyFieldDescriptor* propField);

    /// Destructor.
    virtual ~VariantComboBoxParameterUI();

    /// This returns the combo box managed by this ParameterUI.
    QComboBox* comboBox() const { return _comboBox; }

    /// This method is called when a new editable object has been assigned to the properties owner this
    /// parameter UI belongs to.
    virtual void resetUI() override;

    /// This method updates the displayed value of the property UI.
    virtual void updateUI() override;

    /// Sets the enabled state of the UI.
    virtual void setEnabled(bool enabled) override;

    /// Sets the tooltip text for the combo box widget.
    void setToolTip(const QString& text) const {
        if(comboBox()) comboBox()->setToolTip(text);
    }

    /// Sets the What's This helper text for the combo box.
    void setWhatsThis(const QString& text) const {
        if(comboBox()) comboBox()->setWhatsThis(text);
    }

public:

    Q_PROPERTY(QComboBox comboBox READ comboBox)

public Q_SLOTS:

    /// Takes the value entered by the user and stores it in the property field
    /// this property UI is bound to.
    void updatePropertyValue();

protected:

    /// The combo box of the UI component.
    QPointer<QComboBox> _comboBox;
};

}   // End of namespace
