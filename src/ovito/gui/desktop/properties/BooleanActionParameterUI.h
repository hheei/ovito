// SPDX-FileCopyrightText: 2026 OVITO GmbH, Germany
// SPDX-License-Identifier: GPL-3.0-only OR MIT

#pragma once


#include <ovito/gui/desktop/GUI.h>
#include "PropertyParameterUI.h"

namespace Ovito {

/******************************************************************************
* This UI allows the user to change a boolean property of the object being edited.
******************************************************************************/
class OVITO_GUI_EXPORT BooleanActionParameterUI : public PropertyParameterUI
{
    OVITO_CLASS(BooleanActionParameterUI)
    Q_OBJECT

public:

    /// Constructor.
    void initializeObject(PropertiesEditor* parentEditor, const PropertyFieldDescriptor* propField, QAction* action);

    /// This returns the action associated with this parameter UI.
    QAction* action() const { return _action; }

    /// This method is called when a new editable object has been assigned to the properties owner this
    /// parameter UI belongs to.
    virtual void resetUI() override;

    /// This method updates the displayed value of the property UI.
    virtual void updateUI() override;

    /// Sets the enabled state of the UI.
    virtual void setEnabled(bool enabled) override;

public:

    Q_PROPERTY(QAction action READ action)

public Q_SLOTS:

    /// Takes the value entered by the user and stores it in the property field
    /// this property UI is bound to.
    void updatePropertyValue();

protected:

    /// The check box of the UI component.
    QPointer<QAction> _action;
};

}   // End of namespace
