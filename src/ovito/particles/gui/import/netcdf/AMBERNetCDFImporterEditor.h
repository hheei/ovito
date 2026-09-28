// SPDX-FileCopyrightText: 2026 OVITO GmbH, Germany
// SPDX-License-Identifier: GPL-3.0-only OR MIT

#pragma once


#include <ovito/particles/gui/ParticlesGui.h>
#include <ovito/particles/import/netcdf/AMBERNetCDFImporter.h>
#include <ovito/gui/desktop/dataset/io/FileImporterEditor.h>

namespace Ovito {

/**
 * \brief A properties editor for the AMBERNetCDFImporter class.
 */
class AMBERNetCDFImporterEditor : public FileImporterEditor
{
    OVITO_CLASS(AMBERNetCDFImporterEditor)
    Q_OBJECT

protected:

    /// Creates the user interface controls for the editor.
    virtual void createUI(const RolloutInsertionParameters& rolloutParams) override;

protected Q_SLOTS:

    /// Is called when the user pressed the "Edit column mapping" button.
    void onEditColumnMapping();
};

}   // End of namespace
