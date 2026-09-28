// SPDX-FileCopyrightText: 2026 OVITO GmbH, Germany
// SPDX-License-Identifier: GPL-3.0-only OR MIT

#pragma once


#include <ovito/core/Core.h>

namespace Ovito {

/******************************************************************************
 * \brief A thread-specific container for objects of type T.
 *
 * This class allows to create multiple objects of type T, one for each thread
 * from which the create() method is called. The objects are stored in a map
 * that is indexed by the thread ID.
******************************************************************************/
template<typename T>
class EnumerableThreadSpecific
{
public:

    template<typename... Args>
    T& create(Args&&... args) {
        std::lock_guard<std::mutex> lock(_mutex);
        return _data.try_emplace(std::this_thread::get_id(), std::forward<Args>(args)...).first->second;
    }

    template<typename Function>
    void visitEach(Function&& function) {
        std::lock_guard<std::mutex> lock(_mutex);
        for(auto& entry : _data)
            function(entry.second);
    }

private:

    std::map<std::thread::id, T> _data;
    std::mutex _mutex;
};

}   // End of namespace
