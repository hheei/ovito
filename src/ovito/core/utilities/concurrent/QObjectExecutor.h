// SPDX-FileCopyrightText: 2026 OVITO GmbH, Germany
// SPDX-License-Identifier: GPL-3.0-only OR MIT

#pragma once


#include <ovito/core/Core.h>
#include <ovito/core/app/Application.h>
#include <ovito/core/utilities/concurrent/TaskManager.h>

namespace Ovito {

/**
 * \brief An executor that runs the closure routine in the main thread and in the context of a given QObject.
 *        The closure routine won't be executed in case the object is destroyed before the work could be started.
 *        Execution may happen immediately when the executor is invoked from the main thread.
 */
class OVITO_CORE_EXPORT QObjectExecutor
{
public:

    /// Constructor.
    explicit QObjectExecutor(const QObject* contextObject) noexcept : _contextObject(contextObject) {
        OVITO_ASSERT_MSG(contextObject == nullptr || this_task::isMainThread(), "QObjectExecutor::QObjectExecutor", "QObjectExecutor must be created in the main thread.");
        OVITO_ASSERT_MSG(contextObject == nullptr || contextObject->thread() == QThread::currentThread(), "QObjectExecutor::QObjectExecutor", "QObjectExecutor can only be used with QObjects living in the main thread.");
    }

    /// Executes some work.
    template<typename Function, typename... Args>
    void execute(Function&& f, Args&&... args) const& noexcept;

    /// Executes some work.
    template<typename Function, typename... Args>
    void execute(Function&& f, Args&&... args) && noexcept;

    /// Returns the object this executor is associated with.
    /// Work submitted to this executor will be executed in the context of the object.
    const QPointer<const QObject>& contextObject() const { return _contextObject; }

private:

    /// The object work will be submitted to. Work will be executed in the context of this object,
    /// which means it will be automatically canceled if the object gets deleted before the work
    /// is done.
    QPointer<const QObject> _contextObject;
};

template<typename Function, typename... Args>
inline void QObjectExecutor::execute(Function&& f, Args&&... args) const& noexcept
{
    static_assert(std::is_invocable_v<Function, Args...>, "The function must be invocable with the right arguments.");
    static_assert(std::is_invocable_r_v<void, Function, Args...>, "The function must return void.");
    static_assert(std::is_nothrow_invocable_r_v<void, Function, Args...>, "The function must be noexcept.");

    // If we are in the main thread already, we can immediately execute the work.
    // Otherwise, schedule its execution in the main thread.
    if(this_task::isMainThread()) {
        if(!contextObject().isNull())
            std::invoke(std::forward<Function>(f), std::forward<Args>(args)...);
    }
    else if(!contextObject().isNull()) {
        Application::instance()->taskManager().submitWork([contextObject = contextObject(), f = std::forward<Function>(f), ...args = std::forward<Args>(args)]() mutable noexcept {
            if(!contextObject.isNull())
                std::invoke(std::move(f), std::move(args)...);
        });
    }
}

template<typename Function, typename... Args>
inline void QObjectExecutor::execute(Function&& f, Args&&... args) && noexcept
{
    static_assert(std::is_invocable_v<Function, Args...>, "The function must be invocable with the right arguments.");
    static_assert(std::is_invocable_r_v<void, Function, Args...>, "The function must return void.");
    static_assert(std::is_nothrow_invocable_r_v<void, Function, Args...>, "The function must be noexcept.");

    // If we are in the main thread already, we can immediately execute the work.
    // Otherwise, schedule its execution in the main thread.
    if(this_task::isMainThread()) {
        if(!contextObject().isNull())
            std::invoke(std::forward<Function>(f), std::forward<Args>(args)...);
    }
    else if(!contextObject().isNull()) {
        Application::instance()->taskManager().submitWork([contextObject = std::move(_contextObject), f = std::forward<Function>(f), ...args = std::forward<Args>(args)]() mutable noexcept {
            if(!contextObject.isNull())
                std::invoke(std::move(f), std::move(args)...);
        });
    }
}

}   // End of namespace
