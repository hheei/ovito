// SPDX-FileCopyrightText: 2026 OVITO GmbH, Germany
// SPDX-License-Identifier: GPL-3.0-only OR MIT

#pragma once


#include <ovito/gui/desktop/GUI.h>
#include <ovito/gui/desktop/widgets/general/SpinnerWidget.h>
#include "PropertyParameterUI.h"

namespace Ovito {

/******************************************************************************
* Base class for UI components that allow the user to edit a numerical
* property of an object via a spinner widget and a text box.
******************************************************************************/
class OVITO_GUI_EXPORT NumericalParameterUI : public PropertyParameterUI
{
    OVITO_CLASS(NumericalParameterUI)
    Q_OBJECT

public:

    /// Constructor.
    void initializeObject(PropertiesEditor* parentEditor, const PropertyFieldDescriptor* propField, const QMetaObject* defaultParameterUnitType);

    /// Destructor.
    ~NumericalParameterUI();

    /// Returns a label for the control widget managed by this ParameterUI.
    QLabel* label() const { return _label; }

    /// Returns the spinner widget managed by this ParameterUI.
    SpinnerWidget* spinner() const { return _spinner; }

    /// Returns the text box managed by this ParameterUI.
    QLineEdit* textBox() const { return _textBox; }

    /// Returns the button, which invokes the animation key editor for this animatable parameter.
    QAbstractButton* animateButton() const { return _animateButton; }

    /// Creates a QLayout that contains the text box, the spinner widget, the animate button (if parameter is animatable).
    QLayout* createFieldLayout();

    /// Returns the type of unit conversion service, which is used to format the parameter value as a text string.
    const QMetaObject* parameterUnitType() const { return _parameterUnitType; }

    /// This method is called when a new editable object has been assigned to the properties owner this
    /// parameter UI belongs to.
    virtual void resetUI() override;

    /// Sets the enabled state of the UI.
    virtual void setEnabled(bool enabled) override;

    /// Sets the tooltip text for the text box and the label widget.
    void setToolTip(const QString& text) const {
        if(label()) label()->setToolTip(text);
        if(textBox()) textBox()->setToolTip(text);
    }

    /// Sets the What's This helper text for the label, textbox, and the spinner.
    void setWhatsThis(const QString& text) const {
        if(label()) label()->setWhatsThis(text);
        if(textBox()) textBox()->setWhatsThis(text);
        if(spinner()) spinner()->setWhatsThis(text);
    }

public:

    Q_PROPERTY(SpinnerWidget spinner READ spinner)
    Q_PROPERTY(QLineEdit textBox READ textBox)
    Q_PROPERTY(QLabel label READ label)
    Q_PROPERTY(QAbstractButton animateButton READ animateButton)

public Q_SLOTS:

    /// Takes the value entered by the user and stores it in the property field
    /// this property UI is bound to. This method must be implemented by derived classes.
    virtual void updatePropertyValue() = 0;

    /// Shows or hides all widgets.
    void setVisible(bool visible);

protected:

    /// The spinner UI component.
    QPointer<SpinnerWidget> _spinner;

    /// The text box UI component.
    QPointer<QLineEdit> _textBox;

    /// The label UI component.
    QPointer<QLabel> _label;

    /// The button for editing animatable parameters.
    QPointer<QAbstractButton> _animateButton;

    /// The widget layout managed by this component.
    QPointer<QHBoxLayout> _layout;

    /// The type of unit conversion service, which is used to format the parameter value as a text string.
    const QMetaObject* _parameterUnitType;

private:

    /// Creates the widgets for this property UI.
    void initUIControls(const QString& labelText);
};

}   // End of namespace
