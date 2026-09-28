// SPDX-FileCopyrightText: 2026 OVITO GmbH, Germany
// SPDX-License-Identifier: GPL-3.0-only OR MIT

#pragma once


#include <ovito/core/Core.h>

namespace Ovito {

/**
 * \brief An executor that runs the closure routine in the main thread and in the context of a given OvitoObject.
 *        The closure routine won't be executed in case the object gets deleted before the work could be started.
 *        Execution always happens deferred, i.e., when controls returns to the event loop.
 */
class OVITO_CORE_EXPORT DeferredObjectExecutor
{
public:

    /// Constructor.
    explicit DeferredObjectExecutor(OOWeakRef<const OvitoObject> contextObject) noexcept : _contextObject(std::move(contextObject)) {}

    /// Executes some work.
    template<typename Function, typename... Args>
    void execute(Function&& f, Args&&... args) const& noexcept;

    /// Executes some work.
    template<typename Function, typename... Args>
    void execute(Function&& f, Args&&... args) && noexcept;

    /// Returns the object this executor is associated with.
    /// Work submitted to this executor will be executed in the context of the object.
    const OOWeakRef<const OvitoObject>& contextObject() const { return _contextObject; }

private:

    /// The object work will be submitted to. Work will be executed in the context of this object,
    /// which means it will be automatically canceled if the object gets deleted before the work
    /// is done.
    OOWeakRef<const OvitoObject> _contextObject;
};

}   // End of namespace
