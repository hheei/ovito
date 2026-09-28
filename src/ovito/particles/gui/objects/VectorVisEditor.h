// SPDX-FileCopyrightText: 2026 OVITO GmbH, Germany
// SPDX-License-Identifier: GPL-3.0-only OR MIT

#pragma once


#include <ovito/particles/gui/ParticlesGui.h>
#include <ovito/gui/desktop/properties/PropertiesEditor.h>

namespace Ovito {

/**
 * \brief A properties editor for the VectorVis class.
 */
class VectorVisEditor : public PropertiesEditor
{
    OVITO_CLASS(VectorVisEditor)
    Q_OBJECT

protected:

    /// Creates the user interface controls for the editor.
    virtual void createUI(const RolloutInsertionParameters& rolloutParams) override;

private Q_SLOTS:

    /// Updates the coloring controls shown in the UI.
    void updateColoringOptions();

private:

    IntegerRadioButtonParameterUI* _coloringModeUI;
    ColorParameterUI* _arrowColorUI;
    SubObjectParameterUI* _colorMappingParamUI;
    FloatParameterUI* _transparencyUI;
};

}   // End of namespace
