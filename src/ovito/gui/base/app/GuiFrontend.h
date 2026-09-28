// SPDX-FileCopyrightText: 2026 OVITO GmbH, Germany
// SPDX-License-Identifier: GPL-3.0-only OR MIT

#pragma once


#include <ovito/gui/base/GUIBase.h>
#include <ovito/core/utilities/concurrent/MainThreadOperation.h>

namespace Ovito {

/**
 * \brief A selectable frontend of the graphical user interface, e.g. the classic QtWidgets workbench or the Qt Quick workbench.
 *
 * A frontend provides the workbench object that represents the user interface of the application (see createWorkbench()).
 * Frontends are registered with the GuiFrontendRegistry, which is how a plugin that implements an alternative frontend
 * makes it available, and the user selects one of them with the \c --gui command line option. Registration has to be
 * complete by the time the application starts the selected frontend, which is why frontends are contributed by
 * application services (see ApplicationService::applicationInitializing()).
 */
class OVITO_GUIBASE_EXPORT GuiFrontend
{
public:

    /// Destructor.
    virtual ~GuiFrontend() = default;

    /// Returns the name under which this frontend can be selected on the command line (\c --gui=<name>).
    virtual QString name() const = 0;

    /// Returns a human-readable description of this frontend, which is shown when the available frontends are listed.
    virtual QString description() const { return {}; }

    /// Creates the workbench of this frontend and returns the main thread operation that owns it.
    ///
    /// The returned operation must be an isolated operation (MainThreadOperation::Kind::Isolated): the workbench it
    /// holds represents the user interface of the application and therefore has to stay alive on the main thread until
    /// it is shut down.
    virtual MainThreadOperation createWorkbench() const = 0;
};

}   // End of namespace
