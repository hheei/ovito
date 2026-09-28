// SPDX-FileCopyrightText: 2026 OVITO GmbH, Germany
// SPDX-License-Identifier: GPL-3.0-only OR MIT

#pragma once


#include <ovito/gui/desktop/GUI.h>
#include "PropertyParameterUI.h"

namespace Ovito {

/******************************************************************************
* Allows the user to edit a boolean parameter.
* The check box is displayed in the title bar of a QGroupBox.
******************************************************************************/
class OVITO_GUI_EXPORT BooleanGroupBoxParameterUI : public PropertyParameterUI
{
    OVITO_CLASS(BooleanGroupBoxParameterUI)
    Q_OBJECT

public:

    /// Constructor.
    void initializeObject(PropertiesEditor* parentEditor, const PropertyFieldDescriptor* propField);

    /// Destructor, which releases all GUI widgets.
    ~BooleanGroupBoxParameterUI();

    /// This returns the group box managed by this parameter UI.
    QGroupBox* groupBox() const { return _groupBox; }

    /// This returns the container managed by this parameter UI where child widgets should be inserted into.
    QWidget* childContainer() const { return _childContainer; }

    /// This method is called when a new editable object has been assigned to the properties owner this
    /// parameter UI belongs to.
    virtual void resetUI() override;

    /// This method updates the displayed value of the property UI.
    virtual void updateUI() override;

    /// Sets the enabled state of the UI.
    virtual void setEnabled(bool enabled) override;

    /// Sets the tooltip text for the check box.
    void setToolTip(const QString& text) const { if(groupBox()) groupBox()->setToolTip(text); }

    /// Sets the What's This helper text for the check box.
    void setWhatsThis(const QString& text) const { if(groupBox()) groupBox()->setWhatsThis(text); }

public:

    Q_PROPERTY(QGroupBox groupBox READ groupBox)

public Q_SLOTS:

    /// Takes the value entered by the user and stores it in the property field
    /// this property UI is bound to.
    void updatePropertyValue();

protected:

    /// The QGroupBox control of the UI component.
    QPointer<QGroupBox> _groupBox;

    /// A container widget where child widgets should be inserted into.
    QWidget* _childContainer;
};

}   // End of namespace
