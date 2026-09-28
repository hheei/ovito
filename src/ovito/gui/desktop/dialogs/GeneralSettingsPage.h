// SPDX-FileCopyrightText: 2026 OVITO GmbH, Germany
// SPDX-License-Identifier: GPL-3.0-only OR MIT

#pragma once


#include <ovito/gui/desktop/GUI.h>
#include <ovito/gui/desktop/dialogs/ApplicationSettingsDialog.h>

namespace Ovito {

/**
 * Page of the application settings dialog, which hosts general program options.
 */
class OVITO_GUI_EXPORT GeneralSettingsPage : public ApplicationSettingsDialogPage
{
    OVITO_CLASS(GeneralSettingsPage)

public:

    /// Default constructor.
    explicit GeneralSettingsPage() = default;

    /// \brief Creates the widget.
    virtual void insertSettingsDialogPage(QTabWidget* tabWidget) override;

    /// \brief Lets the settings page to save all values entered by the user.
    virtual void saveValues(QTabWidget* tabWidget) override;

    /// \brief Returns an integer value that is used to sort the dialog pages in ascending order.
    virtual int pageSortingKey() const override { return 1; }

private:

    QCheckBox* _keepDirHistory;
    QCheckBox* _enableAutomaticDarkMode;
    QCheckBox* _useNativeFileDialog;
    QButtonGroup* _importMultipleFilesBehavior;
#if !defined(OVITO_BUILD_APPSTORE_VERSION)
    QCheckBox* _enableUpdateChecks;
#endif
};

}   // End of namespace
