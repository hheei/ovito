// SPDX-FileCopyrightText: 2026 OVITO GmbH, Germany
// SPDX-License-Identifier: GPL-3.0-only OR MIT

#pragma once


#include <ovito/particles/gui/ParticlesGui.h>
#include <ovito/gui/desktop/dialogs/ApplicationSettingsDialog.h>

namespace Ovito {

/**
 * Page of the application settings dialog, which hosts particle-related options.
 */
class ParticleSettingsPage : public ApplicationSettingsDialogPage
{
    OVITO_CLASS(ParticleSettingsPage)
    Q_OBJECT

public:

    /// \brief Creates the widget.
    virtual void insertSettingsDialogPage(QTabWidget* tabWidget) override;

    /// \brief Lets the settings page to save all values entered by the user.
    /// \param settingsDialog The settings dialog box.
    virtual void saveValues(QTabWidget* tabWidget) override;

    /// \brief Returns an integer value that is used to sort the dialog pages in ascending order.
    virtual int pageSortingKey() const override { return 50; }

    /// \brief Help topic to open when the user presses the help button.
    virtual QString helpTopicId() const override {
        return QStringLiteral("manual:application_settings.particles");
    }

public Q_SLOTS:

    /// Restores the built-in default particle colors and sizes.
    void restoreBuiltinParticlePresets();

    /// Exports the current particle type defaults to a JSON theme file.
    void exportTheme();

    /// Imports particle type defaults from a JSON theme file.
    void importTheme();

private:

    QTreeWidget* _predefTypesTable;
    QTreeWidgetItem* _particleTypesItem;
    QTreeWidgetItem* _structureTypesItem;
};

}   // End of namespace
