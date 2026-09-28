// SPDX-FileCopyrightText: 2026 OVITO GmbH, Germany
// SPDX-License-Identifier: GPL-3.0-only OR MIT

#include <ovito/gui/desktop/GUI.h>
#include <ovito/gui/desktop/mainwin/MainWindow.h>
#include <ovito/gui/desktop/app/GuiApplication.h>
#include <ovito/gui/desktop/dialogs/HistoryFileDialog.h>
#include <ovito/gui/desktop/dialogs/ImportFileDialog.h>
#include <ovito/core/app/Application.h>
#include <ovito/core/dataset/io/FileImporter.h>
#include "GeneralSettingsPage.h"

namespace Ovito {

IMPLEMENT_CREATABLE_OVITO_CLASS(GeneralSettingsPage);

/******************************************************************************
* Creates the widget that contains the plugin specific setting controls.
******************************************************************************/
void GeneralSettingsPage::insertSettingsDialogPage(QTabWidget* tabWidget)
{
    QWidget* page = new QWidget();
    tabWidget->addTab(page, tr("General"));
    QVBoxLayout* layout1 = new QVBoxLayout(page);

    QSettings settings;

    // Group "User interface options":
    QGroupBox* uiGroupBox = new QGroupBox(tr("User interface options"), page);
    layout1->addWidget(uiGroupBox);
    QGridLayout* layout2 = new QGridLayout(uiGroupBox);

    _enableAutomaticDarkMode = new QCheckBox(tr("Auto-detect color scheme and enable dark mode"));
    _enableAutomaticDarkMode->setToolTip(tr("<p>If enabled, will switch between light and dark UI depending on current system color theme.</p>"));
    layout2->addWidget(_enableAutomaticDarkMode, 0, 0);
    _enableAutomaticDarkMode->setChecked(GuiApplication::automaticallyEnableDarkMode());
#if defined(Q_OS_LINUX)
    _enableAutomaticDarkMode->setEnabled(false);
    _enableAutomaticDarkMode->setText(_enableAutomaticDarkMode->text() + tr(" (always enabled on Linux)"));
#elif defined(Q_OS_MACOS)
    _enableAutomaticDarkMode->setEnabled(false);
    _enableAutomaticDarkMode->setText(_enableAutomaticDarkMode->text() + tr(" (always enabled on macOS)"));
#else
    _enableAutomaticDarkMode->setText(_enableAutomaticDarkMode->text() + tr(" (requires application restart to take effect)"));
#endif

    _keepDirHistory = new QCheckBox(tr("Use separate working directories for data import/export and session states"));
    _keepDirHistory->setToolTip(tr("<p>If enabled, OVITO maintains individual working directories for different kinds of file operations and remembers them across program sessions.</p><p>If disabled, the same current working directory is used for all file operations.</p>"));
    layout2->addWidget(_keepDirHistory, 1, 0);
    _keepDirHistory->setChecked(HistoryFileDialog::keepWorkingDirectoryHistoryEnabled());

    _useNativeFileDialog = new QCheckBox(tr("Use native file selection dialog"));
    _useNativeFileDialog->setToolTip(tr("<p>If disabled, OVITO will use the Qt widget-based file selection dialog instead of the native dialog provided by the operating system, which is the default choice.</p>"));
    layout2->addWidget(_useNativeFileDialog, 2, 0);
    _useNativeFileDialog->setChecked(!HistoryFileDialog::useQtFileDialog());

    // Group "Data import":
    QGroupBox* importGroupBox = new QGroupBox(tr("Data import options"), page);
    layout1->addWidget(importGroupBox);
    layout2 = new QGridLayout(importGroupBox);
    layout2->setColumnStretch(1, 1);

    layout2->addWidget(new QLabel(tr("Import multiple files of the same type:")), 0, 0);
    _importMultipleFilesBehavior = new QButtonGroup(page);
    QRadioButton* asTrajectoryBtn = new QRadioButton(tr("As trajectory (default)"));
    QRadioButton* asSeparateObjectsBtn = new QRadioButton(tr("As separate objects"));
    _importMultipleFilesBehavior->addButton(asTrajectoryBtn, FileImporter::ImportAsTrajectory);
    _importMultipleFilesBehavior->addButton(asSeparateObjectsBtn, FileImporter::ImportAsSeparateObjects);
    _importMultipleFilesBehavior->button(ImportFileDialog::multiFileImportMode())->setChecked(true);
    layout2->addWidget(asTrajectoryBtn, 0, 1);
    layout2->addWidget(asSeparateObjectsBtn, 1, 1);
#ifndef OVITO_BUILD_PROFESSIONAL
    asTrajectoryBtn->setEnabled(false);
    asSeparateObjectsBtn->setEnabled(false);
    asSeparateObjectsBtn->setText(asSeparateObjectsBtn->text() + tr(" (requires OVITO Pro)"));
#endif

    // Group "Program updates":
#if !defined(OVITO_BUILD_APPSTORE_VERSION)
    QGroupBox* updateGroupBox = new QGroupBox(tr("Program updates"), page);
    layout1->addWidget(updateGroupBox);
    layout2 = new QGridLayout(updateGroupBox);

    _enableUpdateChecks = new QCheckBox(tr("Periodically check ovito.org website for program updates (and display notice when available)"), updateGroupBox);
    _enableUpdateChecks->setToolTip(tr(
            "<p>The news page is fetched from <i>www.ovito.org</i> on each program startup. "
            "It displays information about new program releases as soon as they become available.</p>"));
    layout2->addWidget(_enableUpdateChecks, 0, 0);

    _enableUpdateChecks->setChecked(settings.value("updates/check_for_updates", true).toBool());
#endif

    layout1->addStretch();
}

/******************************************************************************
* Lets the page save all changed settings.
******************************************************************************/
void GeneralSettingsPage::saveValues(QTabWidget* tabWidget)
{
    QSettings settings;
    HistoryFileDialog::setKeepWorkingDirectoryHistoryEnabled(_keepDirHistory->isChecked());
    HistoryFileDialog::setUseQtFileDialog(!_useNativeFileDialog->isChecked());
#if !defined(Q_OS_LINUX) && !defined(Q_OS_MACOS)
    if(_enableAutomaticDarkMode->isChecked())
        settings.setValue("ui/automatic_dark_mode", true);
    else
        settings.remove("ui/automatic_dark_mode");
#endif
#ifdef OVITO_BUILD_PROFESSIONAL
    ImportFileDialog::setMultiFileImportMode(static_cast<FileImporter::MultiFileImportMode>(_importMultipleFilesBehavior->checkedId()));
#endif

#if !defined(OVITO_BUILD_APPSTORE_VERSION)
    settings.setValue("updates/check_for_updates", _enableUpdateChecks->isChecked());
#endif
}

}   // End of namespace
