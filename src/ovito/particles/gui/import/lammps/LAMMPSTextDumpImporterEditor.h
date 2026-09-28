// SPDX-FileCopyrightText: 2026 OVITO GmbH, Germany
// SPDX-License-Identifier: GPL-3.0-only OR MIT

#pragma once


#include <ovito/particles/gui/ParticlesGui.h>
#include <ovito/particles/import/lammps/LAMMPSTextDumpImporter.h>
#include <ovito/gui/desktop/dataset/io/FileImporterEditor.h>

namespace Ovito {

/**
 * \brief A properties editor for the LAMMPSTextDumpImporter class.
 */
class LAMMPSTextDumpImporterEditor : public FileImporterEditor
{
    OVITO_CLASS(LAMMPSTextDumpImporterEditor)
    Q_OBJECT

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
