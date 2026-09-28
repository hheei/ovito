// SPDX-FileCopyrightText: 2026 OVITO GmbH, Germany
// SPDX-License-Identifier: GPL-3.0-only OR MIT

#pragma once


#include <ovito/gui/desktop/GUI.h>
#include <ovito/core/dataset/io/FileImporter.h>

namespace Ovito {

/**
 * This dialog lets the user select a remote file to be imported.
 */
class OVITO_GUI_EXPORT ImportRemoteFileDialog : public QDialog,  public UserInterfaceComponent<MainWindowUI>
{
    Q_OBJECT

public:

    /// Constructor.
    ImportRemoteFileDialog(MainWindowUI& ui, const std::vector<const FileImporterClass*>& importerTypes, QWidget* parent = nullptr, const QString& caption = QString());

    /// Sets the current URL in the dialog.
    void selectFile(const QUrl& url);

    /// Returns the file to import after the dialog has been closed with "OK".
    QUrl urlToImport() const;

    /// Returns the selected importer class and sub-format name.
    const std::pair<const FileImporterClass*, QString>& selectedFileImporter() const;

    virtual QSize sizeHint() const override {
        return QDialog::sizeHint().expandedTo(QSize(700, 0));
    }

protected Q_SLOTS:

    /// This is called when the user has pressed the OK button of the dialog.
    /// Validates and saves all input made by the user and closes the dialog box.
    void onOk();

    /// This is called when the user presses the help button of the dialog.
    void onHelp();

private:

    std::vector<std::pair<const FileImporterClass*, QString>> _importerFormats;
    QComboBox* _urlEdit;
    QComboBox* _formatSelector;
    QRadioButton* _libsshMethod;
    QRadioButton* _opensshMethod;
    QLineEdit* _sftpPath;
};

}   // End of namespace
