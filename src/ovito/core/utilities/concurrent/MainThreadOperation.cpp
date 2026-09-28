// SPDX-FileCopyrightText: 2026 OVITO GmbH, Germany
// SPDX-License-Identifier: GPL-3.0-only OR MIT

#include <ovito/core/Core.h>
#include <ovito/core/app/Application.h>
#include <ovito/core/app/UserInterface.h>
#include <ovito/core/utilities/concurrent/MainThreadOperation.h>
#include <ovito/core/utilities/concurrent/TaskManager.h>
#include <ovito/core/utilities/concurrent/detail/ScopeTask.h>

namespace Ovito {

/******************************************************************************
* Constructor.
******************************************************************************/
MainThreadOperation::MainThreadOperation(UserInterface& userInterface, Kind kind, bool isInteractive) :
    Promise<void>(std::make_shared<detail::ScopeTask>(userInterface.shared_from_this(), kind == Bound ? this_task::get() : nullptr, isInteractive)),
    Task::Scope(task())
{
    // Usage of MainThreadOperation is only permitted in the main thread.
    OVITO_ASSERT_MSG(this_task::isMainThread(), "MainThreadOperation", "MainThreadOperation may only be created in the main thread.");
}

/******************************************************************************
* Destructor.
******************************************************************************/
MainThreadOperation::~MainThreadOperation()
{
    if(TaskPtr task = std::move(_task)) {
        task->setFinished();
    }
}

}   // End of namespace
