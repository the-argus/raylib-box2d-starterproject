#ifndef __GAME_POOL_H__
#define __GAME_POOL_H__

#include "allocator.h"
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

    std::span<T> items() { return std::span<T>{m_buffer, m_size}; }

    std::span<const T> items() const
    {
        return std::span<const T>{m_buffer, m_size};
    }

    /// Performs a linear search until it finds a handle pointing to the given
    /// element
    [[nodiscard]] Handle handleForItem(const T &item)
    {
        const T *addr = std::addressof(item);
        if (addr >= m_buffer and addr < m_buffer + m_size) {
            const u32 index = static_cast<u32>(addr - m_buffer);
            for (u32 i = 0; i < m_capacity; ++i) {
                const MapEntry &entry = m_mappingBuffer[i];
                if (entry.generation % 2 == 0)
                    continue; // dead element

                if (entry.index == index) {
                    return Handle{MapEntry{
                        .index = i,
                        .generation = entry.generation,
                    }};
                }
            }
            uassert(false, "some invariant of Pool broke and an element in the "
                           "valid region does not have a corresponding handle");
        }
        return {};
    }

    template <typename... ConstructorArgs>
        requires std::is_constructible_v<T, ConstructorArgs...>
    Handle make(ConstructorArgs &&...args)
    {
        if (m_buffer == nullptr) [[unlikely]] {
            uassert(m_mappingBuffer == nullptr and m_capacity == 0);
            uassert(m_allocator);

            Result allocation = m_allocator->allocate(alloc::Request{
                .alignment = alignof(T),
                .numBytes = sizeof(T) * PoolStats<T>::initialPoolItems,
            });
            Result mappingAllocation = m_allocator->allocate(alloc::Request{
                .alignment = alignof(MapEntry),
                .numBytes = sizeof(MapEntry) * PoolStats<T>::initialPoolItems,
            });

            if (!mappingAllocation || !allocation)
                return {};

            m_buffer = allocation->data();
            m_mappingBuffer = mappingAllocation->data();
            m_capacity =
                std::min(allocation->size_bytes() / sizeof(T),
                         mappingAllocation->size_bytes() / sizeof(MapEntry));
            m_size = 0;

#ifndef NDEBUG
            ::memset(allocation->data(), 0xCD, allocation->size_bytes());
            ::memset(mappingAllocation->data(), 0xCD,
                     mappingAllocation->size_bytes());
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

            Result reallocation = m_allocator->reallocate(reallocationParams);
            Result mappingBufferReallocation =
                m_allocator->reallocate(mappingBufferReallocationParams);

            if (not reallocation or not mappingBufferReallocation)
                return {};

            const u32 newMemoryStartIndex = m_capacity;
            m_buffer = reallocation->data();
            m_mappingBuffer = mappingBufferReallocation->data();
            m_capacity = std::min(reallocation->size_bytes() / sizeof(T),
                                  mappingBufferReallocation->size_bytes() /
                                      sizeof(MapEntry));
            uassert(m_capacity > newMemoryStartIndex);
#ifndef NDEBUG
            ::memset(m_buffer + m_size, 0xCD,
                     (m_capacity - m_size) * sizeof(T));
            ::memset(m_mappingBuffer + m_size, 0xCD,
                     (m_capacity - m_size) * sizeof(MapEntry));
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
            T &last = items()[m_size - 1];
            Handle handle = handleForItem(last);
            std::construct_at(std::addressof(actual), std::move(last));
            last.~T();
            m_mappingBuffer[handle.m_entry.index].index = entry.index;
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

    Pool() = delete;

    explicit Pool(Allocator *allocator) : m_allocator(allocator) {}

  private:
    Allocator *m_allocator = cAllocator();
    T *m_buffer{};
    MapEntry *m_mappingBuffer{};
    u32 m_size{};
    u32 m_capacity{};
    u32 m_firstFreeMapEntry{};
};

#endif
