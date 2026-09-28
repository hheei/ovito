// SPDX-FileCopyrightText: 2026 OVITO GmbH, Germany
// SPDX-License-Identifier: GPL-3.0-only OR MIT

#include <ovito/particles/gui/ParticlesGui.h>
#include <ovito/particles/export/lammps/LAMMPSDumpExporter.h>
#include <ovito/gui/desktop/properties/IntegerParameterUI.h>
#include <ovito/gui/desktop/properties/BooleanParameterUI.h>
#include "LAMMPSDumpExporterEditor.h"

namespace Ovito {

IMPLEMENT_CREATABLE_OVITO_CLASS(LAMMPSDumpExporterEditor);
SET_OVITO_OBJECT_EDITOR(LAMMPSDumpExporter, LAMMPSDumpExporterEditor);

/******************************************************************************
* Sets up the UI widgets of the editor.
******************************************************************************/
void LAMMPSDumpExporterEditor::createUI(const RolloutInsertionParameters& rolloutParams)
{
    // Create a rollout.
    QWidget* rollout = createRollout(tr("LAMMPS Dump File"), rolloutParams);

    // Create the rollout contents.
    QGridLayout* layout = new QGridLayout(rollout);
    layout->setContentsMargins(4,4,4,4);
    layout->setSpacing(4);
    layout->setColumnStretch(2,1);

    IntegerParameterUI* precisionUI = createParamUI<IntegerParameterUI>(PROPERTY_FIELD(FileExporter::floatOutputPrecision));
    layout->addWidget(precisionUI->label(), 0, 0);
    layout->addLayout(precisionUI->createFieldLayout(), 0, 1);

    BooleanParameterUI* restrictedTriclinicUI = createParamUI<BooleanParameterUI>(PROPERTY_FIELD(LAMMPSDumpExporter::restrictedTriclinic));
    layout->addWidget(restrictedTriclinicUI->checkBox(), 1, 0);

    FileColumnParticleExporterEditor::createUI(rolloutParams.before(rollout));
}

}   // End of namespace
