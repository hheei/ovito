// SPDX-FileCopyrightText: 2026 OVITO GmbH, Germany
// SPDX-License-Identifier: GPL-3.0-only OR MIT

#pragma once


#include <ovito/gui/desktop/GUI.h>
#include "PropertyParameterUI.h"

namespace Ovito {

/******************************************************************************
* Allows the user to pick a font.
******************************************************************************/
class OVITO_GUI_EXPORT FontParameterUI : public PropertyParameterUI
{
    OVITO_CLASS(FontParameterUI)
    Q_OBJECT

public:

    /// Constructor.
    void initializeObject(PropertiesEditor* parentEditor, const PropertyFieldDescriptor* propField);

    /// Destructor.
    ~FontParameterUI();

    /// This returns the font picker widget managed by this parameter UI.
    QPushButton* fontPicker() const { return _fontPicker; }

    /// This returns a label for the widget managed by this FontParameterUI.
    /// The text of the label widget is taken from the description text stored along
    /// with the property field.
    QLabel* label() const { return _label; }

    /// This method is called when a new editable object has been assigned to the properties owner this
    /// parameter UI belongs to.
    virtual void resetUI() override;

    /// This method updates the displayed value of the property UI.
    virtual void updateUI() override;

    /// Sets the enabled state of the UI.
    virtual void setEnabled(bool enabled) override;

    /// Sets the What's This helper text for the label and the color picker.
    void setWhatsThis(const QString& text) const {
        if(label()) label()->setWhatsThis(text);
        if(fontPicker()) fontPicker()->setWhatsThis(text);
    }

public Q_SLOTS:

    /// Is called when the user has pressed the font picker button.
    void onButtonClicked();

protected:

    /// The font picker widget of the UI component.
    QPointer<QPushButton> _fontPicker;

    /// The label of the UI component.
    QPointer<QLabel> _label;
};

}   // End of namespace
