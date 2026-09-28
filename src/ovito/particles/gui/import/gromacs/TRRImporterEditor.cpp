// SPDX-FileCopyrightText: 2026 OVITO GmbH, Germany
// SPDX-License-Identifier: GPL-3.0-only OR MIT

#include <ovito/particles/gui/ParticlesGui.h>
#include <ovito/particles/import/gromacs/TRRImporter.h>
#include <ovito/gui/desktop/properties/BooleanParameterUI.h>
#include "TRRImporterEditor.h"

namespace Ovito {

IMPLEMENT_CREATABLE_OVITO_CLASS(TRRImporterEditor);
SET_OVITO_OBJECT_EDITOR(TRRImporter, TRRImporterEditor);

/******************************************************************************
 * Sets up the UI widgets of the editor.
 ******************************************************************************/
void TRRImporterEditor::createUI(const RolloutInsertionParameters& rolloutParams)
{
    // Create a rollout.
    QWidget* rollout = createRollout(tr("TRR reader"), rolloutParams, "manual:file_formats.input.trr");

    // Create the rollout contents.
    QVBoxLayout* layout = new QVBoxLayout(rollout);
    layout->setContentsMargins(4, 4, 4, 4);
    layout->setSpacing(4);

    QGroupBox* optionsBox = new QGroupBox(tr("Options"), rollout);
    QVBoxLayout* sublayout = new QVBoxLayout(optionsBox);
    sublayout->setContentsMargins(4, 4, 4, 4);
    layout->addWidget(optionsBox);

    // Center simulation cell.
    BooleanParameterUI* recenterCellUI = createParamUI<BooleanParameterUI>(PROPERTY_FIELD(ParticleImporter::recenterCell));
    sublayout->addWidget(recenterCellUI->checkBox());
}

}  // namespace Ovito
