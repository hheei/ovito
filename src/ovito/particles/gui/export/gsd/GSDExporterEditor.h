// SPDX-FileCopyrightText: 2026 OVITO GmbH, Germany
// SPDX-License-Identifier: GPL-3.0-only OR MIT

#pragma once

#include <ovito/particles/gui/ParticlesGui.h>
#include <ovito/gui/desktop/properties/PropertiesEditor.h>

namespace Ovito {

/**
 * \brief User interface component for the GSDExporter class.
 */
class GSDExporterEditor : public PropertiesEditor
{
    OVITO_CLASS(GSDExporterEditor)

protected:
    /// Creates the user interface controls for the editor.
    virtual void createUI(const RolloutInsertionParameters& rolloutParams) override;
};

}  // namespace Ovito
