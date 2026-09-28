// SPDX-FileCopyrightText: 2026 OVITO GmbH, Germany
// SPDX-License-Identifier: GPL-3.0-only OR MIT

#pragma once


#include <ovito/gui/desktop/GUI.h>
#include <ovito/gui/desktop/widgets/general/StableComboBox.h>
#include "ParameterUI.h"

namespace Ovito {

/******************************************************************************
* A list view that shows the fixed set of delegates of a MultiDelegatingModifier,
* which can each be enabled or disabled by the user.
******************************************************************************/
class OVITO_GUI_EXPORT ModifierDelegateVariableListParameterUI : public ParameterUI
{
    OVITO_CLASS(ModifierDelegateVariableListParameterUI)
    Q_OBJECT

public:

    /// Constructor.
    void initializeObject(PropertiesEditor* parentEditor, const OvitoClass& delegateType);

    /// Destructor.
    ~ModifierDelegateVariableListParameterUI();

    /// This method is called when a new editable object has been assigned to the properties owner this
    /// parameter UI belongs to. The parameter UI should react to this change appropriately and
    /// show the properties value for the new edit object in the UI.
    virtual void resetUI() override;

    /// This method updates the displayed value of the parameter UI.
    virtual void updateUI() override;

    /// This returns the container widget managed by this class.
    QWidget* containerWidget() const { return _containerWidget; }

    /// Sets the enabled state of the UI.
    virtual void setEnabled(bool enabled) override;

private Q_SLOTS:

    /// Is called when the user requested the addition of a new delegate to the modifier.
    void onAddDelegate();

    /// Is called when the user requested the removal of an existing delegate from the modifier.
    void onRemoveDelegate();

    /// Is called when the user selects an entry in a combo box.
    void onDelegateSelected(int index);

protected:

    /// This method is called when a reference target changes.
    virtual bool referenceEvent(RefTarget* source, const ReferenceEvent& event) override;

    /// Is called when a RefTarget has been added to a VectorReferenceField of this RefMaker.
    virtual void referenceInserted(const PropertyFieldDescriptor* field, RefTarget* newTarget, int listIndex) override;

    /// Is called when a RefTarget has been removed from a VectorReferenceField of this RefMaker.
    virtual void referenceRemoved(const PropertyFieldDescriptor* field, RefTarget* oldTarget, int listIndex) override;

    /// Is called when a RefTarget has been replaced in a VectorReferenceField of this RefMaker.
    virtual void referenceReplaced(const PropertyFieldDescriptor* field, RefTarget* oldTarget, RefTarget* newTarget, int listIndex) override;

private:

    /// The type of modifier delegates, which the user can choose from.
    OvitoClassPtr _delegateType;

    /// The container widget managed by this parameter UI.
    QPointer<QWidget> _containerWidget;

    /// The QAction for each delegate that removes it.
    QVector<QAction*> _removeDelegateActions;

    /// The QComboBox for each delegate.
    QVector<StableComboBox*> _delegateBoxes;

    /// The current list of delegates.
    DECLARE_VECTOR_REFERENCE_FIELD(OORef<ModifierDelegate>, delegates);
};

}   // End of namespace
