// SPDX-FileCopyrightText: 2026 OVITO GmbH, Germany
// SPDX-License-Identifier: GPL-3.0-only OR MIT

#pragma once


#include <ovito/gui/desktop/GUI.h>
#include "PropertyParameterUI.h"

namespace Ovito {

/******************************************************************************
* This UI allows the user to edit a string property of the object being edited.
******************************************************************************/
class OVITO_GUI_EXPORT StringParameterUI : public PropertyParameterUI
{
    OVITO_CLASS(StringParameterUI)
    Q_OBJECT

public:

    /// Constructor.
    void initializeObject(PropertiesEditor* parentEditor, const PropertyFieldDescriptor* propField);

    /// Destructor.
    virtual ~StringParameterUI();

    /// Returns the text widget managed by this ParameterUI.
    QLineEdit* lineEdit() const { OVITO_ASSERT(qobject_cast<QLineEdit*>(textBox())); return static_cast<QLineEdit*>(textBox()); }

    /// Returns the widget managed by this ParameterUI.
    QWidget* textBox() const { return _textBox; }

    /// Replaces the text widget managed by this ParameterUI. The ParameterUI becomes the owner of the new widget.
    void setTextBox(QWidget* textBox);

    /// This method is called when a new editable object has been assigned to the properties owner this
    /// parameter UI belongs to.
    virtual void resetUI() override;

    /// This method updates the displayed value of the property UI.
    virtual void updateUI() override;

    /// Sets the enabled state of the UI.
    virtual void setEnabled(bool enabled) override;

    /// Sets the tooltip text for the text box.
    void setToolTip(const QString& text) const {
        if(textBox()) textBox()->setToolTip(text);
    }

    /// Sets the What's This helper text for the textbox.
    void setWhatsThis(const QString& text) const {
        if(textBox()) textBox()->setWhatsThis(text);
    }

public:

    Q_PROPERTY(QLineEdit* lineEdit READ lineEdit)
    Q_PROPERTY(QWidget* textBox READ textBox)

public Q_SLOTS:

    /// Takes the value entered by the user and stores it in the property field
    /// this property UI is bound to.
    void updatePropertyValue();

protected:

    /// The text box of the UI component.
    QPointer<QWidget> _textBox;
};

}   // End of namespace
