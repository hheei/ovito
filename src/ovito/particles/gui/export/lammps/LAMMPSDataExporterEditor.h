// SPDX-FileCopyrightText: 2026 OVITO GmbH, Germany
// SPDX-License-Identifier: GPL-3.0-only OR MIT

#pragma once


#include <ovito/particles/gui/ParticlesGui.h>
#include <ovito/gui/desktop/properties/PropertiesEditor.h>

namespace Ovito {

/**
 * \brief User interface component for the LAMMPSDataExporter class.
 */
class LAMMPSDataExporterEditor : public PropertiesEditor
{
    OVITO_CLASS(LAMMPSDataExporterEditor)
    Q_OBJECT

protected:

    /// Creates the user interface controls for the editor.
    virtual void createUI(const RolloutInsertionParameters& rolloutParams) override;

private Q_SLOTS:

    /// Updates the displayed values in the UI elements.
    void updateUI();

    /// Is called whenever the user selects a sub-style for atom style hybrid.
    void hybridSubStyleSelected();

private:

    std::array<QComboBox*,3> _subStyleLists;
};

}   // End of namespace
