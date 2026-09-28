// SPDX-FileCopyrightText: 2026 OVITO GmbH, Germany
// SPDX-License-Identifier: GPL-3.0-only OR MIT

#pragma once


#include <ovito/particles/gui/ParticlesGui.h>
#include <ovito/stdobj/gui/widgets/DataTablePlotWidget.h>
#include <ovito/gui/desktop/properties/PropertiesEditor.h>

namespace Ovito {

/**
 * \brief A properties editor for the CentroSymmetryModifier class.
 */
class CentroSymmetryModifierEditor : public PropertiesEditor
{
    OVITO_CLASS(CentroSymmetryModifierEditor)
    Q_OBJECT

protected Q_SLOTS:

    /// Replots the histogram computed by the modifier.
    void plotHistogram();

private:

    /// The graph widget to display the CSP histogram.
    DataTablePlotWidget* _cspPlotWidget;

protected:

    /// Creates the user interface controls for the editor.
    virtual void createUI(const RolloutInsertionParameters& rolloutParams) override;
};

}   // End of namespace
