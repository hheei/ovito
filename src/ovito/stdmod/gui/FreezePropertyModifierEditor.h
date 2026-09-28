// SPDX-FileCopyrightText: 2026 OVITO GmbH, Germany
// SPDX-License-Identifier: GPL-3.0-only OR MIT

#pragma once


#include <ovito/stdmod/gui/StdModGui.h>
#include <ovito/gui/desktop/properties/PropertiesEditor.h>

namespace Ovito {

/**
 * A properties editor for the FreezePropertyModifier class.
 */
class FreezePropertyModifierEditor : public PropertiesEditor
{
    OVITO_CLASS(FreezePropertyModifierEditor)
    Q_OBJECT

protected:

    /// Creates the user interface controls for the editor.
    virtual void createUI(const RolloutInsertionParameters& rolloutParams) override;

private Q_SLOTS:

    /// Is called when the user has selected a different source property.
    void onSourcePropertyChanged();
};

}   // End of namespace
