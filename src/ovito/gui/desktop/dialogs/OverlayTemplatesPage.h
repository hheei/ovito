// SPDX-FileCopyrightText: 2026 OVITO GmbH, Germany
// SPDX-License-Identifier: GPL-3.0-only OR MIT

#pragma once


#include <ovito/gui/desktop/GUI.h>
#include <ovito/gui/desktop/dialogs/ApplicationSettingsDialog.h>
#include <ovito/gui/base/mainwin/templates/OverlayTemplates.h>
#include "TemplatesPageBase.h"

namespace Ovito {

/**
 * Page of the application settings dialog, which allows the user to manage the user-defined viewport layer templates.
 */
class OVITO_GUI_EXPORT OverlayTemplatesPage : public TemplatesPageBase
{
    OVITO_CLASS(OverlayTemplatesPage)
    Q_OBJECT

protected:

    // UI title of the settings page.
    virtual QString settingsPageTitle() override {
        return tr("Viewport Layer Templates");
    }

    // Informational text shown on the page.
    virtual QString settingsPageDescription() override {
        return tr("Viewport layer templates you define here will appear in the drop-down list of available layers, from where you can quickly add them to the current viewport.");
    }

    /// Help topic to open when the user presses the help button.
    virtual QString helpTopicId() const override {
        return QStringLiteral("manual:viewport_layer_templates");
    }

    /// The kind of objects for which templates are managed on this page.
    virtual QString objectTypeName() override {
        return tr("Viewport layer");
    }

    /// The file type and suffix used for saving and loading templates on this.
    virtual QString templateFileFilter() override {
        return tr("OVITO Viewport Layer Templates (*.ovlayer)");
    }

    /// The object that manages the templates shown on this page.
    virtual ObjectTemplates* templateManager() override {
        return OverlayTemplates::get();
    }

    /// When the user is creating a new template, this method populates the list of available objects,
    /// which the user can select to be included in the template.
    virtual QVector<QTreeWidgetItem*> populateAvailableObjectsList(QTreeWidget* objectListWidget, QComboBox* nameBox) override;

public:

    /// \brief Returns an integer value that is used to sort the dialog pages in ascending order.
    virtual int pageSortingKey() const override { return 40; }
};

}   // End of namespace
