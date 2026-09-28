// SPDX-FileCopyrightText: 2026 OVITO GmbH, Germany
// SPDX-License-Identifier: GPL-3.0-only OR MIT

#include <ovito/gui/desktop/GUI.h>
#include <ovito/core/utilities/io/FileManager.h>
#include <ovito/core/utilities/SortZipped.h>
#include "ImportFileDialog.h"

namespace Ovito {

/******************************************************************************
* Constructs the dialog window.
******************************************************************************/
ImportFileDialog::ImportFileDialog(MainWindowUI& ui, const std::vector<const FileImporterClass*>& importerTypes, QWidget* parent, const QString& caption, bool allowMultiSelection, const QString& dialogClass) :
    HistoryFileDialog(ui, dialogClass, parent, caption)
{
    if(importerTypes.empty())
        throw Exception(tr("There are no importer plugins installed."));

    // Build list of file filter strings.
    QStringList fileFilterStrings;
    fileFilterStrings.push_back(tr("<Auto-detect file format> (*)"));
    _importerFormats.emplace_back(nullptr, QString());

    for(const auto& importerClass : importerTypes) {
        for(const FileImporterClass::SupportedFormat& format : importerClass->supportedFormats()) {
            OVITO_ASSERT(!format.description.isEmpty() && !format.fileFilter.isEmpty());
            fileFilterStrings << QStringLiteral("%1 (%2)").arg(format.description, format.fileFilter);
            _importerFormats.emplace_back(importerClass, format.identifier);
        }
    }
    // Sort file formats alphabetically (but leave leading <Auto-detect> entry in place).
    Ovito::sort_zipped(
        std::span(fileFilterStrings.data(), fileFilterStrings.size()).subspan(1),
        std::span(_importerFormats.data(), _importerFormats.size()).subspan(1),
        [](const QString& a, const QString& b) { return a.compare(b, Qt::CaseInsensitive) < 0; });

    setNameFilters(fileFilterStrings);
    selectNameFilter(fileFilterStrings.front());
    setAcceptMode(QFileDialog::AcceptOpen);
    setFileMode(allowMultiSelection ? QFileDialog::ExistingFiles : QFileDialog::ExistingFile);
}

/******************************************************************************
* Returns the file to import after the dialog has been closed with "OK".
******************************************************************************/
QString ImportFileDialog::fileToImport() const
{
    QStringList filesToImport = selectedFiles();
    if(filesToImport.isEmpty())
        return {};
    return filesToImport.front();
}

/******************************************************************************
* Returns the file to import after the dialog has been closed with "OK".
******************************************************************************/
QUrl ImportFileDialog::urlToImport() const
{
    return FileManager::urlFromUserInput(fileToImport());
}

/******************************************************************************
* Returns the list of files to import after the dialog has been closed with "OK".
******************************************************************************/
std::vector<QUrl> ImportFileDialog::urlsToImport() const
{
    std::vector<QUrl> list;
    for(const QString& file : selectedFiles()) {
        list.push_back(FileManager::urlFromUserInput(file));
    }
    return list;
}

/******************************************************************************
* Returns the selected importer class and sub-format name.
******************************************************************************/
const std::pair<const FileImporterClass*, QString>& ImportFileDialog::selectedFileImporter() const
{
    int importFilterIndex = nameFilters().indexOf(selectedNameFilter());
    OVITO_ASSERT(importFilterIndex >= 0 && importFilterIndex < _importerFormats.size());
    return _importerFormats[importFilterIndex];
}

}   // End of namespace
