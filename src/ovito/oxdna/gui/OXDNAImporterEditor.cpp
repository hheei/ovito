// SPDX-FileCopyrightText: 2026 OVITO GmbH, Germany
// SPDX-License-Identifier: GPL-3.0-only OR MIT

#include <ovito/particles/gui/ParticlesGui.h>
#include <ovito/gui/desktop/dialogs/HistoryFileDialog.h>
#include "OXDNAImporterEditor.h"

namespace Ovito {

IMPLEMENT_CREATABLE_OVITO_CLASS(OXDNAImporterEditor);
SET_OVITO_OBJECT_EDITOR(OXDNAImporter, OXDNAImporterEditor);

/******************************************************************************
* Sets up the UI widgets of the editor.
******************************************************************************/
void OXDNAImporterEditor::createUI(const RolloutInsertionParameters& rolloutParams)
{
    // Create a rollout.
    QWidget* rollout = createRollout(tr("oxDNA"), rolloutParams);

    // Create the rollout contents.
    QVBoxLayout* layout = new QVBoxLayout(rollout);
    layout->setContentsMargins(4,4,4,4);
    layout->setSpacing(4);

    QGroupBox* topologyBox = new QGroupBox(tr("Topology file"), rollout);
    layout->addWidget(topologyBox);
    QGridLayout* gridlayout = new QGridLayout(topologyBox);
    gridlayout->setContentsMargins(4,4,4,4);
    gridlayout->setColumnStretch(1,1);
    gridlayout->setVerticalSpacing(2);
    gridlayout->setHorizontalSpacing(6);

    _topologyFileField = new QLineEdit();
    _topologyFileField->setReadOnly(true);
    _topologyFileField->setFrame(false);
    _topologyFileField->setPlaceholderText(tr("Using automatic discovery"));
    gridlayout->addWidget(_topologyFileField, 0, 0, 1, 2);

    _pickTopologyFileBtn = new QPushButton(tr("Pick..."));
    _pickTopologyFileBtn->setEnabled(false);
    connect(_pickTopologyFileBtn, &QPushButton::clicked, this, &OXDNAImporterEditor::onChooseTopologyFile);
    gridlayout->addWidget(_pickTopologyFileBtn, 1, 0);

    connect(this, &PropertiesEditor::contentsChanged, this, &OXDNAImporterEditor::importerChanged);
}

/******************************************************************************
* Is called by the system when the importer changes.
******************************************************************************/
void OXDNAImporterEditor::importerChanged(RefTarget* editObject)
{
    if(OXDNAImporter* importer = static_object_cast<OXDNAImporter>(editObject)) {
        _pickTopologyFileBtn->setEnabled(true);
        if(importer->topologyFileUrl().isValid())
            _topologyFileField->setText(importer->topologyFileUrl().toString(QUrl::RemovePassword | QUrl::PreferLocalFile | QUrl::PrettyDecoded));
        else
            _topologyFileField->clear();
    }
    else {
        _pickTopologyFileBtn->setEnabled(false);
        _topologyFileField->clear();
    }
}

/******************************************************************************
* Lets the user choose a oxDNA topology file.
******************************************************************************/
void OXDNAImporterEditor::onChooseTopologyFile()
{
    OXDNAImporter* importer = static_object_cast<OXDNAImporter>(editObject());
    if(!importer) return;

    HistoryFileDialog fileDialog(ui(), QStringLiteral("import"), container(), tr("Pick oxDNA topology file"));
    fileDialog.setAcceptMode(QFileDialog::AcceptOpen);
    fileDialog.setFileMode(QFileDialog::ExistingFile);

    if(importer->topologyFileUrl().isValid() && importer->topologyFileUrl().isLocalFile()) {
#ifndef Q_OS_LINUX
        fileDialog.selectFile(importer->topologyFileUrl().toLocalFile());
#else
        // Workaround for bug in QFileDialog on Linux (Qt 6.2.4) crashing in exec() when selectFile() is called before (OVITO issue #216).
        fileDialog.setDirectory(QFileInfo(importer->topologyFileUrl().toLocalFile()).dir());
#endif
    }

    if(fileDialog.exec() == QDialog::Accepted) {
        performTransaction(tr("Set topology file"), [importer, &fileDialog]() {
            const QStringList& selectedFiles = fileDialog.selectedFiles();
            if(!selectedFiles.empty()) {
                importer->setTopologyFileUrl(QUrl::fromLocalFile(selectedFiles.front()));
                importer->requestReload();
            }
        });
    }
}

}   // End of namespace
