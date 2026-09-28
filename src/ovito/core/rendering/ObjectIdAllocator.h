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

namespace Ovito {

/**
 * \brief Manages allocation and reuse of contiguous ranges of uint32_t object IDs
 *        used for rendering and object picking.
 *
 * Object ID 0 is reserved to mean "no hit"; valid IDs start at 1.
 *
 * The allocator is optimised for the common single-ID allocation path:
 * freed single IDs are kept in a fast stack (O(1) pop) and consumed first.
 * Larger contiguous ranges are tracked in a sorted free-list that is kept
 * coalesced to allow best-fit allocation and efficient neighbour merging.
 *
 * \note Not thread-safe. Must only be called from the render thread.
 * \note All \c ObjectHandle instances must be destroyed before the
 *       owning \c ObjectIdAllocator is destroyed; the \c ObjectHandle class
 *       holds a raw back-pointer to the allocator and will call into it
 *       on the last reference's destruction.
 */
class OVITO_CORE_EXPORT ObjectIdAllocator
{
public:

	/// The largest object ID the allocator will ever hand out. The top bit (bit 31) is
	/// reserved as the renderer's ExcludeFromOutline flag on raytracer instance IDs and is
	/// stripped off again during object picking, so valid IDs occupy only the lower 31 bits.
	static constexpr uint32_t MAX_OBJECT_ID = 0x7FFFFFFFu;

	/// Non-copyable, non-movable (raw pointers back to *this are held by ObjectHandle).
	ObjectIdAllocator() = default;
	ObjectIdAllocator(const ObjectIdAllocator&) = delete;
	ObjectIdAllocator& operator=(const ObjectIdAllocator&) = delete;

	/**
	 * \brief A reference-counted handle to an allocated range of object IDs.
	 *
	 * Behaves like a \c std::shared_ptr: copying keeps the allocation alive;
	 * the IDs are returned to the allocator when the last copy is destroyed.
	 *
	 * An implicit conversion to \c uint32_t returns the base (first) ID of the
	 * range. A default-constructed \c ObjectHandle converts to 0 (invalid).
	 */
	class ObjectHandle
	{
	public:

		/// Default constructor — represents an empty/invalid allocation.
		ObjectHandle() noexcept = default;

		/// Copy/move construction and assignment use shared_ptr semantics.
		ObjectHandle(const ObjectHandle&) noexcept = default;
		ObjectHandle& operator=(const ObjectHandle&) noexcept = default;
		ObjectHandle(ObjectHandle&&) noexcept = default;
		ObjectHandle& operator=(ObjectHandle&&) noexcept = default;

		/// Returns the base (first) object ID of the allocated range, or 0 if empty.
		uint32_t id() const noexcept { return _range ? _range->base : 0u; }

		/// Returns the base (first) object ID of the allocated range, or 0 if empty.
		operator uint32_t() const noexcept { return id(); }

		/// Returns true if this handle refers to a live allocation.
		explicit operator bool() const noexcept { return _range != nullptr; }

	private:

		friend class ObjectIdAllocator;

		/// Internal per-allocation state kept alive by the shared_ptr.
		struct Range
		{
			ObjectIdAllocator* allocator; ///< Back-pointer — must outlive the Range.
			uint32_t base;
			uint32_t count;

			~Range() noexcept { allocator->free(base, count); }
		};

		explicit ObjectHandle(std::shared_ptr<Range> r) noexcept
			: _range(std::move(r)) {}

		std::shared_ptr<Range> _range;
	};

	/**
	 * \brief Allocates a contiguous range of \a count object IDs.
	 *
	 * \param count  Number of consecutive IDs to allocate (default 1).
	 * \returns      An \c ObjectHandle whose implicit uint32_t value is the
	 *               base (first) ID of the range.
	 * \throws Exception if the 32-bit ID space is exhausted.
	 */
	[[nodiscard]] ObjectHandle allocate(uint32_t count = 1)
	{
		OVITO_ASSERT(count > 0);
		uint32_t base;

		if(count == 1) {
			// Fast path: reuse a previously freed single ID.
			if(!_freeSingles.empty()) {
				base = _freeSingles.back();
				_freeSingles.pop_back();
				return makeHandle(base, 1);
			}
		}
		else {
			// Search the range free-list for the first entry large enough.
			for(auto it = _freeRanges.begin(); it != _freeRanges.end(); ++it) {
				if(it->second >= count) {
					base = it->first;
					const uint32_t remaining = it->second - count;
					_freeRanges.erase(it);
					if(remaining == 1)
						_freeSingles.push_back(base + count);
					else if(remaining > 1)
						_freeRanges.emplace(base + count, remaining);
					return makeHandle(base, count);
				}
			}
		}

		// No suitable free range — bump the high-water mark.
		// Bit 31 is reserved: the renderer sets it on raytracer instance IDs as the
		// ExcludeFromOutline flag (see AnariScene/OSPRayScene) and unconditionally strips
		// it back off during object picking (ObjectPickingMap). A real ID must therefore
		// never occupy the top bit, so the usable range is limited to the lower 31 bits.
		if(count > MAX_OBJECT_ID + 1 - _nextId)
			throw Exception(QStringLiteral("Object ID space exhausted."));

		base = _nextId;
		_nextId += count;
		return makeHandle(base, count);
	}

private:

