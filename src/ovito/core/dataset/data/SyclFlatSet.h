// SPDX-FileCopyrightText: 2026 OVITO GmbH, Germany
// SPDX-License-Identifier: GPL-3.0-only OR MIT

#pragma once

#include <ovito/core/Core.h>
#include <ovito/core/dataset/data/DataBuffer.h>

#ifdef OVITO_USE_SYCL

namespace Ovito {

/**
 * An associative container that can be accessed in host code and in SYCL kernels.
 * It uses a SYCL buffer as underlying storage.
*/
template<typename Key, typename Compare = std::less<Key>>
class SyclFlatSet
{
public:

    using key_type = Key;
    using value_type = Key;
    using size_type = std::size_t;
    using difference_type = std::ptrdiff_t;
    using key_compare = Compare;

    /// Constructor that initializes the set from an existing associative container, e.g. a std::set.
    template<typename Container>
    SyclFlatSet(const Container& container) {
        if(!container.empty()) {
            _storage.emplace(detail::allocateSyclBuffer<value_type, 1>(container.size()));
            // Copy the contents of the provided container into our internal flat storage.
            sycl::host_accessor acc(*_storage, sycl::write_only, sycl::no_init);
            std::copy(container.begin(), container.end(), acc.begin());
            OVITO_ASSERT(std::is_sorted(acc.begin(), acc.end(), key_compare{}));
        }
    }

    /// Copying is not supported yet.
    SyclFlatSet(const SyclFlatSet& other) = delete;

    /// Copy assignment is not supported yet.
    SyclFlatSet& operator=(const SyclFlatSet& other) = delete;

    /// Checks if the map is empty.
    bool empty() const { return !_storage.has_value(); }

    /// Returns the number of key-value pairs in the map.
    size_type size() const { return _storage ? _storage->size() : 0; }

    /// Returns the internal data storage of the container.
    const sycl::buffer<value_type>& storage() const {
        OVITO_ASSERT(_storage.has_value());
        return *_storage;
    }

    /// Returns the internal data storage of the container.
    sycl::buffer<value_type>& storage() {
        OVITO_ASSERT(_storage.has_value());
        return *_storage;
    }

public:

    /**
     * An accessor to the SyclFlatSet container that allows performing look-ups inside SYCL kernels.
    */
    class Accessor
    {
    public:

        using container_type = SyclFlatSet;
        using key_type = Key;
        using value_type = Key;
        using size_type = std::size_t;
        using difference_type = std::ptrdiff_t;
        using key_compare = Compare;
        using accessor_type = sycl::accessor<value_type, 1, sycl::access_mode::read>;

        // Constructor.
        Accessor(const container_type& set, sycl::handler& commandGroupHandler) :
            _accessor(!set.empty() ? accessor_type{const_cast<container_type&>(set).storage(), commandGroupHandler} : accessor_type{}) {}

    public:

        /// Returns whether this accessor is valid.
        inline explicit operator bool() const noexcept { return _accessor.get_range()[0] != 0; }

        /// Returns an iterator to the beginning of the set.
        auto begin() const { return _accessor.cbegin(); }

        /// Returns an iterator to the end of the set.
        auto end() const { return _accessor.cend(); }

        /// Finds a specific key in the set.
        auto find(const key_type& key) const {
            auto first = begin();
            auto last = end();
            first = std::lower_bound(first, last, key, key_compare{});
            if(first != last && key_compare{}(key, *first))
                first = last;
            return first;
        }

        /// Checks if the given key is in the set.
        bool contains(const key_type& key) const {
            return find(key) != end();
        }

        /// Finds the index of a specific key in the set.
        auto index_of(const key_type& key) const {
            return find(key) - begin();
        }

        /// Returns the number of keys in the set.
        size_type size() const { return _accessor.get_range()[0]; }

    private:

        /// The internal SYCL buffer accessor.
        accessor_type _accessor;
    };

    /// Creates an accessor that can be used inside a SYCL kernel to access the contents of the set.
    Accessor get_access(sycl::handler& commandGroupHandler) const {
        return Accessor(*this, commandGroupHandler);
    }

private:

    /// The data storage.
    std::optional<sycl::buffer<value_type>> _storage;
};

// Class template deduction guide (CTAD):
template<class Container>
SyclFlatSet(const Container& container) -> SyclFlatSet<typename Container::key_type, typename Container::key_compare>;

}   // End of namespace

#endif