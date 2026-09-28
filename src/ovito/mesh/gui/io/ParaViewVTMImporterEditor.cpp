// SPDX-FileCopyrightText: 2026 OVITO GmbH, Germany
// SPDX-License-Identifier: GPL-3.0-only OR MIT

#include <ovito/gui/desktop/GUI.h>
#include <ovito/mesh/io/ParaViewVTMImporter.h>
#include <ovito/gui/desktop/properties/BooleanParameterUI.h>
#include "ParaViewVTMImporterEditor.h"

namespace Ovito {

IMPLEMENT_CREATABLE_OVITO_CLASS(ParaViewVTMImporterEditor);
SET_OVITO_OBJECT_EDITOR(ParaViewVTMImporter, ParaViewVTMImporterEditor);

/******************************************************************************
* Sets up the UI widgets of the editor.
******************************************************************************/
void ParaViewVTMImporterEditor::createUI(const RolloutInsertionParameters& rolloutParams)
{
    // Create a rollout.
    QWidget* rollout = createRollout(tr("VTM file reader"), rolloutParams);

    // Create the rollout contents.
    QVBoxLayout* layout = new QVBoxLayout(rollout);
    layout->setContentsMargins(4,4,4,4);
    layout->setSpacing(4);

    // Create group box.
    QGroupBox* groupBox = new QGroupBox(tr("Import options"));
    layout->addWidget(groupBox);
    QVBoxLayout* sublayout = new QVBoxLayout(groupBox);
    sublayout->setSpacing(4);

    // Unite meshes.
    BooleanParameterUI* uniteMeshesUI = createParamUI<BooleanParameterUI>(PROPERTY_FIELD(ParaViewVTMImporter::uniteMeshes));
    sublayout->addWidget(uniteMeshesUI->checkBox());
}

}   // End of namespace
