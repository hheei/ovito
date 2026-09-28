// SPDX-FileCopyrightText: 2026 OVITO GmbH, Germany
// SPDX-License-Identifier: GPL-3.0-only OR MIT

#include <ovito/particles/gui/ParticlesGui.h>
#include <ovito/particles/import/mol2/MOL2Importer.h>
#include <ovito/gui/desktop/properties/BooleanParameterUI.h>
#include "MOL2ImporterEditor.h"

namespace Ovito {

IMPLEMENT_CREATABLE_OVITO_CLASS(MOL2ImporterEditor);
SET_OVITO_OBJECT_EDITOR(MOL2Importer, MOL2ImporterEditor);

/******************************************************************************
* Sets up the UI widgets of the editor.
******************************************************************************/
void MOL2ImporterEditor::createUI(const RolloutInsertionParameters& rolloutParams)
{
    // Create a rollout.
    QWidget* rollout = createRollout(tr("MOL2 reader"), rolloutParams, "manual:file_formats.input.mol2");

    // Create the rollout contents.
    QVBoxLayout* layout = new QVBoxLayout(rollout);
    layout->setContentsMargins(4, 4, 4, 4);
    layout->setSpacing(4);

    QGroupBox* optionsBox = new QGroupBox(tr("Options"), rollout);
    QVBoxLayout* sublayout = new QVBoxLayout(optionsBox);
    sublayout->setContentsMargins(4, 4, 4, 4);
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
}

}  // namespace Ovito
