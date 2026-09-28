// SPDX-FileCopyrightText: 2026 OVITO GmbH, Germany
// SPDX-License-Identifier: GPL-3.0-only OR MIT

#include <ovito/particles/gui/ParticlesGui.h>
#include <ovito/particles/import/gsd/GSDImporter.h>
#include <ovito/gui/desktop/properties/IntegerParameterUI.h>
#include "GSDImporterEditor.h"

namespace Ovito {

IMPLEMENT_CREATABLE_OVITO_CLASS(GSDImporterEditor);
SET_OVITO_OBJECT_EDITOR(GSDImporter, GSDImporterEditor);

/******************************************************************************
* Sets up the UI widgets of the editor.
******************************************************************************/
void GSDImporterEditor::createUI(const RolloutInsertionParameters& rolloutParams)
{
	// Create a rollout.
	QWidget* rollout = createRollout(tr("GSD reader"), rolloutParams, "manual:file_formats.input.gsd");

    // Create the rollout contents.
	QVBoxLayout* layout = new QVBoxLayout(rollout);
	layout->setContentsMargins(4,4,4,4);
	layout->setSpacing(4);

	QGroupBox* optionsBox = new QGroupBox(tr("Options"), rollout);
	QGridLayout* sublayout = new QGridLayout(optionsBox);
	sublayout->setContentsMargins(4,4,4,4);
	sublayout->setSpacing(6);
	sublayout->setColumnStretch(1, 1);
	layout->addWidget(optionsBox);

	// Rounding resolution
	IntegerParameterUI* resolutionUI = createParamUI<IntegerParameterUI>(PROPERTY_FIELD(GSDImporter::roundingResolution));
	sublayout->addWidget(resolutionUI->label(), 1, 0);
	sublayout->addLayout(resolutionUI->createFieldLayout(), 1, 1);
}

}	// End of namespace
