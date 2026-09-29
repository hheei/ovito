// SPDX-FileCopyrightText: 2026 OVITO GmbH, Germany
// SPDX-License-Identifier: GPL-3.0-only OR MIT

#include <ovito/gui/desktop/GUI.h>
#include <ovito/gui/base/app/GuiSettings.h>
#include "HistoryFileDialog.h"

namespace Ovito {

/******************************************************************************
* Constructs the dialog window.
******************************************************************************/
HistoryFileDialog::HistoryFileDialog(MainWindowUI& ui, const QString& dialogClass, QWidget* parent, const QString& caption, const QString& directory, const QString& filter)
    : QFileDialog(parent, caption, directory.isEmpty() ? QDir::currentPath() : directory, filter)
    , UserInterfaceComponent<MainWindowUI>(ui)
    , _dialogClass(dialogClass)
{
    connect(this, &QFileDialog::fileSelected, this, &HistoryFileDialog::onFileSelected);
    connect(this, &QFileDialog::filesSelected, this, [&](const QStringList& selected) {
        if(!selected.empty()) onFileSelected(selected.front());
    });

    // The user can request Qt's widget-based file dialog instead of the native OS dialog by settings the corresponding option in the application settings dialog.
    // The native dialog (our default choice) is typically faster than the Qt widget implementation.
    // On the other hand, on certain platforms (e.g. Linux with older GNOME desktops) the native dialog may ignore the current working directory settings of the application and always start in the same default directory, which can be very inconvenient.
    // In this case, the user can switch to the Qt dialog, which respects the application's working directory settings.
    if(GuiSettings::instance().preferQtFileDialog())
        setOption(QFileDialog::DontUseNativeDialog);

    if(GuiSettings::instance().keepDirectoryHistory()) {
        QStringList history = GuiSettings::instance().recentDirectories(_dialogClass);
        if(history.isEmpty() == false) {
            if(directory.isEmpty()) {
                setDirectory(history.front());
            }
            setHistory(history);
        }
    }
}

/******************************************************************************
* This is called when the user has pressed the OK button of the dialog.
******************************************************************************/
void HistoryFileDialog::onFileSelected(const QString& file)
{
    if(file.isEmpty())
        return;

    if(GuiSettings::instance().keepDirectoryHistory()) {
        QString currentDir = QFileInfo(file).absolutePath();
        GuiSettings::instance().rememberDirectory(_dialogClass, currentDir);
    }
}

}   // End of namespace
