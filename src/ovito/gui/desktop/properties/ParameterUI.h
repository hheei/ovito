// SPDX-FileCopyrightText: 2026 OVITO GmbH, Germany
// SPDX-License-Identifier: GPL-3.0-only OR MIT

#pragma once


#include <ovito/gui/desktop/GUI.h>
#include <ovito/core/oo/RefTarget.h>
#include <ovito/core/app/undo/UndoableTransaction.h>
#include "PropertiesEditor.h"

namespace Ovito {

/**
 * \brief Base class for UI components that allow the user to edit a parameter
 *        of a RefTarget derived object in the PropertiesEditor.
 */
class OVITO_GUI_EXPORT ParameterUI : public QObject, public RefMaker, public UserInterfaceComponent<MainWindowUI>
{
    OVITO_CLASS(ParameterUI)
    Q_OBJECT

public:

    /// Constructor.
    void initializeObject(PropertiesEditor* editor);

    /// This method gets called by OORef<T>::create() right after the object has been fully initialized.
    void completeObjectInitialization();

    /// Returns the properties editor hosting this component.
    PropertiesEditor* editor() const { return _editor; }

    /// Returns the enabled state of the component.
    bool isEnabled() const { return _enabled; }

    /// Returns the disabled state of the component. This is simply the inverse of the enabled state.
    bool isDisabled() const { return !isEnabled(); }

public:

    Q_PROPERTY(Ovito::RefTarget* editObject READ editObject)
    Q_PROPERTY(bool isEnabled READ isEnabled WRITE setEnabled)
    Q_PROPERTY(bool isDisabled READ isDisabled WRITE setDisabled)

Q_SIGNALS:

    /// This signal is emitted when the user is changing the value of the parameter by manipulating the UI widget.
    /// It is not emitted when the parameter value has been changed programmatically.
    void valueEntered();

public Q_SLOTS:

    /// \brief This method is called when a new editable object has been assigned to the properties owner
    ///       this parameter UI belongs to.
    ///
    /// The parameter UI should react to this change appropriately and
    /// show the properties value for the new edit object in the UI. The default implementation
    /// of this method just calls updateUI() to reflect the change.
    virtual void resetUI() { updateUI(); }

    /// \brief This method updates the displayed value of the parameter UI.
    ///
    /// This method should be overridden by derived classes.
    virtual void updateUI() {}

    /// \brief Sets the enabled state of the UI.
    virtual void setEnabled(bool enabled) { _enabled = enabled; }

    /// \brief Sets the enabled state of the UI. This is just the reverse of setEnabled().
    void setDisabled(bool disabled) { setEnabled(!disabled); }

    /// \brief Sets the object whose property is being displayed in this parameter UI.
    virtual void setEditObject(RefTarget* newObject) {
        _editObject.set(this, PROPERTY_FIELD(editObject), newObject);
        resetUI();
    }

private:

    /// The object whose parameters are being edited.
    DECLARE_REFERENCE_FIELD(OORef<RefTarget>, editObject);

    /// The editor hosting this parameter UI.
    PropertiesEditor* _editor;

    /// Indicates whether this UI component is currently enabled or disabled.
    bool _enabled = true;
};

}   // End of namespace
