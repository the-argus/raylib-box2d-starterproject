#include "arena.h"

#include <algorithm> // for std::max
#ifdef NDEBUG
#include <bit> // for std::bit_cast
#endif
#include <cstring>
#include <memory> // for std::align

#ifndef NDEBUG
#include <cstdlib> // for aligned_alloc + free
#endif

Result<Bytes, alloc::Error>
Arena::impl_reallocate(const alloc::ReallocateRequest &options) NOEXCEPT
{
    if (options.inPlaceOrElseFail) [[unlikely]]
        return alloc::Error::CouldntExpandInPlace;

    // just allocate a new block with the new size and do a copy
    Result allocation = this->allocate(alloc::Request{
        .numBytes = options.calculatePreferredSize(),
        .alignment = options.alignment,
    });

    if (!allocation.isSuccess()) [[unlikely]]
        return allocation;

    Bytes newmem = allocation.value();

    std::ignore = ::memcpy(newmem.data(), options.memory.data(),
                           std::min(newmem.size(), options.memory.size()));

    // freeing the old allocation is not possible with arena
    return newmem;
}

Arena::Arena(Bytes staticBuffer) NOEXCEPT : m_memory(staticBuffer),
                                            m_firstAvailableByteIndex(0)
{
}

Arena::Arena(Allocator *backing_allocator) NOEXCEPT
    : m_memory({}),
      m_firstAvailableByteIndex(0),
      m_backing(backing_allocator)
{
}

void Arena::destroy() NOEXCEPT
{
    callAllDestructors(DestructorListClearMode::ClearAll);
#ifndef NDEBUG
    freeAllocationsAfter(nullptr);
#endif
    if (m_backing && !m_memory.empty()) {
        m_backing->deallocate(m_memory.data());
    }
}

void Arena::callAllDestructors(DestructorListClearMode mode) NOEXCEPT
{
    DestructorBase *node = m_lastPushedDestructor;
    while (node) {
        DestructorBase &noderef = *node;

        if (mode == DestructorListClearMode::StopAfterCurrentScope &&
            noderef.destructorFunction == nullptr) [[unlikely]] {
            m_lastPushedDestructor = noderef.prev;
            return;
        }

        noderef.destructorFunction(noderef);
        node = noderef.prev;
    }
    m_lastPushedDestructor = nullptr;
}

#ifndef NDEBUG
void Arena::freeAllocationsAfter(DebugAllocationListNode *stop) NOEXCEPT
{
    while (m_debugAllocations != stop) {
        uassert(m_debugAllocations != nullptr);
        DebugAllocationListNode *next = m_debugAllocations->next;
        ::free(m_debugAllocations->memory);
        ::free(m_debugAllocations);
        m_debugAllocations = next;
    }
}
#endif

