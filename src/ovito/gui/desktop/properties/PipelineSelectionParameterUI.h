// SPDX-FileCopyrightText: 2026 OVITO GmbH, Germany
// SPDX-License-Identifier: GPL-3.0-only OR MIT

#pragma once


#include <ovito/gui/desktop/GUI.h>
#include "PropertyParameterUI.h"

namespace Ovito {

/**
 * \brief UI component that lets the user select a pipeline from the current scene.
 */
class OVITO_GUI_EXPORT PipelineSelectionParameterUI : public PropertyParameterUI
{
    OVITO_CLASS(PipelineSelectionParameterUI)
    Q_OBJECT

public:

    /// Constructor.
    void initializeObject(PropertiesEditor* parentEditor, const PropertyFieldDescriptor* propField);

    /// Destructor.
    ~PipelineSelectionParameterUI();

    /// This returns the combo box managed by this ParameterUI.
    QComboBox* comboBox() const { return _comboBox; }

    /// Sets the enabled state of the UI.
    virtual void setEnabled(bool enabled) override;

    /// This method updates the displayed value of the parameter UI.
    virtual void updateUI() override;

    /// This method is called when a new editable object has been assigned to the properties owner this
    /// parameter UI belongs to.
    virtual void resetUI() override;

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

    /// This method is called when a reference target changes.
    virtual bool referenceEvent(RefTarget* source, const ReferenceEvent& event) override;

    /// The combo box of the UI component.
    QPointer<QComboBox> _comboBox;
};

}   // End of namespace
