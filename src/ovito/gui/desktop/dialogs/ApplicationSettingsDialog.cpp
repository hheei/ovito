// SPDX-FileCopyrightText: 2026 OVITO GmbH, Germany
// SPDX-License-Identifier: GPL-3.0-only OR MIT

#include <ovito/gui/desktop/GUI.h>
#include <ovito/gui/desktop/mainwin/MainWindow.h>
#include <ovito/gui/base/actions/ActionManager.h>
#include <ovito/core/app/PluginManager.h>
#include "ApplicationSettingsDialog.h"

namespace Ovito {

IMPLEMENT_ABSTRACT_OVITO_CLASS(ApplicationSettingsDialogPage);

/******************************************************************************
* The constructor of the settings dialog class.
******************************************************************************/
ApplicationSettingsDialog::ApplicationSettingsDialog(MainWindowUI& ui, OvitoClassPtr startPage) : QDialog(ui.mainWindow()), UserInterfaceComponent<MainWindowUI>(ui)
{
    setWindowTitle(tr("Application Settings"));

    QVBoxLayout* layout1 = new QVBoxLayout(this);

    // Create dialog contents.
    _tabWidget = new QTabWidget(this);
    layout1->addWidget(_tabWidget);

    // Instantiate all ApplicationSettingsDialogPage derived classes.
    for(OvitoClassPtr clazz : PluginManager::instance().listClasses(ApplicationSettingsDialogPage::OOClass())) {
        try {
            OORef<ApplicationSettingsDialogPage> page = static_object_cast<ApplicationSettingsDialogPage>(clazz->createInstance());
            page->setUserInterface(ui);
            page->_settingsDialog = this;
            _pages.push_back(std::move(page));
        }
        catch(const Exception& ex) {
            ui.reportError(ex);
        }
    }

    // Sort pages.
    std::ranges::sort(_pages, [](const auto& page1, const auto& page2) { return page1->pageSortingKey() < page2->pageSortingKey(); });

    // Show pages in dialog.
    int defaultPage = 0;
    for(const auto& page : _pages) {
        if(startPage && startPage->isMember(page))
            defaultPage = _tabWidget->count();
        handleExceptions([&]() {
            page->insertSettingsDialogPage(_tabWidget);
        });
    }
    if(defaultPage >= 0 && defaultPage < _tabWidget->count())
        _tabWidget->setCurrentIndex(defaultPage);

    // Add a label that displays the location of the application settings store on the computer.
    QLabel* configLocationLabel = new QLabel();
    configLocationLabel->setText(tr("<p style=\"font-size: small; color: #686868;\">Program settings are stored in %1</p>").arg(QSettings().fileName()));
    configLocationLabel->setTextInteractionFlags(Qt::TextSelectableByMouse);
    layout1->addWidget(configLocationLabel);

    // Ok and Cancel buttons
    QDialogButtonBox* buttonBox = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel | QDialogButtonBox::Help, Qt::Horizontal, this);
    connect(buttonBox, &QDialogButtonBox::accepted, this, &ApplicationSettingsDialog::onOk);
    connect(buttonBox, &QDialogButtonBox::rejected, this, &ApplicationSettingsDialog::reject);
    connect(buttonBox, &QDialogButtonBox::helpRequested, this, &ApplicationSettingsDialog::onHelp);
    connect(this, &QDialog::rejected, this, &ApplicationSettingsDialog::onCancel);
    layout1->addWidget(buttonBox);

    // Determine the initial size of the dialog window.
    // Note: We cannot rely on Qt's automatic sizing here, because QTabWidget::sizeHint() truncates the
    // width of the tab bar to 200 pixels whenever the tab bar uses scroll buttons - which is the case
    // with the default widget styles on Windows and Linux (but not on macOS). The dialog window would
    // end up too narrow to display all tabs of the dialog.
    QSize dialogSize = sizeHint();
    dialogSize.setWidth(std::max(dialogSize.width(),
        _tabWidget->tabBar()->sizeHint().width()
        + layout1->contentsMargins().left() + layout1->contentsMargins().right()
        + 2 * style()->pixelMetric(QStyle::PM_DefaultFrameWidth, nullptr, _tabWidget)));
    // But never make the dialog window larger than the available screen space.
    resize(dialogSize.boundedTo(screen()->availableSize()));
}

/******************************************************************************
* This is called when the user has pressed the OK button of the settings dialog.
* Validates and saves all settings made by the user and closes the dialog box.
******************************************************************************/
void ApplicationSettingsDialog::onOk()
{
    setFocus(); // Remove focus from child widgets to commit newly entered values in text widgets etc.

    handleExceptions([&]() {

        // Let all pages validate the changes the user made to the settings.
        for(const auto& page : _pages) {
            if(!page->validateValues(_tabWidget)) {
                return;
            }
        }

        // Let all pages save their settings.
        for(const auto& page : _pages) {
            page->saveValues(_tabWidget);
        }

        // Close dialog box.
        accept();

        // Emit the settingsChanged() signal.
        Application::instance()->emitSettingsChangedSignal();
    });
}

/******************************************************************************
* This is called when the user closes the dialog box using the Cancel button.
******************************************************************************/
void ApplicationSettingsDialog::onCancel()
{
    setFocus(); // Remove focus from child widgets to commit newly entered values in text widgets etc.

    handleExceptions([&]() {
        // Let all pages restore their settings to the old values.
        for(const auto& page : _pages)
            page->restoreValues(_tabWidget);
    });
}

/******************************************************************************
* This is called when the user has pressed the help button of the settings dialog.
******************************************************************************/
void ApplicationSettingsDialog::onHelp()
{
    ApplicationSettingsDialogPage* currentPage = nullptr;
    if(_tabWidget->currentIndex() >= 0 && _tabWidget->currentIndex() < _pages.size())
        currentPage = _pages[_tabWidget->currentIndex()].get();
    if(currentPage) {
        QString helpTopicId = currentPage->helpTopicId();
        if(!helpTopicId.isEmpty()) {
            actionManager()->openHelpTopic(helpTopicId);
            return;
        }
    }

    actionManager()->openHelpTopic(QStringLiteral("manual:application_settings"));
}

}   // End of namespace
