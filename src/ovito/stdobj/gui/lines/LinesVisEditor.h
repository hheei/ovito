// SPDX-FileCopyrightText: 2026 OVITO GmbH, Germany
// SPDX-License-Identifier: GPL-3.0-only OR MIT

#pragma once

#include <ovito/particles/gui/ParticlesGui.h>
#include <ovito/gui/desktop/properties/PropertiesEditor.h>

namespace Ovito {

/**
 * \brief A properties editor for the LinesVis class.
 */
class OVITO_STDOBJGUI_EXPORT LinesVisEditor : public PropertiesEditor
{
    OVITO_CLASS(LinesVisEditor)

protected:

    /// Creates the user interface controls for the editor.
    virtual void createUI(const RolloutInsertionParameters& rolloutParams) override;

private Q_SLOTS:

    /// Updates the coloring controls shown in the UI.
    void updateColoringOptions();

private:

    IntegerRadioButtonParameterUI* _coloringModeUI;
    ColorParameterUI* _lineColorUI;
    SubObjectParameterUI* _colorMappingParamUI;
    BooleanParameterUI* _showUpToCurrentTimeUI;
};

}  // namespace Ovito
