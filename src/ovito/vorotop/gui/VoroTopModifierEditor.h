// SPDX-FileCopyrightText: 2026 OVITO GmbH, Germany
// SPDX-License-Identifier: GPL-3.0-only OR MIT

#pragma once


#include <ovito/vorotop/VoroTopPlugin.h>
#include <ovito/vorotop/VoroTopModifier.h>
#include <ovito/gui/desktop/properties/PropertiesEditor.h>

namespace Ovito::VoroTop {

/**
 * A properties editor for the VoroTopModifier class.
 */
class VoroTopModifierEditor : public PropertiesEditor
{
    OVITO_CLASS(VoroTopModifierEditor)
    Q_OBJECT

protected:

    /// Creates the user interface controls for the editor.
    virtual void createUI(const RolloutInsertionParameters& rolloutParams) override;
};

}   // End of namespace
