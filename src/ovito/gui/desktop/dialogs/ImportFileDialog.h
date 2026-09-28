// SPDX-FileCopyrightText: 2026 OVITO GmbH, Germany
// SPDX-License-Identifier: GPL-3.0-only OR MIT

#pragma once


#include <ovito/gui/desktop/GUI.h>
#include <ovito/core/dataset/io/FileImporter.h>
#include "HistoryFileDialog.h"

namespace Ovito {

/**
 * This file chooser dialog lets the user select a file to be imported.
 */
class OVITO_GUI_EXPORT ImportFileDialog : public HistoryFileDialog
{
    Q_OBJECT

public:

    /// Returns what should happen if the user imports several files of the same kind.
    static FileImporter::MultiFileImportMode multiFileImportMode() {
#ifdef OVITO_BUILD_PROFESSIONAL
        return QSettings().value("file/multi_file_import_mode", FileImporter::ImportAsTrajectory).value<FileImporter::MultiFileImportMode>();
#else
        return FileImporter::ImportAsTrajectory;
#endif
    }

    /// Sets what should happen if the user imports several files of the same kind.
    static void setMultiFileImportMode(FileImporter::MultiFileImportMode mode) {
        QSettings settings;
        if(mode != FileImporter::ImportAsTrajectory) settings.setValue("file/multi_file_import_mode", mode);
        else settings.remove("file/multi_file_import_mode");
    }

public:

    /// Constructor.
    ImportFileDialog(MainWindowUI& ui, const std::vector<const FileImporterClass*>& importerTypes, QWidget* parent, const QString& caption, bool allowMultiSelection, const QString& dialogClass = QStringLiteral("import"));

    /// Returns the file to import after the dialog has been closed with "OK".
    QString fileToImport() const;

    /// Returns the file to import after the dialog has been closed with "OK".
    QUrl urlToImport() const;

    /// Returns the list of files to import after the dialog has been closed with "OK".
    std::vector<QUrl> urlsToImport() const;

    /// Returns the selected importer class and sub-format name.
    const std::pair<const FileImporterClass*, QString>& selectedFileImporter() const;

private:

    std::vector<std::pair<const FileImporterClass*, QString>> _importerFormats;
};

}   // End of namespace
