// SPDX-FileCopyrightText: 2026 OVITO GmbH, Germany
// SPDX-License-Identifier: GPL-3.0-only OR MIT

#include <ovito/particles/gui/ParticlesGui.h>
#include <ovito/particles/import/sdf/SDFImporter.h>
#include <ovito/gui/desktop/properties/BooleanParameterUI.h>
#include "SDFImporterEditor.h"

namespace Ovito {

IMPLEMENT_CREATABLE_OVITO_CLASS(SDFImporterEditor);
SET_OVITO_OBJECT_EDITOR(SDFImporter, SDFImporterEditor);

/******************************************************************************
 * Sets up the UI widgets of the editor.
 ******************************************************************************/
void SDFImporterEditor::createUI(const RolloutInsertionParameters& rolloutParams)
{
    // Create a rollout.
    QWidget* rollout = createRollout(tr("MOL/SDF reader"), rolloutParams);

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
}

}  // namespace Ovito
