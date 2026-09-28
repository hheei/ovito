// SPDX-FileCopyrightText: 2026 OVITO GmbH, Germany
// SPDX-License-Identifier: GPL-3.0-only OR MIT

#pragma once


#include <ovito/particles/gui/ParticlesGui.h>
#include <ovito/stdobj/gui/widgets/DataTablePlotWidget.h>
#include <ovito/gui/desktop/properties/PropertiesEditor.h>

class QwtPlot;
class QwtPlotCurve;

namespace Ovito {

/**
 * A properties editor for the SpatialCorrelationFunctionModifier class.
 */
class SpatialCorrelationFunctionModifierEditor : public PropertiesEditor
{
    OVITO_CLASS(SpatialCorrelationFunctionModifierEditor)
    Q_OBJECT

protected:

    /// Creates the user interface controls for the editor.
    virtual void createUI(const RolloutInsertionParameters& rolloutParams) override;

    /// Replots one of the correlation function computed by the modifier.
    std::pair<FloatType,FloatType> plotData(const DataTable* table, DataTablePlotWidget* plotWidget, FloatType offset, FloatType fac, BufferReadAccess<FloatType> normalization);

protected Q_SLOTS:

    /// Replots the correlation function computed by the modifier.
    void plotAllData();

private:

    /// The plotting widget for displaying the computed real-space correlation function.
    DataTablePlotWidget* _realSpacePlot;

    /// The plotting widget for displaying the computed reciprocal-space correlation function.
    DataTablePlotWidget* _reciprocalSpacePlot;

    /// The plot item for the short-ranged part of the real-space correlation function.
    QwtPlotCurve* _neighCurve = nullptr;
};

}   // End of namespace
