// SPDX-FileCopyrightText: 2026 OVITO GmbH, Germany
// SPDX-License-Identifier: GPL-3.0-only OR MIT

#include <ovito/particles/gui/ParticlesGui.h>
#include <ovito/particles/export/xyz/XYZExporter.h>
#include <ovito/gui/desktop/properties/VariantComboBoxParameterUI.h>
#include <ovito/gui/desktop/properties/IntegerParameterUI.h>
#include "XYZExporterEditor.h"

namespace Ovito {

IMPLEMENT_CREATABLE_OVITO_CLASS(XYZExporterEditor);
SET_OVITO_OBJECT_EDITOR(XYZExporter, XYZExporterEditor);

/******************************************************************************
* Sets up the UI widgets of the editor.
******************************************************************************/
void XYZExporterEditor::createUI(const RolloutInsertionParameters& rolloutParams)
{
    // Create a rollout.
    QWidget* rollout = createRollout(tr("XYZ File"), rolloutParams);

    // Create the rollout contents.
    QGridLayout* layout = new QGridLayout(rollout);
    layout->setContentsMargins(4,4,4,4);
    layout->setSpacing(4);
    layout->setColumnStretch(1,1);
    layout->setColumnStretch(4,1);
    layout->setColumnMinimumWidth(2,10);
    layout->addWidget(new QLabel(tr("XYZ format style:")), 0, 0);

    VariantComboBoxParameterUI* subFormatUI = createParamUI<VariantComboBoxParameterUI>(PROPERTY_FIELD(XYZExporter::subFormat));
    subFormatUI->comboBox()->addItem("Extended (default)", QVariant::fromValue((int)XYZExporter::ExtendedFormat));
    subFormatUI->comboBox()->addItem("Parcas", QVariant::fromValue((int)XYZExporter::ParcasFormat));
    layout->addWidget(subFormatUI->comboBox(), 0, 1);

    IntegerParameterUI* precisionUI = createParamUI<IntegerParameterUI>(PROPERTY_FIELD(FileExporter::floatOutputPrecision));
    layout->addWidget(precisionUI->label(), 0, 3);
    layout->addLayout(precisionUI->createFieldLayout(), 0, 4);

    FileColumnParticleExporterEditor::createUI(rolloutParams.before(rollout));
}

}   // End of namespace
