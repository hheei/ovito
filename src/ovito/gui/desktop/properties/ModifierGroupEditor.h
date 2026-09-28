// SPDX-FileCopyrightText: 2026 OVITO GmbH, Germany
// SPDX-License-Identifier: GPL-3.0-only OR MIT

#pragma once


#include <ovito/gui/desktop/GUI.h>
#include <ovito/gui/desktop/properties/PropertiesEditor.h>

namespace Ovito {

/**
 * \brief The properties editor for the ModifierGroup class.
 */
class ModifierGroupEditor : public PropertiesEditor
{
    OVITO_CLASS(ModifierGroupEditor)

protected:

    /// Creates the user interface controls for the editor.
    virtual void createUI(const RolloutInsertionParameters& rolloutParams) override;

    /// This method is called when a reference target changes.
    virtual bool referenceEvent(RefTarget* source, const ReferenceEvent& event) override;

private Q_SLOTS:

    /// Rebuilds the list of sub-editors for the group's modifier applications.
    void updateSubEditors();

private:

    /// The editors for the group's modifier applications.
    std::vector<OORef<PropertiesEditor>> _subEditors;

    /// Specifies where the sub-editors are opened and whether the sub-editors are opened in a collapsed state.
    RolloutInsertionParameters _rolloutParams;
};

}   // End of namespace
