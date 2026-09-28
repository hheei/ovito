// SPDX-FileCopyrightText: 2026 OVITO GmbH, Germany
// SPDX-License-Identifier: GPL-3.0-only OR MIT

#include <ovito/particles/gui/ParticlesGui.h>
#include <ovito/particles/import/vasp/VASPHDF5Importer.h>
#include <ovito/gui/desktop/properties/BooleanParameterUI.h>
#include "VASPHDF5ImporterEditor.h"

namespace Ovito {

IMPLEMENT_CREATABLE_OVITO_CLASS(VASPHDF5ImporterEditor);
SET_OVITO_OBJECT_EDITOR(VASPHDF5Importer, VASPHDF5ImporterEditor);

/******************************************************************************
* Sets up the UI widgets of the editor.
******************************************************************************/
void VASPHDF5ImporterEditor::createUI(const RolloutInsertionParameters& rolloutParams)
{
    // Create a rollout.
    QWidget* rollout = createRollout(tr("VASP HDF5 reader"), rolloutParams, "manual:file_formats.input.vasp_hdf5");

    // Create the rollout contents.
    QVBoxLayout* layout = new QVBoxLayout(rollout);
    layout->setContentsMargins(4,4,4,4);
    layout->setSpacing(4);

    QGroupBox* optionsBox = new QGroupBox(tr("Options"), rollout);
    QVBoxLayout* sublayout = new QVBoxLayout(optionsBox);
    sublayout->setContentsMargins(4,4,4,4);
    layout->addWidget(optionsBox);

    // Center simulation cell.
    BooleanParameterUI* recenterCellUI = createParamUI<BooleanParameterUI>(PROPERTY_FIELD(ParticleImporter::recenterCell));
    sublayout->addWidget(recenterCellUI->checkBox());

    // Generate bonds
    BooleanParameterUI* generateBondsUI = createParamUI<BooleanParameterUI>(PROPERTY_FIELD(ParticleImporter::generateBonds));
    sublayout->addWidget(generateBondsUI->checkBox());
}

}   // End of namespace
