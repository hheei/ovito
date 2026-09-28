// SPDX-FileCopyrightText: 2026 OVITO GmbH, Germany
// SPDX-License-Identifier: GPL-3.0-only OR MIT

#pragma once


#include <ovito/gui/desktop/GUI.h>
#include <ovito/gui/desktop/dialogs/ApplicationSettingsDialog.h>
#include <ovito/gui/base/mainwin/templates/ModifierTemplates.h>
#include "TemplatesPageBase.h"

namespace Ovito {

/**
 * Page of the application settings dialog, which allows the user to manage the user-defined modifier templates.
 */
class OVITO_GUI_EXPORT ModifierTemplatesPage : public TemplatesPageBase
{
    OVITO_CLASS(ModifierTemplatesPage)
    Q_OBJECT

protected:

    // UI title of the settings page.
    virtual QString settingsPageTitle() override {
        return tr("Modifier Templates");
    }

    // Informational text shown on the page.
    virtual QString settingsPageDescription() override {
        return tr(
            "Modifier templates you define here will appear in the drop-down list of available modifiers, from where you can quickly insert them into a data pipeline. "
            "Templates may consist of several modifiers, allowing you to quickly reuse the same modifier sequence.");
    }

    /// Help topic to open when the user presses the help button.
    virtual QString helpTopicId() const override {
        return QStringLiteral("manual:modifier_templates");
    }

    /// The kind of objects for which templates are managed on this page.
    virtual QString objectTypeName() override {
        return tr("Modifier");
    }

    /// The file type and suffix used for saving and loading templates on this.
    virtual QString templateFileFilter() override {
        return tr("OVITO Modifier Templates (*.ovmod)");
    }

    /// The object that manages the templates shown on this page.
    virtual ObjectTemplates* templateManager() override {
        return ModifierTemplates::get();
    }

    /// When the user is creating a new template, this method populates the list of available objects,
    /// which the user can select to be included in the template.
    virtual QVector<QTreeWidgetItem*> populateAvailableObjectsList(QTreeWidget* objectListWidget, QComboBox* nameBox) override;

public:

    /// \brief Returns an integer value that is used to sort the dialog pages in ascending order.
    virtual int pageSortingKey() const override { return 30; }
};

}   // End of namespace
