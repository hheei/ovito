// SPDX-FileCopyrightText: 2026 OVITO GmbH, Germany
// SPDX-License-Identifier: GPL-3.0-only OR MIT

#pragma once


#include <ovito/gui/desktop/GUI.h>
#include <ovito/gui/desktop/properties/PropertiesEditor.h>
#include <ovito/stdobj/gui/widgets/DataTablePlotWidget.h>

class QwtPlotMarker;

namespace Ovito {

/**
 * \brief A properties editor for the CreateIsosurfaceModifier class.
 */
class CreateIsosurfaceModifierEditor : public PropertiesEditor
{
    OVITO_CLASS(CreateIsosurfaceModifierEditor)
    Q_OBJECT

protected:

    /// Creates the user interface controls for the editor.
    virtual void createUI(const RolloutInsertionParameters& rolloutParams) override;

protected Q_SLOTS:

    /// Replots the value histogram computed by the modifier.
    void plotHistogram();

    /// Is called when the user starts or stops picking a location in the plot widget.
    void onPickerActivated(bool on);

    /// Is called when the user picks a location in the plot widget.
    void onPickerPoint(const QPointF& pt);

private:

    /// The graph widget to display the histogram.
    DataTablePlotWidget* _plotWidget;

    /// The plot item for indicating the current iso level value.
    QwtPlotMarker* _isoLevelIndicator;

    /// Used to make changes to the iso level reversible.
    UndoableTransaction _undoTransaction;
};

}   // End of namespace