[[nodiscard]] Result<Bytes, alloc::Error>
Arena::impl_allocate(const alloc::Request &request) NOEXCEPT
{
#ifndef NDEBUG
    auto *node = static_cast<DebugAllocationListNode *>(
        ::malloc(sizeof(DebugAllocationListNode)));
    if (!node) [[unlikely]]
        return alloc::Error::OOM;

    const size_t align = request.alignment < alignof(std::max_align_t)
                             ? alignof(std::max_align_t)
                             : request.alignment;
    // aligned_alloc requires size be a multiple of alignment.
    const size_t size = (request.numBytes + align - 1) / align * align;

    void *mem = ::aligned_alloc(align, size);
    if (not mem) [[unlikely]] {
        ::free(node);
        return alloc::Error::OOM;
    }

    uassert(reinterpret_cast<uintptr_t>(mem) % request.alignment == 0);
    ::memset(mem, 0, request.numBytes);

    node->memory = mem;
    node->next = m_debugAllocations;
    m_debugAllocations = node;

    return Bytes(static_cast<u8 *>(mem), request.numBytes);
#else
    using namespace alloc;
    constexpr auto extraBookkeepingBytes = 100;
    // handle first-time allocation case
    if (m_memory.empty()) [[unlikely]] {
        if (!m_backing) [[unlikely]]
            return alloc::Error::OOM;

        uassert(m_firstAvailableByteIndex == 0);

        Allocator &backing = *m_backing;
        const alloc::Request backing_request{
            .numBytes = request.numBytes + extraBookkeepingBytes,
            .alignment = request.alignment,
        };

        Result result = backing.allocate(backing_request);
        if (isSuccess(result)) [[likely]] {
            m_memory = result.value();
            m_firstAvailableByteIndex = 0;
        } else [[unlikely]] {
            return result;
        }
    }

    // handle needs reallocation case
    const auto alignOrReallocInPlace = [&]() -> Result<u8 *, alloc::Error> {
        const auto getAlignedStart = [this] {
            // NOTE: this might return a pointer off the end of the memory,
            // but if so then get_space_remaining() should return 0
            return m_memory.data() + m_firstAvailableByteIndex;
        };
        const auto getSpaceRemaining = [this] {
            uassert(m_firstAvailableByteIndex <= m_memory.size());
            return m_memory.size() - m_firstAvailableByteIndex;
        };

        void *alignedStartVoidptr = getAlignedStart();
        size_t spaceRemainingAfterAlignment = getSpaceRemaining();

        if (!std::align(request.alignment, request.numBytes,
                        alignedStartVoidptr, spaceRemainingAfterAlignment)) {

            if (!m_backing) [[unlikely]]
                return alloc::Error::OOM;

            auto &backing = *m_backing;

            constexpr auto growth_factor = 2;

            auto maybeNewMemory = backing.reallocate(ReallocateRequest{
                .memory = m_memory,
                .newSizeBytes = std::max(
                    static_cast<u64>(m_memory.size() * growth_factor),
                    static_cast<u64>(m_memory.size() + request.numBytes +
                                     extraBookkeepingBytes)),
                .inPlaceOrElseFail = true,
            });

            if (!isSuccess(maybeNewMemory)) [[unlikely]]
                return maybeNewMemory.error();

            uassert(maybeNewMemory.value().data() == m_memory.data(), "");
            uassert(alignedStartVoidptr == getAlignedStart());
            uassert(maybeNewMemory.value().size() > m_memory.size());
            uassert(m_firstAvailableByteIndex <= m_memory.size());

            m_memory = maybeNewMemory.value();
            spaceRemainingAfterAlignment = getSpaceRemaining();

            uassert(m_firstAvailableByteIndex < m_memory.size());

            // re-align
#ifndef ASSERTS_DISABLED
            const bool align_succeeded =
#endif
                std::align(request.alignment, request.numBytes,
                           alignedStartVoidptr, spaceRemainingAfterAlignment);
            uassert(align_succeeded);
        }

        u8 *const alignedStart = static_cast<u8 *>(alignedStartVoidptr);

        return alignedStart;
    };

    const auto maybeAlignedStart = alignOrReallocInPlace();

    if (!isSuccess(maybeAlignedStart)) [[unlikely]]
        return maybeAlignedStart.error();

    u8 *const alignedStart = maybeAlignedStart.value();

    u8 *const newAvailableStart = alignedStart + request.numBytes;

    uassert(m_firstAvailableByteIndex < m_memory.size());

    m_firstAvailableByteIndex = newAvailableStart - m_memory.data();

    // its okay for m_first_available_byte_index to be *equal* to memory.size()
    // here, that means things are full
    uassert(m_firstAvailableByteIndex <= m_memory.size());

    ::memset(alignedStart, 0, request.numBytes);
    return Bytes(alignedStart, request.numBytes);
#endif
}

[[nodiscard]] void *Arena::impl_arenaNewScope() NOEXCEPT
{
    // a null destructor indicates a change in scope
    Result destructor = this->make<DestructorBase>();
    // NOTE: if we have no space for a destructor, then restoring the scope
    // won't work. That's fine because we will OOM after this anyways
    if (isSuccess(destructor)) [[likely]]
        impl_arenaPushDestructor(destructor.value());

#ifndef NDEBUG
    return static_cast<void *>(m_debugAllocations);
#else
    return std::bit_cast<void *>(m_firstAvailableByteIndex);
#endif
}

void Arena::impl_arenaRestoreScope(void *handle) NOEXCEPT
{
    callAllDestructors(DestructorListClearMode::StopAfterCurrentScope);
#ifndef NDEBUG
    freeAllocationsAfter(static_cast<DebugAllocationListNode *>(handle));
#else
    uassert(m_firstAvailableByteIndex < m_memory.size());
    m_firstAvailableByteIndex = std::bit_cast<usize>(handle);
    uassert(m_firstAvailableByteIndex < m_memory.size());
#endif
}

void Arena::impl_arenaPushDestructor(DestructorBase &destructor) NOEXCEPT
{
    destructor.prev = m_lastPushedDestructor;
    m_lastPushedDestructor = std::addressof(destructor);
}

void Arena::clear() NOEXCEPT
{
    callAllDestructors(DestructorListClearMode::ClearAll);
#ifndef NDEBUG
    freeAllocationsAfter(nullptr);
#else
    memset(m_memory.data(), 0x69, m_memory.size_bytes());
    // memfill(m_memory, 0x69);
#endif
    m_firstAvailableByteIndex = 0;
}
