// SPDX-FileCopyrightText: 2026 OVITO GmbH, Germany
// SPDX-License-Identifier: GPL-3.0-only OR MIT

#pragma once


#include <ovito/gui/desktop/GUI.h>
#include <ovito/gui/desktop/dialogs/ApplicationSettingsDialog.h>
#include <ovito/gui/base/mainwin/templates/ObjectTemplates.h>

namespace Ovito {

/**
 * Abstract base class for pages in the application settings dialog, which allows the user to manage object templates, e.g. modifier and viewport layer templates.
 */
class OVITO_GUI_EXPORT TemplatesPageBase : public ApplicationSettingsDialogPage
{
    OVITO_CLASS(TemplatesPageBase)
    Q_OBJECT

protected:

    // UI title of the settings page.
    virtual QString settingsPageTitle() = 0;

    // Informational text shown on the page.
    virtual QString settingsPageDescription() = 0;

    /// The kind of objects for which templates are managed on this page.
    virtual QString objectTypeName() = 0;

    /// The file type and suffix used for saving and loading templates on this.
    virtual QString templateFileFilter() = 0;

    /// The object that manages the templates shown on this page.
    virtual ObjectTemplates* templateManager() = 0;

    /// The kind of objects for which templates are managed on this page.
    QString objectTypeNameLC() { return objectTypeName().toLower(); }

    /// When the user is creating a new template, this method populates the list of available objects,
    /// which the user can select to be included in the template.
    virtual QVector<QTreeWidgetItem*> populateAvailableObjectsList(QTreeWidget* objectListWidget, QComboBox* nameBox) = 0;

public:

    /// \brief Creates the widgets of the settings page.
    virtual void insertSettingsDialogPage(QTabWidget* tabWidget) override;

    /// \brief Lets the settings page to save all values entered by the user.
    virtual void saveValues(QTabWidget* tabWidget) override;

    /// \brief Lets the settings page restore the original values of changed settings when the user presses the Cancel button.
    virtual void restoreValues(QTabWidget* tabWidget) override;

private Q_SLOTS:

    /// Is invoked when the user presses the "Create template" button.
    void onCreateTemplate();

    /// Is invoked when the user presses the "Delete template" button.
    void onDeleteTemplate();

    /// Is invoked when the user presses the "Rename template" button.
    void onRenameTemplate();

    /// Is invoked when the user presses the "Export templates" button.
    void onExportTemplates();

    /// Is invoked when the user presses the "Import templates" button.
    void onImportTemplates();

private:

    QListView* _listWidget;
    bool _dirtyFlag = false;
};

}   // End of namespace
