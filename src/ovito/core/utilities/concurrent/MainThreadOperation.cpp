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
