// SPDX-FileCopyrightText: 2026 OVITO GmbH, Germany
// SPDX-License-Identifier: GPL-3.0-only OR MIT

#pragma once


#include <ovito/gui/desktop/GUI.h>
#include "PropertyParameterUI.h"

namespace Ovito {

/******************************************************************************
* This UI allows the user to select a filename as property value.
******************************************************************************/
class OVITO_GUI_EXPORT FilenameParameterUI : public PropertyParameterUI
{
    OVITO_CLASS(FilenameParameterUI)
    Q_OBJECT

public:

    /// Constructor.
    void initializeObject(PropertiesEditor* parentEditor, const PropertyFieldDescriptor* propField, const QStringList& fileFilter, bool existingFile);

    /// Destructor.
    ~FilenameParameterUI();

    /// This returns the button managed by this ParameterUI.
    QPushButton* selectorWidget() const { return _selectorButton; }

    /// This method is called when a new editable object has been assigned to the properties owner this
    /// parameter UI belongs to.
    virtual void resetUI() override;

    /// This method updates the displayed value of the property UI.
    virtual void updateUI() override;

    /// Sets the enabled state of the UI.
    virtual void setEnabled(bool enabled) override;

    /// Sets the What's This helper text for the selector widget.
    void setWhatsThis(const QString& text) const {
        if(selectorWidget()) selectorWidget()->setWhatsThis(text);
    }

public:

    Q_PROPERTY(QPushButton selectorWidget READ selectorWidget)

private Q_SLOTS:

    /// Is called when the user presses the button.
    void onPickFilename();

protected:

    /// The selector control.
    QPointer<QPushButton> _selectorButton;

    /// List of file type filters.
    QStringList _fileFilter;

    /// Flag indicating whether the selected file must already exist.
    bool _existingFile;
};

}   // End of namespace
