// SPDX-FileCopyrightText: 2026 OVITO GmbH, Germany
// SPDX-License-Identifier: GPL-3.0-only OR MIT

#pragma once


#include <ovito/particles/gui/ParticlesGui.h>
#include <ovito/particles/import/lammps/LAMMPSBinaryDumpImporter.h>
#include <ovito/gui/desktop/dataset/io/FileImporterEditor.h>

namespace Ovito {

/**
 * \brief A properties editor for the LAMMPSBinaryDumpImporter class.
 */
class LAMMPSBinaryDumpImporterEditor : public FileImporterEditor
{
    OVITO_CLASS(LAMMPSBinaryDumpImporterEditor)
    Q_OBJECT

public:

    /// This is called by the system when the user has selected a new file to import.
    virtual void inspectNewFile(FileImporter* importer, const QUrl& sourceFile) override;

protected:

    /// Creates the user interface controls for the editor.
    virtual void createUI(const RolloutInsertionParameters& rolloutParams) override;

    /// This method is called when a reference target changes.
    virtual bool referenceEvent(RefTarget* source, const ReferenceEvent& event) override;

protected Q_SLOTS:

    /// Is called when the user pressed the "Edit column mapping" button.
    void onEditColumnMapping();

private:

    BooleanParameterUI* _multitimestepUI;
};

}   // End of namespace
