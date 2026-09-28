// SPDX-FileCopyrightText: 2026 OVITO GmbH, Germany
// SPDX-License-Identifier: GPL-3.0-only OR MIT

#pragma once


#include <ovito/gui/desktop/GUI.h>
#include <ovito/gui/desktop/properties/PropertiesEditor.h>

namespace Ovito {

/**
 * \brief The default properties editor used for RefTarget-derived classes if they do not define their own editor type.
 */
class OVITO_GUI_EXPORT DefaultPropertiesEditor : public PropertiesEditor
{
    OVITO_CLASS(DefaultPropertiesEditor)

public:

    /// Returns the list of editors for the referenced sub-objects.
    const auto& subEditors() const { return _subEditors; }

protected:

    /// Creates the user interface controls for the editor.
    virtual void createUI(const RolloutInsertionParameters& rolloutParams) override;

    /// This method is called when a reference target changes.
    virtual bool referenceEvent(RefTarget* source, const ReferenceEvent& event) override;

    /// Is called when the value of a reference field of this RefMaker changes.
    virtual void referenceReplaced(const PropertyFieldDescriptor* field, RefTarget* oldTarget, RefTarget* newTarget, int listIndex) override;

private:

    /// Rebuilds the list of sub-editors for the current edit object.
    void updateSubEditors();

    /// The editors for the referenced sub-objects.
    std::vector<OORef<PropertiesEditor>> _subEditors;

    /// Specifies where the sub-editors are opened and whether the sub-editors are opened in a collapsed state.
    RolloutInsertionParameters _rolloutParams;
};

}   // End of namespace
