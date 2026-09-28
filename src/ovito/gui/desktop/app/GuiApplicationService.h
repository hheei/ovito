// SPDX-FileCopyrightText: 2026 OVITO GmbH, Germany
// SPDX-License-Identifier: GPL-3.0-only OR MIT

#pragma once


#include <ovito/gui/desktop/GUI.h>
#include <ovito/core/app/ApplicationService.h>

namespace Ovito {

/**
 * \brief Abstract base class for plugin services that integrate into the main window GUI.
 */
class OVITO_GUI_EXPORT GuiApplicationService : public ApplicationService
{
    OVITO_CLASS(GuiApplicationService)

public:

    /// Is called when a new main window is created.
    virtual void registerActions(MainWindowUI& ui) {}
};

}   // End of namespace
