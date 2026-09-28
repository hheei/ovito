// SPDX-FileCopyrightText: 2026 OVITO GmbH, Germany
// SPDX-License-Identifier: GPL-3.0-only OR MIT

#pragma once


#include <ovito/gui/desktop/GUI.h>
#include <ovito/gui/desktop/properties/PropertiesEditor.h>

namespace Ovito {

/**
 * \brief A properties editor for the SurfaceMeshVis class.
 */
class SurfaceMeshVisEditor : public PropertiesEditor
{
    OVITO_CLASS(SurfaceMeshVisEditor)

protected:

    /// Creates the user interface controls for the editor.
    virtual void createUI(const RolloutInsertionParameters& rolloutParams) override;

private Q_SLOTS:

    /// Updates the coloring controls shown in the UI.
    void updateColoringOptions();

private:

    IntegerRadioButtonParameterUI* _coloringModeUI;
    ColorParameterUI* _surfaceColorUI;
    SubObjectParameterUI* _colorMappingParamUI;
    BooleanGroupBoxParameterUI* _capGroupUI;
    BooleanParameterUI* _clipAtDomainBoundariesUI;
};

}   // End of namespace
