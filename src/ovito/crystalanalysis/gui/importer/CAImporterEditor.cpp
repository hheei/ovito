// SPDX-FileCopyrightText: 2026 OVITO GmbH, Germany
// SPDX-License-Identifier: GPL-3.0-only OR MIT

#include <ovito/crystalanalysis/CrystalAnalysis.h>
#include <ovito/crystalanalysis/importer/CAImporter.h>
#include <ovito/gui/desktop/properties/BooleanParameterUI.h>
#include "CAImporterEditor.h"

namespace Ovito {

IMPLEMENT_CREATABLE_OVITO_CLASS(CAImporterEditor);
SET_OVITO_OBJECT_EDITOR(CAImporter, CAImporterEditor);

/******************************************************************************
* Sets up the UI widgets of the editor.
******************************************************************************/
void CAImporterEditor::createUI(const RolloutInsertionParameters& rolloutParams)
{
    // Create a rollout.
    QWidget* rollout = createRollout(tr("Crystal analysis file"), rolloutParams);

    // Create the rollout contents.
    QVBoxLayout* layout = new QVBoxLayout(rollout);
    layout->setContentsMargins(4,4,4,4);
    layout->setSpacing(4);

    // Multi-timestep file
    _multitimestepUI = createParamUI<BooleanParameterUI>(PROPERTY_FIELD(FileSourceImporter::isMultiTimestepFile));
    layout->addWidget(_multitimestepUI->checkBox());
}

/******************************************************************************
* This method is called when a reference target changes.
******************************************************************************/
bool CAImporterEditor::referenceEvent(RefTarget* source, const ReferenceEvent& event)
{
    if(source == editObject() && event.type() == FileSourceImporter::MultiTimestepFileChanged) {
        _multitimestepUI->updateUI();
    }
    return PropertiesEditor::referenceEvent(source, event);
}

#ifndef OVITO_BUILD_PROFESSIONAL
/******************************************************************************
* This method is called by the FileSource each time a new source
* file has been selected by the user.
******************************************************************************/
void CAImporterEditor::inspectNewFile(FileImporter* importer, const QUrl& sourceFile)
{
    throw Exception(tr(
        "<p>Crystal Analysis (CA) file import is only available in OVITO Pro.</p>"
        "<p>Please consider upgrading to the <a href=\"https://www.ovito.org/#proFeatures\">professional version</a>, which offers more features for working with precomputed DXA results.</p>"));
}
#endif

}   // End of namespace
