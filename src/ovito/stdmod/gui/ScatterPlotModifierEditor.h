// SPDX-FileCopyrightText: 2026 OVITO GmbH, Germany
// SPDX-License-Identifier: GPL-3.0-only OR MIT

#pragma once


#include <ovito/stdmod/gui/StdModGui.h>
#include <ovito/gui/desktop/properties/PropertiesEditor.h>
#include <ovito/stdobj/gui/widgets/DataTablePlotWidget.h>

class QwtPlotZoneItem;

namespace Ovito {

/**
 * A properties editor for the ScatterPlotModifier class.
 */
class ScatterPlotModifierEditor : public PropertiesEditor
{
    OVITO_CLASS(ScatterPlotModifierEditor)
    Q_OBJECT

protected:

    /// Creates the user interface controls for the editor.
    virtual void createUI(const RolloutInsertionParameters& rolloutParams) override;

protected Q_SLOTS:

    /// Replots the scatter plot.
    void plotScatterPlot();

private:

    /// The graph widget to display the plot.
    DataTablePlotWidget* _plotWidget;

    /// Marks the range of selected points in the X direction.
    QwtPlotZoneItem* _selectionRangeIndicatorX;

    /// Marks the range of selected points in the Y direction.
    QwtPlotZoneItem* _selectionRangeIndicatorY;
};

}   // End of namespace
