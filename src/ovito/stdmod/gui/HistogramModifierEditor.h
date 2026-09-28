// SPDX-FileCopyrightText: 2026 OVITO GmbH, Germany
// SPDX-License-Identifier: GPL-3.0-only OR MIT

#pragma once


#include <ovito/stdmod/gui/StdModGui.h>
#include <ovito/gui/desktop/properties/PropertiesEditor.h>
#include <ovito/stdobj/gui/widgets/PropertyReferenceParameterUI.h>
#include <ovito/stdobj/gui/widgets/DataTablePlotWidget.h>

class QwtPlotZoneItem;

namespace Ovito {
/**
 * A properties editor for the HistogramModifier class.
 */
class HistogramModifierEditor : public PropertiesEditor
{
    OVITO_CLASS(HistogramModifierEditor)
    Q_OBJECT

protected:

    /// Creates the user interface controls for the editor.
    virtual void createUI(const RolloutInsertionParameters& rolloutParams) override;

protected Q_SLOTS:

    /// Replots the histogram computed by the modifier.
    void plotHistogram();

private:

    /// The graph widget to display the histogram.
    DataTablePlotWidget* _plotWidget;

    /// The plot item for indicating the selection range.
    QwtPlotZoneItem* _selectionRangeIndicator;
};

}   // End of namespace
