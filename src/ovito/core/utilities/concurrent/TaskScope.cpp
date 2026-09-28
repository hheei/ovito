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
#include <ovito/core/app/UserInterface.h>
#include "TaskScope.h"
#include "detail/ScopeTask.h"

namespace Ovito {

/******************************************************************************
* Creates the scope's own task, establishing the parent link for a Bound scope.
******************************************************************************/
TaskPtr TaskScope::createScopeTask(Kind kind)
{
    Task* parentTask = (kind == Bound) ? this_task::get() : nullptr;
    std::shared_ptr<UserInterface> ui = parentTask ? parentTask->userInterface() : nullptr;
    const bool isInteractive = parentTask ? parentTask->isInteractive() : false;
    return std::make_shared<detail::ScopeTask>(std::move(ui), parentTask, isInteractive);
}

/******************************************************************************
* Constructor.
******************************************************************************/
TaskScope::TaskScope(Kind kind) :
    _scopeTask(createScopeTask(kind)),
    _ambient(_scopeTask)
{
}

/******************************************************************************
* Destructor: requests stop on all owned children, joins them, then finishes
* the scope's own task.
******************************************************************************/
TaskScope::~TaskScope()
{
    // The ambient Task::Scope installed by this object (the _ambient member, destroyed only after this
    // body returns) must still be in effect, i.e. this->_scopeTask must be the current task. This holds
    // as long as the scope is destroyed in proper (reverse) nesting order with respect to any inner
    // Task::Scope/TaskScope, and on the thread it was created on. The Task::waitFor() join below relies
    // on it (it uses this_task::get() as the waiting task); enforce it here so a violation fails at the
    // cause rather than as a confusing assertion deep inside waitFor().
    OVITO_ASSERT(this_task::get() == _scopeTask.get());

    // Request stop on each child. We cancel the children directly rather than the scope task, so the
    // scope task remains a valid 'waiting task' for the Task::waitFor() join below.
    for(Child& child : _children)
        child.dep->cancel();

    // Join: wait for each child to actually reach a completion channel (returnEarlyIfCanceled = false).
    // On the main thread this pumps the event loop; throwOnError is false because we must not throw
    // from a destructor.
    for(Child& child : _children)
        (void)Task::waitFor(child.dep.get(), /*throwOnError*/ false, /*returnEarlyIfCanceled*/ false, /*cancelWaitingIfAwaitedCanceled*/ false);

    // Tear down the per-child stop callbacks and demand references before finishing the scope task.
    _children.clear();

    // Finish the scope's own task. (The ambient Task::Scope asserted above is restored when the
    // _ambient member is destroyed after this body returns.)
    _scopeTask->setFinished();
}

}   // End of namespace
