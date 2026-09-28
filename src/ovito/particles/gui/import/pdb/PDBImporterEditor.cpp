// SPDX-FileCopyrightText: 2026 OVITO GmbH, Germany
// SPDX-License-Identifier: GPL-3.0-only OR MIT

#include <ovito/particles/gui/ParticlesGui.h>
#include <ovito/particles/import/pdb/PDBImporter.h>
#include <ovito/gui/desktop/properties/BooleanParameterUI.h>
#include <ovito/gui/desktop/properties/BooleanRadioButtonParameterUI.h>
#include "PDBImporterEditor.h"

namespace Ovito {

IMPLEMENT_CREATABLE_OVITO_CLASS(PDBImporterEditor);
SET_OVITO_OBJECT_EDITOR(PDBImporter, PDBImporterEditor);

/******************************************************************************
* Sets up the UI widgets of the editor.
******************************************************************************/
void PDBImporterEditor::createUI(const RolloutInsertionParameters& rolloutParams)
{
    // Create a rollout.
    QWidget* rollout = createRollout(tr("PDB reader"), rolloutParams, "manual:file_formats.input.pdb");

    // Create the rollout contents.
    QVBoxLayout* layout = new QVBoxLayout(rollout);
    layout->setContentsMargins(4,4,4,4);
    layout->setSpacing(4);

    QGroupBox* optionsBox = new QGroupBox(tr("Options"), rollout);
    QVBoxLayout* sublayout = new QVBoxLayout(optionsBox);
    sublayout->setContentsMargins(4,4,4,4);
    layout->addWidget(optionsBox);

    // Generate bounding box option.
    BooleanParameterUI* generateBoundingBoxUI = createParamUI<BooleanParameterUI>(PROPERTY_FIELD(ParticleImporter::generateBoundingBox));
    sublayout->addWidget(generateBoundingBoxUI->checkBox());

    // Center simulation cell.
    BooleanParameterUI* recenterCellUI = createParamUI<BooleanParameterUI>(PROPERTY_FIELD(ParticleImporter::recenterCell));
    sublayout->addWidget(recenterCellUI->checkBox());

    // Sort particles.
    BooleanParameterUI* sortParticlesUI = createParamUI<BooleanParameterUI>(PROPERTY_FIELD(ParticleImporter::sortParticles));
    sublayout->addWidget(sortParticlesUI->checkBox());

    // Generate bonds
    sublayout->addSpacing(8);
    BooleanRadioButtonParameterUI* generateBondsUI = createParamUI<BooleanRadioButtonParameterUI>(PROPERTY_FIELD(ParticleImporter::generateBonds));
    generateBondsUI->buttonTrue()->setText(tr("Generate distance-based bonds"));
    sublayout->addWidget(generateBondsUI->buttonTrue());
    generateBondsUI->buttonFalse()->setText(tr("Load bonds from file"));
    sublayout->addWidget(generateBondsUI->buttonFalse());
}

}   // End of namespace
