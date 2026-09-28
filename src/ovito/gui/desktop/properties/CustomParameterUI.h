// SPDX-FileCopyrightText: 2026 OVITO GmbH, Germany
// SPDX-License-Identifier: GPL-3.0-only OR MIT

#pragma once


#include <ovito/gui/desktop/GUI.h>
#include "PropertyParameterUI.h"

namespace Ovito {

/******************************************************************************
* Utility class for creating UIs for custom parameter types.
******************************************************************************/
class OVITO_GUI_EXPORT CustomParameterUI : public PropertyParameterUI
{
    OVITO_CLASS(CustomParameterUI)
    Q_OBJECT

public:

    /// Constructor.
    void initializeObject(PropertiesEditor* parentEditor, const PropertyFieldDescriptor* propField, QWidget* widget,
            std::function<void(const QVariant&)>&& updateWidgetFunction,
            std::function<QVariant()>&& updatePropertyFunction,
            std::function<void(RefTarget*)>&& resetUIFunction = std::function<void(RefTarget*)>());

    /// Destructor.
    ~CustomParameterUI();

    /// This returns the widget managed by this ParameterUI.
    QWidget* widget() const { return _widget; }

    /// This method is called when a new editable object has been assigned to the properties owner this
    /// parameter UI belongs to.
    virtual void resetUI() override;

    /// This method updates the displayed value of the property UI.
    virtual void updateUI() override;

    /// Sets the enabled state of the UI.
    virtual void setEnabled(bool enabled) override;

    /// Sets the tooltip text for the widget.
    void setToolTip(const QString& text) const {
        if(widget()) widget()->setToolTip(text);
    }

    /// Sets the What's This helper text for the widget.
    void setWhatsThis(const QString& text) const {
        if(widget()) widget()->setWhatsThis(text);
    }

public:

    Q_PROPERTY(QWidget widget READ widget)

public Q_SLOTS:

    /// Takes the value entered by the user and stores it in the property field this property UI is bound to.
    void updatePropertyValue();

protected:

    /// The widget managed by this UI component.
    QPointer<QWidget> _widget;

    /// This function is called when the property value has changed and the UI widget needs to be updated.
    std::function<void(const QVariant&)> _updateWidgetFunction;

    /// This function is called when the user has manipulated the UI widget and the property needs to be set accordingly.
    std::function<QVariant()> _updatePropertyFunction;

    /// This function is called when a new object is loaded into the editor.
    std::function<void(RefTarget*)> _resetUIFunction;
};

}   // End of namespace
