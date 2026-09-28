// SPDX-FileCopyrightText: 2026 OVITO GmbH, Germany
// SPDX-License-Identifier: GPL-3.0-only OR MIT

#include <ovito/core/Core.h>
#include <ovito/core/utilities/concurrent/FutureWatcher.h>
#include <ovito/core/utilities/concurrent/detail/TaskOutcome.h>

namespace Ovito {

/******************************************************************************
* Called when the monitored future has reached the finished state.
* Emits the appropriate signals based on the outcome (canceled, error, or completed).
******************************************************************************/
void FutureWatcherBase::futureFinished(Task& task) noexcept
{
    OVITO_ASSERT(this_task::isMainThread());
    OVITO_ASSERT(task.isFinished());

    switch(detail::decodeOutcome(task)) {
        case detail::TaskOutcome::Canceled:
            Q_EMIT canceled();
            break;
        case detail::TaskOutcome::Failed:
            Q_EMIT error(*task.getExceptionIfFailed());
            break;
        case detail::TaskOutcome::Completed:
            Q_EMIT completed();
            break;
    }
}

}   // End of namespace
