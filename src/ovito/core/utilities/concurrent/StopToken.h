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
#include "Task.h"
#include "detail/TaskCallback.h"

namespace Ovito {

template<typename Callback> class StopCallback;

/**
 * \brief A lightweight, copyable handle that observes the cancellation ("stopped") channel of a Task.
 *
 * This is the first-class, named *view* of OVITO's unified cancellation concept (see ARCHITECTURE.md
 * in this directory). It is deliberately modeled on `std::stop_token` so that a future migration is
 * mechanical, but it is backed by a Task so there remains exactly one canceled-state of record
 * (`Task::Canceled`). It is purely additive over the existing machinery and introduces no second
 * cancellation system.
 *
 * A StopToken keeps the observed Task *object* alive (so its state stays readable and callbacks can
 * be registered), but it does not express *demand* on the task's result the way Future /
 * detail::TaskDependency do. Holding a token therefore never keeps the underlying work running.
 */
class StopToken
{
public:

    /// Constructs an empty token that is not associated with any stop state.
    StopToken() noexcept = default;

    /// Constructs a token observing the stopped channel of the given task.
    explicit StopToken(TaskPtr task) noexcept : _task(std::move(task)) {}

    /// Returns whether cancellation has been requested on the associated task.
    /// Equivalent to `Task::isCanceled()`. Returns false for an empty token.
    [[nodiscard]] bool stop_requested() const noexcept { return _task && _task->isCanceled(); }

    /// Returns whether the associated task can still be — or has already been — canceled.
    /// Returns false for an empty token or a task that finished without being canceled.
    [[nodiscard]] bool stop_possible() const noexcept { return _task && (_task->isCanceled() || !_task->isFinished()); }

    /// Returns whether this token is associated with a stop state.
    explicit operator bool() const noexcept { return static_cast<bool>(_task); }

    /// Two tokens compare equal if they refer to the same stop state (or are both empty).
    [[nodiscard]] friend bool operator==(const StopToken& a, const StopToken& b) noexcept { return a._task == b._task; }

    /// Exchanges the stop state of two tokens.
    void swap(StopToken& other) noexcept { _task.swap(other._task); }

private:

    /// The Task whose stopped channel this token observes. May be null (empty token).
    TaskPtr _task;

    template<typename Callback> friend class StopCallback;
    friend class StopSource;
};

/**
 * \brief A handle for requesting cancellation, paired with the StopToken(s) that observe it.
 *
 * Modeled on `std::stop_source`, backed by a Task. A StopSource owns a fresh stop state (a Task)
 * and is responsible for driving it to the 'finished' state on destruction — mirroring how a Promise
 * finishes its task. It is move-only (unlike `std::stop_source`, which is copyable); the shared,
 * copyable view is StopToken.
 *
 * In most OVITO code the role of a stop source is already played by an existing primitive — a Promise
 * (which owns and finishes a Task) or the demand count of detail::TaskDependency. Use StopSource only
 * when you need a standalone cancellation context that is not tied to a Promise/Future.
 */
class StopSource
{
public:

    /// Creates a stop source with a fresh, independent stop state.
    StopSource() : _task(std::make_shared<Task>()) {}

    /// Creates an empty stop source not associated with any stop state.
    explicit StopSource(std::nullptr_t) noexcept {}

    /// Move constructor.
    StopSource(StopSource&& other) noexcept = default;

    /// Move assignment.
    StopSource& operator=(StopSource&& other) noexcept {
        StopSource(std::move(other)).swap(*this);
        return *this;
    }

    /// A stop source is not copyable.
    StopSource(const StopSource&) = delete;
    StopSource& operator=(const StopSource&) = delete;

    /// Destructor. Drives an owned, not-yet-finished stop state to the canceled+finished state,
    /// so the Task does not outlive this source in an unfinished state.
    ~StopSource() {
        if(_task && !_task->isFinished()) {
            _task->cancel();
            _task->setFinished();
        }
    }

    /// Returns a token observing this source's stop state.
    [[nodiscard]] StopToken get_token() const noexcept { return StopToken(_task); }

    /// Requests cancellation of the associated stop state.
    /// Returns true if this call performed the request, false if it had already been requested
    /// (or there is no associated stop state).
    bool request_stop() noexcept {
        if(!_task || _task->isCanceled())
            return false;
        _task->cancel();
        return true;
    }

    /// Returns whether cancellation has been requested on the associated stop state.
    [[nodiscard]] bool stop_requested() const noexcept { return _task && _task->isCanceled(); }

    /// Returns whether this source is associated with a stop state.
    [[nodiscard]] bool stop_possible() const noexcept { return static_cast<bool>(_task); }

    /// Exchanges the stop state of two sources.
    void swap(StopSource& other) noexcept { _task.swap(other._task); }

private:

    /// The owned stop state. May be null (empty / moved-from source).
    TaskPtr _task;
};

/**
 * \brief Runs a callback when cancellation is requested on a StopToken's stop state.
 *
 * Modeled on `std::stop_callback`. The callback fires exactly once, when the observed task enters the
 * stopped channel; if cancellation has *already* been requested at construction time, it fires
 * immediately in the constructing thread. It never fires for a task that finishes through the value or
 * error channel. The callback is deregistered on destruction.
 *
 * \note The callback is invoked while the task's internal mutex is held (the existing TaskCallback
 *       contract). It must be noexcept and must not call back into the same task's locking API.
 */
template<typename Callback>
class StopCallback : public detail::TaskCallback<StopCallback<Callback>>
{
public:

    static_assert(std::is_nothrow_invocable_r_v<void, Callback>, "The stop callback must be callable with no arguments and be noexcept.");

    /// Registers the callback with the given token's stop state.
    template<typename C>
    explicit StopCallback(const StopToken& token, C&& callback) : _callback(std::forward<C>(callback)) {
        if(token._task)
            this->registerCallback(token._task.get(), /*replayStateChanges*/ true);
    }

    /// A stop callback is neither copyable nor movable (it registers a pointer to itself).
    StopCallback(const StopCallback&) = delete;
    StopCallback& operator=(const StopCallback&) = delete;

private:

    /// Invoked by the TaskCallback machinery on every state change of the observed task.
    void taskStateChangedCallback(int state, Task::MutexLock& lock) noexcept {
        if((state & Task::Canceled) && !_invoked) {
            _invoked = true;
            _callback();
        }
    }

    /// The user-provided callback to run upon cancellation.
    Callback _callback;

    /// Guards against invoking the callback more than once.
    bool _invoked = false;

    template<typename Derived> friend class detail::TaskCallback;
};

// Deduction guide so `StopCallback cb(token, lambda);` deduces the callback type.
template<typename C>
StopCallback(const StopToken&, C&&) -> StopCallback<std::decay_t<C>>;

namespace this_task {

/// Returns a StopToken observing the cancellation state of the task currently active in this thread.
/// Returns an empty token if there is no active task. This is the token-based view of the same
/// ambient cancellation context read by `this_task::isCanceled()` / `throwIfCanceled()`.
[[nodiscard]] inline StopToken get_stop_token() noexcept {
    if(Task* task = get())
        return StopToken(task->shared_from_this());
    return StopToken();
}

}   // End of namespace this_task

}   // End of namespace
