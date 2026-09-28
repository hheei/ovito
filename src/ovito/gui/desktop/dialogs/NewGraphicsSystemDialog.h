// SPDX-FileCopyrightText: 2026 OVITO GmbH, Germany
// SPDX-License-Identifier: GPL-3.0-only OR MIT

#pragma once

#include <ovito/gui/desktop/app/GuiApplicationService.h>
#include <ovito/gui/desktop/GUI.h>

namespace Ovito {

/**
 * Application service that presents a one-time GPU adapter selection dialog on the
 * first startup with OVITO's new native QRhi-based graphics rendering engine.
 */
class OVITO_GUI_EXPORT NewGraphicsSystemService : public QObject, public GuiApplicationService
{
    Q_OBJECT
    OVITO_CLASS(NewGraphicsSystemService)

public:

    /// Is called by the system during standalone application startup after the main window is created.
    /// Shows the first-run GPU adapter selection dialog if the user has not yet confirmed a choice.
    void applicationStarting() override;

    /// Runs after the licensing services (100, 200) and before the update notification (1000).
    int startupPriority() const override { return 300; }
};

}  // namespace Ovito
