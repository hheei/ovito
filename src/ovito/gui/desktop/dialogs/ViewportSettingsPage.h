// SPDX-FileCopyrightText: 2026 OVITO GmbH, Germany
// SPDX-License-Identifier: GPL-3.0-only OR MIT

#pragma once


#include <ovito/gui/desktop/GUI.h>
#include <ovito/gui/desktop/dialogs/ApplicationSettingsDialog.h>
#include <ovito/core/viewport/ViewportSettings.h>

namespace Ovito {

class ConfigureViewportGraphicsDialog;

/**
 * Page of the application settings dialog, which hosts viewport-related program options.
 */
class OVITO_GUI_EXPORT ViewportSettingsPage : public ApplicationSettingsDialogPage
{
    OVITO_CLASS(ViewportSettingsPage)
    Q_OBJECT

public:

    /// Creates the widgets of the settings page.
    virtual void insertSettingsDialogPage(QTabWidget* tabWidget) override;

    /// Lets the settings page to save all values entered by the user.
    virtual void saveValues(QTabWidget* tabWidget) override;

    /// Returns an integer value that is used to sort the dialog pages in ascending order.
    virtual int pageSortingKey() const override { return 10; }

    /// \brief Help topic to open when the user presses the help button.
    virtual QString helpTopicId() const override {
        return QStringLiteral("manual:application_settings.viewports");
    }

private Q_SLOTS:

    /// Shows the dialog for configuring the viewport graphics system.
    void showConfigureViewportGraphicsDialog();

private:

    /// The settings object being modified.
    ViewportSettings _viewportSettings;

    QButtonGroup* _upDirectionGroup;
    QCheckBox* _constrainCameraRotationBox;
    QButtonGroup* _colorScheme;
    QPointer<ConfigureViewportGraphicsDialog> _configureViewportGraphicsDialog;
};

}   // End of namespace
