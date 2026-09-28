// SPDX-FileCopyrightText: 2026 OVITO GmbH, Germany
// SPDX-License-Identifier: GPL-3.0-only OR MIT

#pragma once


#include <ovito/stdmod/gui/StdModGui.h>
#include <ovito/gui/desktop/properties/PropertiesEditor.h>
#include <ovito/core/utilities/units/PrescribedScaleUnit.h>

namespace Ovito {

/**
 * A properties editor for the AffineTransformationModifier class.
 */
class AffineTransformationModifierEditor : public PropertiesEditor
{
    Q_OBJECT
    OVITO_CLASS(AffineTransformationModifierEditor)

protected:

    /// Creates the user interface controls for the editor.
    virtual void createUI(const RolloutInsertionParameters& rolloutParams) override;

private Q_SLOTS:

    /// Is called when the spinner value has changed.
    void onSpinnerValueChanged();

    /// This method updates the displayed matrix values.
    void updateUI();

    /// Is called when the user switches between Cartesian and reduced cell coordinates for the translation vector.
    void onReducedCoordinatesOptionChanged();

    /// Is called when the user presses the 'Enter rotation' button.
    void onEnterRotation();

    /// Auto-adjusts the increment steps of the numeric parameter spinner widgets.
    void updateParameterUnitScales();

private:

    SpinnerWidget* _relativeCellSpinners[3][4];
    std::optional<PrescribedScaleUnit> _relativeTranslationUnits[3];
    std::optional<PrescribedScaleUnit> _relativeMatrixUnits;
    std::optional<PrescribedScaleUnit> _absoluteCellUnits[3][3];
    std::optional<PrescribedScaleUnit> _absoluteOriginUnits[3];
    BooleanRadioButtonParameterUI* _relativeModeUI;
};

}   // End of namespace
