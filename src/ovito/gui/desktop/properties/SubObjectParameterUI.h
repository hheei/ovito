// SPDX-FileCopyrightText: 2026 OVITO GmbH, Germany
// SPDX-License-Identifier: GPL-3.0-only OR MIT

#pragma once


#include <ovito/gui/desktop/GUI.h>
#include "PropertyParameterUI.h"
#include "PropertiesEditor.h"

namespace Ovito {

/******************************************************************************
* This parameter UI will open up a sub-editor for an object that is
* referenced by the edit object.
******************************************************************************/
class OVITO_GUI_EXPORT SubObjectParameterUI : public PropertyParameterUI
{
    OVITO_CLASS(SubObjectParameterUI)
    Q_OBJECT

public:

    /// Constructor.
    void initializeObject(PropertiesEditor* parentEditor, const PropertyFieldDescriptor* refField, const RolloutInsertionParameters& rolloutParams = RolloutInsertionParameters());

    /// This method is called when a new sub-object has been assigned to the reference field of the editable object
    /// this parameter UI is bound to. It is also called when the editable object itself has
    /// been replaced in the editor.
    virtual void resetUI() override;

    /// Returns the current sub-editor or NULL if there is none.
    PropertiesEditor* subEditor() const { return _subEditor; }

    /// Sets the enabled state of the UI.
    virtual void setEnabled(bool enabled) override;

protected:

    /// The editor for the referenced sub-object.
    OORef<PropertiesEditor> _subEditor;

    /// Controls where the sub-editor is opened and whether the sub-editor is opened in a collapsed state.
    RolloutInsertionParameters _rolloutParams;
};

}   // End of namespace
