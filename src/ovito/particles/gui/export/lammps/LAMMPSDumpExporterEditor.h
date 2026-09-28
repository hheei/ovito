// SPDX-FileCopyrightText: 2026 OVITO GmbH, Germany
// SPDX-License-Identifier: GPL-3.0-only OR MIT

#pragma once


#include <ovito/particles/gui/ParticlesGui.h>
#include <ovito/particles/gui/export/FileColumnParticleExporterEditor.h>

namespace Ovito {

/**
 * \brief User interface component for the LAMMPSDumpExporter class.
 */
class LAMMPSDumpExporterEditor : public FileColumnParticleExporterEditor
{
    OVITO_CLASS(LAMMPSDumpExporterEditor)
    Q_OBJECT

protected:

    /// Creates the user interface controls for the editor.
    virtual void createUI(const RolloutInsertionParameters& rolloutParams) override;
};

}   // End of namespace
