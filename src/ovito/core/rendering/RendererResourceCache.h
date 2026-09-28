// SPDX-FileCopyrightText: 2026 OVITO GmbH, Germany
// SPDX-License-Identifier: GPL-3.0-only OR MIT

#pragma once

#include <ovito/core/Core.h>

namespace Ovito {

/**
 * \brief A tagged (=strongly-typed) tuple, which can be used as key for the RendererResourceCache class.
 *
 * Hashing and equality are handled centrally by RendererResourceCache via its key hasher, which
 * recursively dispatches into nested containers (tuples, arrays, ranges, ...). No qHash overload
 * is needed here.
 */
template<typename TagType, typename... TupleFields>
struct RendererResourceKey : public std::tuple<TupleFields...>
{
    /// Inherit constructors of tuple type.
    using std::tuple<TupleFields...>::tuple;
};

namespace detail {

/// Detects whether T derives from std::tuple<Ts...> (covers std::tuple itself and RendererResourceKey).
template<typename... Ts>
auto rendererCacheTupleBase(const std::tuple<Ts...>*) -> std::tuple<Ts...>;
inline auto rendererCacheTupleBase(const void*) -> void;

template<typename T>
using rendererCacheTupleBaseT = decltype(rendererCacheTupleBase(static_cast<T*>(nullptr)));

template<typename T>
concept RendererCacheTupleLike = !std::is_same_v<rendererCacheTupleBaseT<T>, void>;

/// Detects whether T derives from std::array<U, N> (or is std::array<U, N> itself).
template<typename U, std::size_t N>
auto rendererCacheArrayBase(const std::array<U, N>*) -> std::array<U, N>;
inline auto rendererCacheArrayBase(const void*) -> void;

template<typename T>
using rendererCacheArrayBaseT = decltype(rendererCacheArrayBase(static_cast<T*>(nullptr)));

template<typename T>
concept RendererCacheArrayLike = !std::is_same_v<rendererCacheArrayBaseT<T>, void>;

/// Detects std::variant.
template<typename T> struct RendererCacheIsVariant : std::false_type {};
template<typename... Ts> struct RendererCacheIsVariant<std::variant<Ts...>> : std::true_type {};

/// Hasher functor for cache keys. Container-shaped types (tuple-derived, array-derived) are
/// recursively dispatched through this same functor — that way, awkwardly nested types such as
/// raw std::array (whose namespace doesn't bring an ADL-discoverable qHash into scope) work
/// without needing extension. Everything else delegates to Qt's qHash via ADL.
struct RendererCacheKeyHasher {
    template<typename T>
    std::size_t operator()(const T& v) const noexcept {
        // Bring Qt's global qHash overloads into scope so they participate in unqualified-name
        // lookup alongside ADL-discovered overloads from the Ovito namespace.
        using ::qHash;

        using D = std::remove_cvref_t<T>;
        if constexpr(RendererCacheTupleLike<D>) {
            using Base = rendererCacheTupleBaseT<D>;
            return hashTuple(static_cast<const Base&>(v),
                std::make_index_sequence<std::tuple_size_v<Base>>{});
        }
        else if constexpr(RendererCacheArrayLike<D>) {
            using Base = rendererCacheArrayBaseT<D>;
            std::size_t seed = 0;
            for(const auto& elem : static_cast<const Base&>(v))
                seed = qHashMulti(seed, (*this)(elem));
            return seed;
        }
        else if constexpr(RendererCacheIsVariant<D>::value) {
            // Mix the active alternative's index in so distinct alternatives that happen to
            // share a hash don't collide.
            const std::size_t indexHash = std::hash<std::size_t>{}(v.index());
            return qHashMulti(indexHash, std::visit(
                [this](const auto& alt) noexcept -> std::size_t { return (*this)(alt); },
                v));
        }
        else if constexpr(std::is_same_v<D, std::monostate>) {
            return 0;
        }
        else if constexpr(std::is_same_v<D, QSizeF>) {
            return qHashMulti(0, v.width(), v.height());
        }
        else if constexpr(std::is_same_v<D, QPointF>) {
            return qHashMulti(0, v.x(), v.y());
        }
        else if constexpr(std::ranges::range<D>) {
            // Generic ranges (std::vector, std::list, ...). Recurses through (*this) so that
            // element types Qt's qHash can't reach (e.g. raw std::tuple) still work.
            std::size_t seed = 0;
            for(const auto& elem : v)
                seed = qHashMulti(seed, (*this)(elem));
            return seed;
        }
        else if constexpr(std::is_default_constructible_v<std::hash<T>>) {
            // Use std::hash if available.
            return std::hash<T>{}(v);
        }
        else {
            return qHash(v);
        }
    }

private:
    template<typename Tuple, std::size_t... I>
    std::size_t hashTuple(const Tuple& t, std::index_sequence<I...>) const noexcept {
        std::size_t seed = 0;
        ((seed = qHashMulti(seed, (*this)(std::get<I>(t)))), ...);
        return seed;
    }
};

/// Hasher functor for the (Key type, Value type) bucket index.
struct RendererCacheTypePairHasher {
    std::size_t operator()(const std::pair<std::type_index, std::type_index>& p) const noexcept {
        return qHashMulti(0, p.first.hash_code(), p.second.hash_code());
    }
};

}   // namespace detail

/**
 * \brief A cache data structure that accepts keys with arbitrary type and which handles resource lifetime.
 *
 * All methods of the class are thread-safe.
 *
 * Entries are partitioned into buckets keyed on the (Key type, Value type) pair so that lookups within
 * a bucket can use hash-based search in O(1) average time instead of a linear scan.
 */
class RendererResourceCache : public std::enable_shared_from_this<RendererResourceCache>
{
    Q_DISABLE_COPY_MOVE(RendererResourceCache)

public:

