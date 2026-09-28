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
