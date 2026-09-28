// SPDX-FileCopyrightText: 2026 OVITO GmbH, Germany
// SPDX-License-Identifier: GPL-3.0-only OR MIT

#pragma once


#include <ovito/crystalanalysis/CrystalAnalysis.h>
#include <ovito/stdobj/gui/widgets/DataTablePlotWidget.h>
#include <ovito/gui/desktop/properties/PropertiesEditor.h>

class QwtPlotZoneItem;

namespace Ovito {

/**
 * Properties editor for the GrainSegmentationModifier class.
 */
class GrainSegmentationModifierEditor : public PropertiesEditor
{
    OVITO_CLASS(GrainSegmentationModifierEditor)
    Q_OBJECT

protected Q_SLOTS:

    /// Replots the merge sequence computed by the modifier.
    void plotMerges();

protected:

    /// Creates the user interface controls for the editor.
    virtual void createUI(const RolloutInsertionParameters& rolloutParams) override;

private:

    /// The graph widget to display the merge size scatter plot.
    DataTablePlotWidget* _mergePlotWidget;

    /// Marks the merge distance cutoff in the scatter plot.
    QwtPlotZoneItem* _mergeRangeIndicator;

    /// The graph widget to display the log-log scatter plot.
    DataTablePlotWidget* _logPlotWidget;

    /// Marks the merge distance cutoff in the log-log scatter plot.
    QwtPlotZoneItem* _logRangeIndicator;
};

}   // End of namespace