    /// Data type used by the resource cache to refer to a frame being in flight.
    using ResourceFrameHandle = std::uint64_t;

    /// A frame being in flight.
    class ResourceFrame
    {
    public:
        /// Default constructor.
        ResourceFrame() = default;

        /// Destructor.
        ~ResourceFrame() {
            if(_cache)
                _cache->releaseResourceFrame(_frameNumber);
        }

        // A frame cannot be copied.
        ResourceFrame(const ResourceFrame& other) = delete;
        ResourceFrame& operator=(const ResourceFrame& other) = delete;

        // A frame can be moved.
        ResourceFrame(ResourceFrame&& rhs) noexcept : _cache(std::exchange(rhs._cache, {})), _frameNumber(std::exchange(rhs._frameNumber, 0)) { OVITO_ASSERT(!rhs._cache); }
        ResourceFrame& operator=(ResourceFrame&& rhs) noexcept {
            ResourceFrame(std::move(rhs)).swap(*this);
            return *this;
        }
        inline void swap(ResourceFrame& rhs) noexcept {
            _cache.swap(rhs._cache);
            std::swap(_frameNumber, rhs._frameNumber);
        }

        /// Returns a reference to the cached value for the given key.
        /// If the key is not yet in the cache, inserts a new entry and initializes it via the provided initializer.
        template<typename Value, typename Key, typename Initializer>
        const Value& lookup(Key&& key, Initializer&& initializer) const {
            OVITO_ASSERT(_cache && _frameNumber != 0);
            return _cache->lookup<Value>(std::forward<Key>(key), _frameNumber, std::forward<Initializer>(initializer));
        }

        /// Returns true if the frame is valid.
        inline operator bool() const noexcept { return (bool)_cache; }

    private:
        /// Constructor.
        ResourceFrame(std::shared_ptr<RendererResourceCache> cache, ResourceFrameHandle frameNumber) noexcept : _cache(std::move(cache)), _frameNumber(frameNumber) {}

        std::shared_ptr<RendererResourceCache> _cache;
        ResourceFrameHandle _frameNumber = 0;

        friend class RendererResourceCache;
    };

public:

    /// Constructor.
    RendererResourceCache() = default;

#ifdef OVITO_DEBUG
    /// Destructor.
    ~RendererResourceCache() {
        // The cache should be completely empty at the time it is destroyed.
        OVITO_ASSERT(_activeResourceFrames.empty());
        OVITO_ASSERT(_buckets.empty());
    }
#endif

    /// Returns a reference to the cached value for the given key.
    /// If the key is not yet in the cache, inserts a new entry and initializes it via the provided initializer.
    template<typename Value, typename Key, typename Initializer>
    const Value& lookup(Key&& key, ResourceFrameHandle resourceFrame, Initializer&& initializer) {
        using KeyT = std::remove_cvref_t<Key>;
        using BucketT = TypedBucket<KeyT, Value>;

        std::lock_guard lock(_mutex);
        OVITO_ASSERT(std::find(_activeResourceFrames.begin(), _activeResourceFrames.end(), resourceFrame) != _activeResourceFrames.end());

        BucketT& bucket = getOrCreateBucket<KeyT, Value>();

        // Look up the key in the bucket's hash map.
        if(auto it = bucket.entries.find(key); it != bucket.entries.end()) {
            Entry<Value>& entry = *it->second;
            // Keep track of the frame in which the requested resource was accessed most recently.
            if(std::ranges::find(entry.frames, resourceFrame) == entry.frames.end())
                entry.frames.push_back(resourceFrame);
            return entry.value;
        }

        // Create a new entry, then run the initializer.
        // The Entry is heap-allocated via unique_ptr, so the returned reference to `value` remains
        // valid even if nested initializer lookups cause this bucket's map to rehash.
        auto newEntry = std::make_unique<Entry<Value>>();
        newEntry->frames.push_back(resourceFrame);
        Entry<Value>* entryPtr = newEntry.get();
        bucket.entries.emplace(std::forward<Key>(key), std::move(newEntry));
        Value& v = entryPtr->value;

        // The initializer may perform nested cache lookups, that's why we are using a recursive mutex here.
        if constexpr(std::is_invocable_v<Initializer, Value&>)
            initializer(v);
        else
            std::apply(std::forward<Initializer>(initializer), v);
        return v;
    }

