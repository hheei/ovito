// SPDX-FileCopyrightText: 2026 OVITO GmbH, Germany
// SPDX-License-Identifier: GPL-3.0-only OR MIT

#pragma once


#include <ovito/core/Core.h>
#include <ovito/core/utilities/concurrent/Task.h>

namespace Ovito {

/**
 * This RAII helper class temporarily clears the interactive flag of the current task to establish a non-interactive execution context.
 */
class NoninteractiveContext
{
public:

    /// Constructor.
    NoninteractiveContext() noexcept : _wasInteractive(this_task::get()->setIsInteractive(false)) {
        OVITO_ASSERT(this_task::get());
    }

    /// Destructor.
    ~NoninteractiveContext() {
        OVITO_ASSERT(_task == this_task::get());
        if(_wasInteractive)
            this_task::get()->setIsInteractive(true);
    }

private:

    bool _wasInteractive;

#ifdef OVITO_DEBUG
    Task* _task = this_task::get();
#endif
};

}   // End of namespace
