// SPDX-FileCopyrightText: 2026 OVITO GmbH, Germany
// SPDX-License-Identifier: GPL-3.0-only OR MIT

#pragma once

#include <ovito/gui/desktop/app/GuiApplicationService.h>
#include <ovito/gui/desktop/GUI.h>

namespace Ovito {

/**
 * A application-wise service that informs the user about new program updates when they become available.
 */
class OVITO_GUI_EXPORT UpdateNotificationService : public QObject, public GuiApplicationService
{
    Q_OBJECT
    OVITO_CLASS(UpdateNotificationService)

public:

    /// Is called by the system during standalone application startup.
    /// Downloads the news page from the web server and displays it in the command panel.
    void applicationStarting() override;

private:

    /// Extracts the two version strings from the first line of the news webpage.
    /// Pattern of the version strings: <!--vX+.Y+.Z+|vA+.B+.C+-->
    /// where X+.Y+.Z+ are the significant version even shown when "Skip this version" is pressed
    /// and A+.B+.C+ are all program versions shown to every user.
    static QStringList extractVersion(const QString& input);

    /// Called when the web request finishes.
    /// Show the update dialog and set the "ProgramNotice".
    void onWebRequestFinished();

    /// Creates the update popup window.
    /// Checks the newly available version and compares it against the current version
    /// and validates against the "Skip this version" choice by the user.
    void createUpdateDialog(const QStringList& versionMatch) const;

private:

    /// Pointer to the current main window.
    QPointer<MainWindow> _mainWindow;
};

}  // namespace Ovito