    /// Begins a new resource frame and returns an RAII handle to it.
    /// Resources inserted during the frame's lifetime are kept alive until the handle is destroyed.
    ResourceFrame acquireResourceFrame() {
        std::lock_guard lock(_mutex);

        // On the first frame, the cache should be empty.
        OVITO_ASSERT(!_activeResourceFrames.empty() || _buckets.empty());

        // Advance the counter; skip the value 0, which would collide with a default-constructed handle
        // (and trip the assertion in releaseResourceFrame()).
        do {
            ++_nextResourceFrame;
        }
        while(_nextResourceFrame == 0);

#ifdef OVITO_DEBUG
        _activeResourceFrames.push_back(_nextResourceFrame);
#endif
        return ResourceFrame(shared_from_this(), _nextResourceFrame);
    }

private:

    /// One cache entry: the value plus the list of in-flight frames currently referencing it.
    template<typename Value>
    struct Entry {
        Value value{};
        QVarLengthArray<ResourceFrameHandle, 6> frames;
    };

    /// Type-erased base for per-(Key,Value) buckets so they can be stored together in `_buckets`.
    struct BucketBase {
        virtual ~BucketBase() = default;
        virtual void releaseFrame(ResourceFrameHandle frame) = 0;
        virtual bool empty() const noexcept = 0;
    };

    /// Concrete bucket holding entries with a known Key and Value type.
    template<typename Key, typename Value>
    struct TypedBucket final : BucketBase {
        std::unordered_map<Key, std::unique_ptr<Entry<Value>>, detail::RendererCacheKeyHasher> entries;

        void releaseFrame(ResourceFrameHandle frame) override {
            for(auto it = entries.begin(); it != entries.end(); ) {
                auto& frames = it->second->frames;
                auto frameIter = std::find(frames.begin(), frames.end(), frame);
                if(frameIter != frames.end()) {
                    if(frames.size() != 1) {
                        // Other in-flight frames still reference this entry: just drop our handle.
                        *frameIter = frames.back();
                        frames.pop_back();
                        ++it;
                    }
                    else {
                        // This was the last frame referencing the entry: erase it.
                        it = entries.erase(it);
                    }
                }
                else {
                    ++it;
                }
            }
        }

        bool empty() const noexcept override { return entries.empty(); }
    };

    /// Returns the bucket for the given (Key, Value) type pair, creating it on demand.
    template<typename Key, typename Value>
    TypedBucket<Key, Value>& getOrCreateBucket() {
        const std::pair<std::type_index, std::type_index> bucketKey{typeid(Key), typeid(Value)};
        if(auto it = _buckets.find(bucketKey); it != _buckets.end())
            return static_cast<TypedBucket<Key, Value>&>(*it->second);

        auto newBucket = std::make_unique<TypedBucket<Key, Value>>();
        TypedBucket<Key, Value>* rawPtr = newBucket.get();
        _buckets.emplace(bucketKey, std::move(newBucket));
        return *rawPtr;
    }

    /// Ends the given frame and evicts any cache entries that are no longer referenced by any in-flight frame.
    void releaseResourceFrame(ResourceFrameHandle frame) {
        OVITO_ASSERT(frame > 0);
        std::lock_guard lock(_mutex);

#ifdef OVITO_DEBUG
        // Remove frame from the list of active frames.
        // There is no need to maintain the original list order.
        // We can move the last item into the erased list position.
        auto iter = std::find(_activeResourceFrames.begin(), _activeResourceFrames.end(), frame);
        OVITO_ASSERT(iter != _activeResourceFrames.end());
        *iter = _activeResourceFrames.back();
        _activeResourceFrames.pop_back();
#endif

        // Release the resources associated with the frame unless they are shared with another frame that is still in flight.
        for(auto bucketIt = _buckets.begin(); bucketIt != _buckets.end(); ) {
            bucketIt->second->releaseFrame(frame);
            if(bucketIt->second->empty())
                bucketIt = _buckets.erase(bucketIt);
            else
                ++bucketIt;
        }

        OVITO_ASSERT(!_activeResourceFrames.empty() || _buckets.empty());
    }

    /// Buckets of cache entries, partitioned by (Key type, Value type) for hash-based lookup.
    std::unordered_map<std::pair<std::type_index, std::type_index>,
                       std::unique_ptr<BucketBase>,
                       detail::RendererCacheTypePairHasher> _buckets;

    /// Mutex to protect the cache from concurrent accesses. Recursive because initializers may
    /// perform nested lookups on the same cache.
    std::recursive_mutex _mutex;

    /// Monotonically increasing counter used to generate unique frame handle values.
    ResourceFrameHandle _nextResourceFrame = 0;

#ifdef OVITO_DEBUG
    /// Frame handles currently in flight (i.e., resources may still be in use by the CPU or GPU).
    std::vector<ResourceFrameHandle> _activeResourceFrames;
#endif
};

}   // End of namespace
