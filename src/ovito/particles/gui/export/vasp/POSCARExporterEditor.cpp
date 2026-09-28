// SPDX-FileCopyrightText: 2026 OVITO GmbH, Germany
// SPDX-License-Identifier: GPL-3.0-only OR MIT

#include <ovito/particles/gui/ParticlesGui.h>
#include <ovito/particles/export/vasp/POSCARExporter.h>
#include <ovito/gui/desktop/properties/BooleanParameterUI.h>
#include "POSCARExporterEditor.h"

namespace Ovito {

IMPLEMENT_CREATABLE_OVITO_CLASS(POSCARExporterEditor);
SET_OVITO_OBJECT_EDITOR(POSCARExporter, POSCARExporterEditor);

/******************************************************************************
* Sets up the UI widgets of the editor.
******************************************************************************/
void POSCARExporterEditor::createUI(const RolloutInsertionParameters& rolloutParams)
{
    // Create a rollout.
    QWidget* rollout = createRollout(tr("POSCAR format"), rolloutParams);

    // Create the rollout contents.
    QHBoxLayout* layout = new QHBoxLayout(rollout);
    layout->setContentsMargins(6,6,6,6);
    layout->setSpacing(4);

    BooleanParameterUI* reducedCoordsUI = createParamUI<BooleanParameterUI>(PROPERTY_FIELD(POSCARExporter::writeReducedCoordinates));
    layout->addWidget(reducedCoordsUI->checkBox());
}

}   // End of namespace
