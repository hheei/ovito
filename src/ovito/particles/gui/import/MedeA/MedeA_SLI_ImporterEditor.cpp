// SPDX-FileCopyrightText: 2026 OVITO GmbH, Germany
// SPDX-License-Identifier: GPL-3.0-only OR MIT

#include <ovito/particles/gui/ParticlesGui.h>
#include <ovito/particles/import/MedeA/MedeA_SLI_Importer.h>
#include <ovito/gui/desktop/properties/BooleanParameterUI.h>
#include "MedeA_SLI_ImporterEditor.h"

namespace Ovito {

IMPLEMENT_CREATABLE_OVITO_CLASS(MedeA_SLI_ImporterEditor);
SET_OVITO_OBJECT_EDITOR(MedeA_SLI_Importer, MedeA_SLI_ImporterEditor);

/******************************************************************************
* Sets up the UI widgets of the editor.
******************************************************************************/
void MedeA_SLI_ImporterEditor::createUI(const RolloutInsertionParameters& rolloutParams)
{
    // Create a rollout.
    QWidget* rollout = createRollout(tr("MedeA SLI reader"), rolloutParams, "manual:file_formats.input.medea_sli");

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
