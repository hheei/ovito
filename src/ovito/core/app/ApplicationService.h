// SPDX-FileCopyrightText: 2026 OVITO GmbH, Germany
// SPDX-License-Identifier: GPL-3.0-only OR MIT

#pragma once


#include <ovito/core/Core.h>

namespace Ovito {

/**
 * \brief Abstract base class for services that want to perform actions on
 *        application startup.
 *
 * If you derive a subclass, a single instance of it will automatically be
 * created by the system and its virtual callback methods will be called at
 * appropriate times during the application's life cycle.
 *
 * For example, it is possible for a plugin to register additional command line
 * options with the global Application object and handle them when they are used
 * by the user.
 */
class OVITO_CORE_EXPORT ApplicationService : public OvitoObject
{
    OVITO_CLASS(ApplicationService)

public:

    /// Registers additional command line options when running in standalone application mode.
    virtual void registerCommandLineOptions(QCommandLineParser& cmdLineParser) {}

    /// Is called by the system during standalone application startup before a main window is created.
    virtual void applicationInitializing() {}

    /// Is called by the system during standalone application startup after a main window has been created.
    virtual void applicationStarting() {}

    /// Is called by the system after the standalone application has been completely initialized.
    virtual void applicationStarted() {}

    /// Controls the order in which startup callbacks are invoked relative to other services.
    /// Services with lower values run first. The default value of 1000 places unordered services last.
    virtual int startupPriority() const { return 1000; }
};

}   // End of namespace
