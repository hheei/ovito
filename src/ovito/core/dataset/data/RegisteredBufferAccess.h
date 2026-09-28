// SPDX-FileCopyrightText: 2026 OVITO GmbH, Germany
// SPDX-License-Identifier: GPL-3.0-only OR MIT

#pragma once


#include <ovito/core/Core.h>
#include <ovito/core/dataset/data/DataBuffer.h>

namespace Ovito {

/// Helper class that gets created when direct access to a property's memory is requested
/// from the Python side. Instances of the class register themselves with a TaskManager,
/// which allows to shutdown all existing memory accessor before the SYCL queue gets closed.
#ifndef OVITO_USE_SYCL
class OVITO_CORE_EXPORT RegisteredBufferAccess
{
public:

    /// Constructor.
    RegisteredBufferAccess(DataBuffer& buffer) : _accessor(&buffer), _buffer(&buffer) {
        // Flag the buffer as being accessed externally.
        OVITO_ASSERT(!buffer.isBeingAccessedExternally());
        buffer._isBeingAccessedExternally.store(true, std::memory_order_release);
    }

    /// Destructor.
    ~RegisteredBufferAccess() {
        // After the buffer's memory has been accessed externally, inform the buffer object that its contents may have changed.
        _buffer->invalidateCachedInfo();
        // Reset the flag that indicates that the buffer's memory is being accessed externally.
        OVITO_ASSERT(_buffer->isBeingAccessedExternally());
        _buffer->_isBeingAccessedExternally.store(false, std::memory_order_release);
    }

    /// Returns a C pointer to the buffer's internal storage.
    const void* dataPointer() { return _accessor.cdata(); }

    /// Returns a pointer to the buffer object.
    DataBuffer* buffer() const { return _buffer.get(); }

private:

    /// To keep the buffer object alive while it is being accessed.
    OORef<DataBuffer> _buffer;

    /// Internal memory accessor.
    RawBufferReadAccess _accessor;
};
#else
class OVITO_CORE_EXPORT RegisteredBufferAccess
{
public:

    /// The internal SYCL buffer accessor.
    using accessor_type = sycl::host_accessor<DataBuffer::Byte, 1, sycl::access_mode::read_write>;

    /// Constructor.
    RegisteredBufferAccess(DataBuffer& buffer, TaskManager& taskManager) : _buffer(&buffer), _syclAccessor(buffer.size() != 0 ? accessor_type{buffer.syclBuffer()} : accessor_type{}), _next(taskManager._registeredBufferAccessors), _listHead(&taskManager._registeredBufferAccessors) {
        if(_next) _next->_prev = this;
        *_listHead = this;

        // Flag the buffer as being accessed externally.
        OVITO_ASSERT(!buffer.isBeingAccessedExternally());
        buffer._isBeingAccessedExternally.store(true, std::memory_order_release);
    }

    /// Returns a pointer to the buffer object.
    DataBuffer* buffer() const { return _buffer.get(); }

    ~RegisteredBufferAccess() {
        if(_prev == nullptr) {
            *_listHead = _next;
            if(_next) _next->_prev = nullptr;
        }
        else {
            _prev->_next = _next;
            if(_next) _next->_prev = _prev;
        }

        // After the buffer's memory has been accessed externally, inform the buffer object that its contents may have changed.
        _buffer->invalidateCachedInfo();
        // Reset the flag that indicates that the buffer's memory is being accessed externally.
        OVITO_ASSERT(_buffer->isBeingAccessedExternally());
        _buffer->_isBeingAccessedExternally.store(false, std::memory_order_release);
    }

    /// Returns a C pointer to the buffer's internal storage.
    const void* dataPointer() { return !_syclAccessor.empty() ? _syclAccessor.get_pointer() : nullptr; }

private:

    /// To keep the buffer object alive while it is being accessed.
    OORef<DataBuffer> _buffer;

    /// SYCL host memory accessor.
    accessor_type _syclAccessor;

    /// Linked list to keep track of all existing instances of this class.
    /// The linked list is maintained by the TaskManager this SYCL accessor is associated with.
    RegisteredBufferAccess* _next = nullptr;
    RegisteredBufferAccess* _prev = nullptr;
    RegisteredBufferAccess** _listHead = nullptr;

    friend class TaskManager;
};
#endif

}   // End of namespace