// SPDX-FileCopyrightText: 2026 OVITO GmbH, Germany
// SPDX-License-Identifier: GPL-3.0-only OR MIT

#pragma once


#include <ovito/stdmod/gui/StdModGui.h>
#include <ovito/gui/desktop/properties/PropertiesEditor.h>
#include <ovito/stdmod/modifiers/ClearSelectionModifier.h>

namespace Ovito {

/**
 * A properties editor for the ClearSelectionModifier class.
 */
class ClearSelectionModifierEditor : public PropertiesEditor
{
    OVITO_CLASS(ClearSelectionModifierEditor)

protected:

    /// Creates the user interface controls for the editor.
    virtual void createUI(const RolloutInsertionParameters& rolloutParams) override;
};

}   // End of namespace
