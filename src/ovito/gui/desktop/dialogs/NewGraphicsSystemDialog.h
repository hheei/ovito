////////////////////////////////////////////////////////////////////////////////////////
//
//  Copyright 2026 OVITO GmbH, Germany
//
//  This file is part of OVITO (Open Visualization Tool).
//
//  OVITO is free software; you can redistribute it and/or modify it either under the
//  terms of the GNU General Public License version 3 as published by the Free Software
//  Foundation (the "GPL") or, at your option, under the terms of the MIT License.
//  If you do not alter this notice, a recipient may use your version of this
//  file under either the GPL or the MIT License.
//
//  You should have received a copy of the GPL along with this program in a
//  file LICENSE.GPL.txt.  You should have received a copy of the MIT License along
//  with this program in a file LICENSE.MIT.txt
//
//  This software is distributed on an "AS IS" basis, WITHOUT WARRANTY OF ANY KIND,
//  either express or implied. See the GPL or the MIT License for the specific language
//  governing rights and limitations.
//
////////////////////////////////////////////////////////////////////////////////////////

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
