// SPDX-FileCopyrightText: 2026 OVITO GmbH, Germany
// SPDX-License-Identifier: GPL-3.0-only OR MIT

#pragma once


#include <ovito/core/Core.h>
#include "Task.h"
#include "Promise.h"

namespace Ovito {

/**
 * A promise-like object that is used during long-running program operations that are performed synchronously by the program's main thread.
 *
 * The operation is automatically put into the 'finished' state by the class' destructor.
 */
class OVITO_CORE_EXPORT MainThreadOperation : public Promise<void>, Task::Scope
{
public:

    enum Kind {
        Isolated, ///< When passed to the constructor, the task is created with no parent.
        Bound,    ///< When passed to the constructor, the created task becomes a child of the current task (if any).
    };

    /// Constructor.
    [[nodiscard]] explicit MainThreadOperation(
        UserInterface& userInterface = *this_task::ui(),
        Kind kind = Bound,
        bool isInteractive = true);

    /// Destructor.
    ~MainThreadOperation();
};

}   // End of namespace
