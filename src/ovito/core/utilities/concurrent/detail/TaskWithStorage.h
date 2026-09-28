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

#pragma once


#include <ovito/core/Core.h>
#include <ovito/core/utilities/concurrent/Task.h>

namespace Ovito {

/**
 * \brief Composite class template that packages a Task together with the storage for the task's results.
 */
template<class R, class TaskBase = Task>
class TaskWithStorage : public TaskBase
{
public:

    /// Constructor which leaves the results storage uninitialized.
    explicit TaskWithStorage(Task::State initialState, std::nullopt_t) : TaskBase(initialState, nullptr) {}

    /// Constructor default-constructing the task's results storage.
    explicit TaskWithStorage(Task::State initialState) : TaskBase(initialState, nullptr) {
        constructResult();
    }

    /// Constructor initializing the task's results storage with a value.
    template<typename InitialValue>
    explicit TaskWithStorage(Task::State initialState, InitialValue&& initialResult) : TaskBase(initialState, nullptr) {
        constructResult(std::forward<InitialValue>(initialResult));
    }

    /// Destructor: destroys the result value, but only if it was actually constructed.
    ~TaskWithStorage() {
        if(this->_resultsStorage != nullptr)
            _result.~R();
    }

    /// Constructs the task's result value in place from the given value. May be called only once.
    template<typename R2>
    void setResult(R2&& value) {
        constructResult(std::forward<R2>(value));
    }

protected:

    /// Provides direct read/write access to the internal results. Only valid once the result has been constructed.
    R& resultStorage() {
        OVITO_ASSERT(this->_resultsStorage != nullptr);
        return _result;
    }

private:

    /// Constructs the result value in place and publishes its address through the base class'
    /// _resultsStorage pointer, which thereby records that the result is now constructed.
    template<typename... Args>
    void constructResult(Args&&... args) {
        OVITO_ASSERT(this->_resultsStorage == nullptr); // The result value may be constructed only once.
        ::new(static_cast<void*>(std::addressof(_result))) R(std::forward<Args>(args)...);
        this->_resultsStorage = std::addressof(_result);
#ifdef OVITO_DEBUG
        // Keep the debug-build bookkeeping (used by getResult()/takeResult() assertions) in sync.
        this->_hasResultsStored.store(true);
#endif
    }

    /// Storage for the task's result value, held in a union so its lifetime can be managed manually:
    /// the nullopt constructor leaves it uninitialized and setResult() constructs it in place.
    union { R _result; };
};

/**
 * \brief Composite class template that packages a Task together with the storage for the task's results.
 */
template<class TaskBase>
class TaskWithStorage<void, TaskBase> : public TaskBase
{
public:

    /// Constructor which leaves results storage uninitialized.
    explicit TaskWithStorage(Task::State initialState) : TaskBase(initialState) {}

    /// Constructor which leaves results storage uninitialized.
    explicit TaskWithStorage(Task::State initialState, std::nullopt_t) : TaskBase(initialState) {}
};

}   // End of namespace
