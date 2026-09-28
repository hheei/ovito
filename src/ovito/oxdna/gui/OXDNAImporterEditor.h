// SPDX-FileCopyrightText: 2026 OVITO GmbH, Germany
// SPDX-License-Identifier: GPL-3.0-only OR MIT

#pragma once


#include <ovito/particles/gui/ParticlesGui.h>
#include <ovito/oxdna/OXDNAImporter.h>
#include <ovito/gui/desktop/dataset/io/FileImporterEditor.h>

namespace Ovito {

/**
 * \brief A properties editor for the OXDNAImporter class.
 */
class OXDNAImporterEditor : public FileImporterEditor
{
    OVITO_CLASS(OXDNAImporterEditor)
    Q_OBJECT

protected:

    /// Creates the user interface controls for the editor.
    virtual void createUI(const RolloutInsertionParameters& rolloutParams) override;

private Q_SLOTS:

    /// Is called by the system when the importer changes.
    void importerChanged(RefTarget* editObject);

    /// Lets the user choose a oxDNA topology file.
    void onChooseTopologyFile();

private:

    QLineEdit* _topologyFileField;
    QPushButton* _pickTopologyFileBtn;
};

}   // End of namespace
