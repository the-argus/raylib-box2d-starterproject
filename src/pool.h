#ifndef __GAME_POOL_H__
#define __GAME_POOL_H__

#include "allocator.h"
#include "logging.h"
#include "macros.h"

#ifndef NDEBUG
#include <cstring> // for filling with debug bytes
#endif

#include <limits>

template <typename T> struct PoolStats
{
    static constexpr usize initialPoolItems = 128;
    static constexpr f32 growFactor = 1.5f;

    PoolStats() = delete;
};

constexpr u32 invalidU32 = std::numeric_limits<u32>::max();

template <typename T> class Pool
{
    inline static Pool defaultGlobalInstance{cAllocator()};
    inline static Pool *globalInstance = &defaultGlobalInstance;

    struct MapEntry
    {
        u32 index;
        u32 generation;

        constexpr bool operator==(const MapEntry &) const = default;
    };

  public:
    class Handle
    {
      public:
        constexpr Handle() : m_entry(invalidU32, invalidU32) {}

        constexpr Handle(const Handle &) = default;
        constexpr Handle &operator=(const Handle &) = default;
        constexpr Handle(Handle &&) = default;
        constexpr Handle &operator=(Handle &&) = default;

        friend class Pool;

        T &operator*() const { return this->get(); }

        T *operator->() const { return std::addressof(this->get()); }

        explicit operator bool() const
        {
            Pool *instance = Pool::instance();
            uassert(instance);
            return instance->isValid(*this);
        }

        constexpr bool operator==(const Handle &) const = default;

      private:
        T &get() const
        {
            Pool *instance = Pool::instance();
            uassert(instance);
            T *item = instance->get(*this);
            uassert(item);
            return *item;
        }

        MapEntry m_entry;
        constexpr Handle(MapEntry entry) : m_entry(entry) {}
    };

    static Pool *instance() { return globalInstance; }
    static void setInstance(Pool *newValue) { globalInstance = newValue; }

    struct Range;
    friend struct Range;

    // the express purpose of a sentinel is to be an object which can be
    // compared to an iterator (a pointer to T that gets incremented) and then
    // say whether that pointer is 'done' ie. out of bounds / not looking at
    // items anymore
    struct Sentinel
    {
        [[nodiscard]] bool operator==(T *item)
        {
            auto *instance = Pool::instance();
            return item < instance->m_buffer or
                   item >= instance->m_buffer + instance->m_size;
        }
    };

    struct Range
    {
        Range() { Pool::instance()->m_numLiveIterators++; }
        ~Range() { destroy(); }

        Range(Range &&other) : m_owning(std::exchange(other.m_owning, false)) {}

        Range &operator=(Range &&other)
        {
            if (&other == this)
                return *this;
            destroy();
            m_owning = std::exchange(other.m_owning, false);
            return *this;
        }

        Range(const Range &) = delete;
        Range &operator=(const Range &) = delete;

        T *begin()
        {
            auto *instance = Pool::instance();
            return instance->m_buffer;
        }

        Sentinel end() { return {}; }

        /// the number of live items currently in the pool
        [[nodiscard]] u32 size() const { return Pool::instance()->m_size; }

      private:
        void destroy()
        {
            if (m_owning) {
                Pool::instance()->m_numLiveIterators--;
            }
        }

        bool m_owning = true;
        T *m_current = Pool::instance()->m_buffer;
    };

    Range items() { return Range{}; }

    [[nodiscard]] Handle handleForItem(const T &item)
    {
        const T *addr = std::addressof(item);
        if (addr >= m_buffer and addr < m_buffer + m_size) {
            const u32 dataIndex = static_cast<u32>(addr - m_buffer);
            const u32 entryIndex = m_reverseLookupBuffer[dataIndex];
            uassert(entryIndex < m_capacity);
            const MapEntry &entry = m_mappingBuffer[entryIndex];
            uassert(entry.generation % 2 == 1,
                    "expected live element, reverse lookup must be broken");
            uassert(entry.index == dataIndex,
                    "reverse lookup and mappingBuffer have fallen out of sync");
            return Handle{MapEntry{
                .index = entryIndex,
                .generation = entry.generation,
            }};
        }
        return {};
    }

    template <typename... ConstructorArgs>
        requires std::is_constructible_v<T, ConstructorArgs...>
    Handle make(ConstructorArgs &&...args)
    {
        if (m_buffer == nullptr) [[unlikely]] {
            uassert(m_mappingBuffer == nullptr and
                    m_reverseLookupBuffer == nullptr and m_capacity == 0);
            uassert(m_allocator);

            Result allocation = m_allocator->allocate(alloc::Request{
                .numBytes = sizeof(T) * PoolStats<T>::initialPoolItems,
                .alignment = alignof(T),
            });
            Result mappingAllocation = m_allocator->allocate(alloc::Request{
                .numBytes = sizeof(MapEntry) * PoolStats<T>::initialPoolItems,
                .alignment = alignof(MapEntry),
            });
            Result reverseAllocation = m_allocator->allocate(alloc::Request{
                .numBytes = sizeof(u32) * PoolStats<T>::initialPoolItems,
                .alignment = alignof(u32),
            });

            if (!mappingAllocation || !allocation || !reverseAllocation)
                return {};

            m_buffer = std::launder(reinterpret_cast<T *>(allocation->data()));
            m_mappingBuffer = std::launder(
                reinterpret_cast<MapEntry *>(mappingAllocation->data()));
            m_reverseLookupBuffer = std::launder(
                reinterpret_cast<u32 *>(reverseAllocation->data()));
            m_capacity = std::min(
                std::min(allocation->size_bytes() / sizeof(T),
                         mappingAllocation->size_bytes() / sizeof(MapEntry)),
                reverseAllocation->size_bytes() / sizeof(u32));
            m_size = 0;

#ifndef NDEBUG
            ::memset(allocation->data(), 0xCD, allocation->size_bytes());
            ::memset(mappingAllocation->data(), 0xCD,
                     mappingAllocation->size_bytes());
            ::memset(reverseAllocation->data(), 0xCD,
                     reverseAllocation->size_bytes());
#endif

            // initially, MapEntry indexes refer to where the next free spot is
            // in the mappingBuffer. When they are allocated, the meaning
            // changes to mean their index in the array itself
            for (u32 i = 0; i < m_capacity; ++i) {
                m_mappingBuffer[i] = MapEntry{.index = i + 1, .generation = 0};
            }
            m_firstFreeMapEntry = 0;

        } else if (m_size == m_capacity) {
            const auto currentSizeBytes = m_capacity * sizeof(T);
            const auto currentMappingSizeBytes = m_capacity * sizeof(MapEntry);
            const auto currentReverseSizeBytes = m_capacity * sizeof(u32);

            const alloc::ReallocateRequest reallocationParams{
                .memory =
                    Bytes(reinterpret_cast<u8 *>(m_buffer), currentSizeBytes),
                .newSizeBytes = currentSizeBytes + sizeof(T),
                .preferredSizeBytes =
                    static_cast<u64>(static_cast<f32>(m_capacity) *
                                     PoolStats<T>::growFactor) *
                    sizeof(T),
                .alignment = alignof(T),
            };
            const alloc::ReallocateRequest mappingBufferReallocationParams{
                .memory = Bytes(reinterpret_cast<u8 *>(m_mappingBuffer),
                                currentMappingSizeBytes),
                .newSizeBytes = currentMappingSizeBytes + sizeof(MapEntry),
                .preferredSizeBytes =
                    static_cast<u64>(static_cast<f32>(m_capacity) *
                                     PoolStats<T>::growFactor) *
                    sizeof(MapEntry),
                .alignment = alignof(MapEntry),
            };
            const alloc::ReallocateRequest reverseBufferReallocationParams{
                .memory = Bytes(reinterpret_cast<u8 *>(m_reverseLookupBuffer),
                                currentReverseSizeBytes),
                .newSizeBytes = currentReverseSizeBytes + sizeof(u32),
                .preferredSizeBytes =
                    static_cast<u64>(static_cast<f32>(m_capacity) *
                                     PoolStats<T>::growFactor) *
                    sizeof(u32),
                .alignment = alignof(u32),
            };

            Result reallocation = m_allocator->reallocate(reallocationParams);
            Result mappingBufferReallocation =
                m_allocator->reallocate(mappingBufferReallocationParams);
            Result reverseBufferReallocation =
                m_allocator->reallocate(reverseBufferReallocationParams);

            if (not reallocation or not mappingBufferReallocation or
                not reverseBufferReallocation)
                return {};

            const u32 newMemoryStartIndex = m_capacity;
            m_buffer =
                std::launder(reinterpret_cast<T *>(reallocation->data()));
            m_mappingBuffer = std::launder(reinterpret_cast<MapEntry *>(
                mappingBufferReallocation->data()));
            m_reverseLookupBuffer = std::launder(
                reinterpret_cast<u32 *>(reverseBufferReallocation->data()));
            m_capacity =
                std::min(std::min(reallocation->size_bytes() / sizeof(T),
                                  mappingBufferReallocation->size_bytes() /
                                      sizeof(MapEntry)),
                         reverseBufferReallocation->size_bytes() / sizeof(u32));
            uassert(m_capacity > newMemoryStartIndex);
#ifndef NDEBUG
            ::memset(m_buffer + m_size, 0xCD,
                     (m_capacity - m_size) * sizeof(T));
            ::memset(m_mappingBuffer + m_size, 0xCD,
                     (m_capacity - m_size) * sizeof(MapEntry));
            ::memset(m_reverseLookupBuffer + m_size, 0xCD,
                     (m_capacity - m_size) * sizeof(u32));
#endif
            for (u32 i = newMemoryStartIndex; i < m_capacity; ++i) {
                m_mappingBuffer[i] = MapEntry{.index = i + 1, .generation = 0};
            }
            m_firstFreeMapEntry = newMemoryStartIndex;
        }

        uassert(m_size < m_capacity);

        const u32 entryIndex = m_firstFreeMapEntry;
        MapEntry &entry = m_mappingBuffer[entryIndex];
        m_firstFreeMapEntry = entry.index;
        entry.generation++;
        if (entry.generation == invalidU32) [[unlikely]] {
            // insane, but integer overflow is about to happen. maybe the game
            // ran for a very long time and spawned a lot of things
            entry.generation = 1;
        }
        entry.index = m_size;
        m_reverseLookupBuffer[entry.index] = entryIndex;
        m_size++;
        T *actual = std::addressof(m_buffer[entry.index]);
        std::construct_at(actual, std::forward<ConstructorArgs>(args)...);

        uassert(entry.generation % 2 == 1,
                "odd generation is expected to indicate an alive element");

        return Handle{MapEntry{
            .index = entryIndex,
            .generation = entry.generation,
        }};
    }

    /// returns true if the object existed and was destroyed
    bool destroy(Handle item)
    {
        if (m_numLiveIterators > 0) {
            LOGERROR_MSG(
                Pool,
                "Tried to destroy a thing from a pool while iterating over the "
                "pool. Probably use ctx->doAtEndOfFrame([thing]{ "
                "destroy(thing); }) instead");
            return false;
        }
        if (not isValid(item)) {
            return false;
        }

        MapEntry &entry = m_mappingBuffer[item.m_entry.index];
        uassert(entry.index < m_size);
        T &actual = m_buffer[entry.index];

        // destroy and move the last item into the freed spot, and update its
        // handle
        actual.~T();
        entry.generation++;
        if (entry.generation == invalidU32) [[unlikely]] {
            // insane, but integer overflow is about to happen. maybe the game
            // ran for a very long time and spawned a lot of things
            entry.generation = 0;
        }

        uassert(m_size > 0);
        if (entry.index != m_size - 1) {
            const u32 lastDataIndex = m_size - 1;
            T &last = m_buffer[lastDataIndex];
            const u32 movedEntryIndex = m_reverseLookupBuffer[lastDataIndex];
            std::construct_at(std::addressof(actual), std::move(last));
            last.~T();
            m_mappingBuffer[movedEntryIndex].index = entry.index;
            m_reverseLookupBuffer[entry.index] = movedEntryIndex;
        }

        entry.index = m_firstFreeMapEntry;
        m_firstFreeMapEntry = item.m_entry.index;
        m_size--;

        uassert(entry.generation % 2 == 0,
                "even generation is expected to indicate a dead element");

        return true;
    }

    const T *get(Handle item) const
    {
        return isValid(item)
                   ? std::addressof(
                         m_buffer[m_mappingBuffer[item.m_entry.index].index])
                   : nullptr;
    }

    T *get(Handle item)
    {
        return isValid(item)
                   ? std::addressof(
                         m_buffer[m_mappingBuffer[item.m_entry.index].index])
                   : nullptr;
    }

    [[nodiscard]] bool isValid(Handle item) const
    {
        if (item.m_entry.index >= m_capacity) [[unlikely]]
            return false;
        MapEntry &entry = m_mappingBuffer[item.m_entry.index];
        const bool valid = entry.generation == item.m_entry.generation;
        uassert(not valid or entry.index < m_size);
        return valid;
    }

    [[nodiscard]] bool isNull(Handle item) const { return not isValid(item); }

    void clear()
    {
        if (m_buffer == nullptr) {
            uassert(m_size == 0);
            return;
        }

        for (u32 i = 0; i < m_capacity; ++i) {
            MapEntry &entry = m_mappingBuffer[i];
            if (entry.generation % 2 == 1) {
                uassert(entry.index < m_size);
                m_buffer[entry.index].~T();

                entry.generation++;
                if (entry.generation == invalidU32) [[unlikely]] {
                    entry.generation = 0;
                }
                uassert(entry.generation % 2 == 0);
            }
            m_mappingBuffer[i].index = i + 1;
        }

        m_firstFreeMapEntry = 0;
        m_size = 0;

#ifndef NDEBUG
        ::memset(m_buffer, 0xCD, m_capacity * sizeof(T));
        ::memset(m_reverseLookupBuffer, 0xCD, m_capacity * sizeof(u32));
#endif
    }

    Pool() = delete;

    explicit Pool(Allocator *allocator) : m_allocator(allocator) {}

  private:
    Allocator *m_allocator = cAllocator();
    T *m_buffer{};
    MapEntry *m_mappingBuffer{};
    u32 *m_reverseLookupBuffer{};
    u32 m_size{};
    u32 m_capacity{};
    u32 m_firstFreeMapEntry{};
    u32 m_numLiveIterators{};
};

/// Create an item in its corresponding pool. May return null on allocation
/// failure
template <typename T, typename... ConstructorArgs>
    requires std::is_constructible_v<T, ConstructorArgs...>
Pool<T>::Handle make(ConstructorArgs &&...args)
{
    return Pool<T>::instance()->make(std::forward<ConstructorArgs>(args)...);
}

/// Remove an item from its pool and call its destructor
template <typename T> void destroy(typename Pool<T>::Handle handle)
{
    return Pool<T>::instance()->destroy(handle);
}

/// Remove an item from its pool and call its destructor
template <typename T> void destroy(const T &item)
{
    auto *instance = Pool<T>::instance();
    instance->destroy(instance->handleForItem(item));
}

/// Get the handle for something from a reference to it. Returns null if the
/// thing passed as a reference is not actually a thing in the pool
template <typename T> Pool<T>::Handle handleForItem(const T &item)
{
    return Pool<T>::instance()->handleForItem(item);
}

#endif
