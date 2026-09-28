// SPDX-FileCopyrightText: 2026 OVITO GmbH, Germany
// SPDX-License-Identifier: GPL-3.0-only OR MIT

#pragma once


#include <ovito/gui/desktop/GUI.h>
#include <ovito/gui/base/app/GuiFrontend.h>

namespace Ovito {

/**
 * \brief The classic frontend of OVITO, which presents the workbench as a QtWidgets main window.
 *
 * This is the default frontend of the application (see GuiApplication) and the one the QML frontend has to reach
 * feature parity with.
 */
class OVITO_GUI_EXPORT QtWidgetsFrontend : public GuiFrontend
{
public:

    /// Returns the name under which this frontend is selected on the command line.
    virtual QString name() const override;

    /// Returns a human-readable description of this frontend.
    virtual QString description() const override;

    /// Creates the classic main window and its user interface object.
    virtual MainThreadOperation createWorkbench() const override;
};

}   // End of namespace
