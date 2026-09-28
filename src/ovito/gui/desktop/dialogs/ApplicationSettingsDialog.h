// SPDX-FileCopyrightText: 2026 OVITO GmbH, Germany
// SPDX-License-Identifier: GPL-3.0-only OR MIT

#pragma once


#include <ovito/gui/desktop/GUI.h>
#include <ovito/core/oo/OvitoObject.h>

namespace Ovito {

class ApplicationSettingsDialog;        // defined below.

/**
 * \brief Abstract base class for tab providers for the application's settings dialog.
 */
class OVITO_GUI_EXPORT ApplicationSettingsDialogPage : public QObject, public OvitoObject, public UserInterfaceComponent<MainWindowUI>
{
    OVITO_CLASS(ApplicationSettingsDialogPage)
    Q_OBJECT

protected:

    /// Base class constructor.
    explicit ApplicationSettingsDialogPage() = default;

public:

    /// \brief Creates the tab that is inserted into the settings dialog.
    /// \param tabWidget The QTabWidget into which the method should insert the settings page.
    virtual void insertSettingsDialogPage(QTabWidget* tabWidget) = 0;

    /// \brief Lets the settings page validate the values entered by the user before saving them.
    /// \return true if the settings are valid; false if settings need to be corrected by the user and the dialog should not be closed.
    virtual bool validateValues(QTabWidget* tabWidget) { return true; }

    /// \brief Lets the settings page to save all values entered by the user.
    virtual void saveValues(QTabWidget* tabWidget) {}

    /// \brief Lets the settings page restore the original values of changed settings.
    virtual void restoreValues(QTabWidget* tabWidget) {}

    /// \brief Returns an integer value that is used to sort the dialog pages in ascending order.
    virtual int pageSortingKey() const { return 1000; }

    /// Returns the help topic ID for this settings page.
    /// An empty string means that no help topic is available for this page.
    /// Then, the main help topic of the application settings dialog is opened instead.
    virtual QString helpTopicId() const { return {}; }

    /// Returns the parent dialog hosting this settings page.
    ApplicationSettingsDialog* settingsDialog() const { OVITO_ASSERT(_settingsDialog); return _settingsDialog; }

private:

    ApplicationSettingsDialog* _settingsDialog = nullptr;

    friend class ApplicationSettingsDialog;
};

/**
 * \brief The dialog window that lets the user change the global application settings.
 *
 * Plugins can add additional pages to this dialog by deriving new classes from
 * the ApplicationSettingsDialogPage class.
 */
class OVITO_GUI_EXPORT ApplicationSettingsDialog : public QDialog, public UserInterfaceComponent<MainWindowUI>
{
    Q_OBJECT

public:

    /// Constructs the dialog window.
    ApplicationSettingsDialog(MainWindowUI& ui, OvitoClassPtr startPage = nullptr);

public Q_SLOTS:

    /// This is called when the user has pressed the OK button of the settings dialog.
    /// Validates and saves all settings made by the user and closes the dialog box.
    void onOk();

    /// This is called when the user closes the dialog box using the Cancel button.
    void onCancel();

    /// This is called when the user has pressed the help button of the settings dialog.
    void onHelp();

private:

    QVector<OORef<ApplicationSettingsDialogPage>> _pages;
    QTabWidget* _tabWidget;
};

}   // End of namespace
