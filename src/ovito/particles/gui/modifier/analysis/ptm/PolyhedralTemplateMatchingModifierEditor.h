// SPDX-FileCopyrightText: 2026 OVITO GmbH, Germany
// SPDX-License-Identifier: GPL-3.0-only OR MIT

#pragma once


#include <ovito/particles/gui/ParticlesGui.h>
#include <ovito/stdobj/gui/widgets/DataTablePlotWidget.h>
#include <ovito/gui/desktop/properties/PropertiesEditor.h>

class QwtPlotZoneItem;

namespace Ovito {

/**
 * \brief A properties editor for the PolyhedralTemplateMatchingModifier class.
 */
class PolyhedralTemplateMatchingModifierEditor : public PropertiesEditor
{
    OVITO_CLASS(PolyhedralTemplateMatchingModifierEditor)
    Q_OBJECT

protected Q_SLOTS:

    /// Replots the histogram computed by the modifier.
    void plotHistogram();

protected:

    /// Creates the user interface controls for the editor.
    virtual void createUI(const RolloutInsertionParameters& rolloutParams) override;

private:

    /// The graph widget to display the RMSD histogram.
    DataTablePlotWidget* _rmsdPlotWidget;

    /// Marks the RMSD cutoff in the histogram plot.
    QwtPlotZoneItem* _rmsdRangeIndicator;
};

}   // End of namespace
