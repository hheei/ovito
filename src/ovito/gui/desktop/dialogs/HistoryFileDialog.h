// SPDX-FileCopyrightText: 2026 OVITO GmbH, Germany
// SPDX-License-Identifier: GPL-3.0-only OR MIT

#pragma once


#include <ovito/gui/desktop/GUI.h>

namespace Ovito {

/**
 * \brief The file chooser dialog that saves a history of recently visited directories.
 */
class OVITO_GUI_EXPORT HistoryFileDialog : public QFileDialog, public UserInterfaceComponent<MainWindowUI>
{
    Q_OBJECT

public:

    /// Constructs the dialog window.
    HistoryFileDialog(MainWindowUI& ui, const QString& dialogClass, QWidget* parent = nullptr, const QString& caption = QString(), const QString& directory = QString(), const QString& filter = QString());

    /// Returns whether the user has activated the program option to maintain separate
    /// working directories for different file I/O operations.
    static bool keepWorkingDirectoryHistoryEnabled() {
        return QSettings().value("file/keep_dir_history", true).toBool();
    }

    /// Sets whether to maintain separate working directories for different file I/O operations.
    static void setKeepWorkingDirectoryHistoryEnabled(bool on) {
        QSettings settings;
        if(!on) settings.setValue("file/keep_dir_history", false);
        else settings.remove("file/keep_dir_history");
    }

    /// Returns whether the user has activated the program option to use the Qt widget-based file dialog instead of the native OS dialog.
    static bool useQtFileDialog() {
        return QSettings().value("file/dialog_type").toString() == "qt";
    }

    /// Sets whether to use the Qt widget-based file dialog instead of the native OS dialog.
    static void setUseQtFileDialog(bool on) {
        QSettings settings;
        if(on) settings.setValue("file/dialog_type", "qt");
        else settings.remove("file/dialog_type");
    }

    virtual int exec() override {
        TaskManager::setNativeDialogActive(true);
        auto dlgResult = QFileDialog::exec();
        TaskManager::setNativeDialogActive(false);
        return dlgResult;
    }

private Q_SLOTS:

    /// This is called when the user has pressed the OK button of the dialog box.
    void onFileSelected(const QString& file);

private:

    /// The type of file dialog: "import", "export" etc.
    QString _dialogClass;
};

}   // End of namespace
