// SPDX-FileCopyrightText: 2026 OVITO GmbH, Germany
// SPDX-License-Identifier: GPL-3.0-only OR MIT

#pragma once

#include <ovito/gui/desktop/GUI.h>
#include <ovito/gui/desktop/mainwin/data_inspector/DataInspectionApplet.h>
#include <ovito/stdobj/simcell/SimulationCell.h>

namespace Ovito {

/**
 * \brief Data inspector page for global attribute values.
 */
class SimulationCellInspectionApplet : public DataInspectionApplet
{
    OVITO_CLASS(SimulationCellInspectionApplet)
    Q_OBJECT

public:

    /// Constructor.
    void initializeObject() { DataInspectionApplet::initializeObject(SimulationCell::OOClass()); }

    /// Returns the key value for this applet that is used for ordering the applet tabs.
    virtual int orderingKey() const override { return 95; }

    /// Determines whether the given pipeline flow state contains data that can be displayed by this applet.
    virtual bool appliesTo(const DataCollection& data) override;

    /// Lets the applet create the UI widget that is to be placed into the data inspector panel.
    virtual QWidget* createWidget() override;

    /// Updates the contents displayed in the inspector.
    virtual void updateDisplay() override;

    /// Returns the help topic ID for the documentation page of this applet.
    virtual QString helpTopicId() const override { return QStringLiteral("manual:data_inspector.simulation_cell"); }

private Q_SLOTS:

    /// Is called to edit the simulation cell.
    void onEditSimulationCell();

private:

    /// Displays the dimensionality of the simulation cell.
    QLabel* _dimensionalityDisplay = nullptr;

    /// Simulation cell pbc flags
    std::array<QLabel*, 3> _checkboxPBC;

    /// Simulation cell cell vectors and origin.
    std::array<std::array<QLineEdit*, 3>, 4> _cellVectorFields;

    /// Simulation cell vectors length and angles (cell parameters).
    std::array<std::array<QLineEdit*, 3>, 2> _cellParamsFields;

    /// Simulation cell bounding box size.
    std::array<QLineEdit*, 3> _bboxFields;

    /// Two widget palettes for indicating PBC flags.
    QPalette _pbcEnabledColor, _pbcDisabledColor;
};

}  // namespace Ovito