	/// Called by ObjectHandle::Range destructor to return IDs to the allocator.
	void free(uint32_t base, uint32_t count) noexcept
	{
		OVITO_ASSERT(count > 0);

		// Optimization: if this range is at the top of the high-water mark, just lower it.
		if(base + count == _nextId) {
			_nextId = base;
			// Also pull back any free ranges that are now above the new high-water mark
			// (they may have accumulated due to earlier partial frees at the top).
			while(!_freeRanges.empty()) {
				auto last = std::prev(_freeRanges.end());
				if(last->first >= _nextId) {
					_nextId = last->first; // already == last->first since ranges are non-overlapping
					_freeRanges.erase(last);
				}
				else {
					break;
				}
			}
			// Same for singles above the new high-water — they are rare but possible.
			// (Linear scan is acceptable since _freeSingles is typically small.)
			for(auto it = _freeSingles.begin(); it != _freeSingles.end(); ) {
				if(*it >= _nextId)
					it = _freeSingles.erase(it);
				else
					++it;
			}
			return;
		}

		if(count == 1) {
			// Try to merge into an adjacent range before falling back to the singles stack.
			// Check for an existing range ending exactly at base (predecessor).
			// map key = base of range, value = count, so predecessor ends at key+value.
			// We need a range whose base+count == base, i.e. base-of-range == base-count_of_range.
			// Use lower_bound to find the first entry >= base, then step back.
			auto it = _freeRanges.lower_bound(base);
			// Check predecessor range: does it end exactly at 'base'?
			if(it != _freeRanges.begin()) {
				auto pred = std::prev(it);
				if(pred->first + pred->second == base) {
					// Extend the predecessor range by 1.
					pred->second += 1;
					// Now try to merge with successor if it starts exactly where pred now ends.
					if(it != _freeRanges.end() && pred->first + pred->second == it->first) {
						pred->second += it->second;
						_freeRanges.erase(it);
					}
					return;
				}
			}
			// Check successor range: does it start exactly at base+1?
			if(it != _freeRanges.end() && it->first == base + 1) {
				// Replace successor entry with an extended range starting at base.
				uint32_t newCount = it->second + 1;
				_freeRanges.erase(it);
				_freeRanges.emplace(base, newCount);
				return;
			}
			// No adjacent range — push onto the singles stack.
			_freeSingles.push_back(base);
		}
		else {
			// Multi-ID range: insert into _freeRanges and coalesce with neighbours.
			auto [it, _inserted] = _freeRanges.emplace(base, count);

			// Merge with predecessor if it ends exactly at 'base'.
			if(it != _freeRanges.begin()) {
				auto pred = std::prev(it);
				if(pred->first + pred->second == it->first) {
					pred->second += it->second;
					it = _freeRanges.erase(it);
					it = std::prev(it); // point at pred
				}
			}

			// Merge with successor if it starts exactly where 'it' now ends.
			auto succ = std::next(it);
			if(succ != _freeRanges.end() && it->first + it->second == succ->first) {
				it->second += succ->second;
				_freeRanges.erase(succ);
			}
		}
	}

	ObjectHandle makeHandle(uint32_t base, uint32_t count)
	{
		return ObjectHandle(std::make_shared<ObjectHandle::Range>(this, base, count));
	}

	/// Stack of freed single IDs. O(1) push/pop. Used as the fast path for allocate(1).
	std::vector<uint32_t> _freeSingles;

	/// Free-list for multi-ID ranges, keyed by base ID. Kept coalesced.
	std::map<uint32_t, uint32_t> _freeRanges;

	/// Next never-used object ID (high-water mark). Starts at 1 (0 == no hit).
	uint32_t _nextId = 1;
};

}   // End of namespace
