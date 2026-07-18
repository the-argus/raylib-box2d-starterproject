#ifndef __UHANDER_ALLOCATOR_H__
#define __UHANDER_ALLOCATOR_H__

#include "macros.h"
#include "result.h"

#include <memory> // for construct_at
#include <span>
#include <type_traits>
#include <utility>

class Allocator;

using Bytes = std::span<u8>;

namespace alloc {
enum class Error : u8
{
    Success = 0,
    OOM,
    Unsupported,
    Usage,
    CouldntExpandInPlace,
    PlatformFailure,
};

inline constexpr u64 defaultAlign = alignof(u64) * 2;

struct Request
{
    u64 numBytes;
    u64 alignment = alloc::defaultAlign;
};

struct ReallocateRequest
{
    Bytes memory;
    // minimum size of the memory after reallocating. arraylist may set this to
    // current size + sizeof(T) when appending. Although it is not the optimal
    // size increase, it is the minimum needed to continue without an error.
    u64 newSizeBytes;
    // the optimal new size after reallocation. for an arraylist this would be
    // current size * growth_factor. ignored if shrinking or if zero
    u64 preferredSizeBytes = 0;
    u64 alignment = alloc::defaultAlign;
    bool inPlaceOrElseFail = false;

    [[nodiscard]] constexpr bool is_valid() const NOEXCEPT
    {
        return !memory.empty() &&
               // no attempt to... free the memory?
               (newSizeBytes != 0) &&
               // preferred should be zero OR ( (we're growing OR staying the
               // same size) + preferred is greater than required )
               (preferredSizeBytes == 0 || (newSizeBytes >= memory.size() &&
                                            preferredSizeBytes > newSizeBytes));
    }

    [[nodiscard]] constexpr size_t calculatePreferredSize() const noexcept
    {
        return preferredSizeBytes == 0 ? newSizeBytes : preferredSizeBytes;
    }
};
} // namespace alloc

class Allocator
{
  public:
    [[nodiscard]] constexpr Result<Bytes, alloc::Error>
    allocate(const alloc::Request &request) NOEXCEPT
    {
        // one way for request to be invalid
        if (request.numBytes == 0) [[unlikely]] {
            uassert(false, "Attempt to allocate 0 bytes from allocator.");
            return alloc::Error::Usage;
        }
        return impl_allocate(request);
    }

    constexpr void deallocate(void *memory, size_t size_hint = 0) NOEXCEPT
    {
        if (memory) [[likely]]
            impl_deallocate(memory, size_hint);
    }

    [[nodiscard]] constexpr Result<Bytes, alloc::Error>
    reallocate(const alloc::ReallocateRequest &options) NOEXCEPT
    {
        if (!options.is_valid()) [[unlikely]] {
            uassert(false, "invalid ReallocateRequest");
            return alloc::Error::Usage;
        }
        return impl_reallocate(options);
    }

    struct RestorePoint
    {
        friend class Allocator;

        RestorePoint(const RestorePoint &) = delete;
        RestorePoint &operator=(const RestorePoint &) = delete;
        RestorePoint(RestorePoint &&) = delete;
        RestorePoint &operator==(RestorePoint &&) = delete;

        constexpr ~RestorePoint()
        {
            m_allocator.impl_arenaRestoreScope(m_handle);
        }

        RestorePoint() = delete;

      private:
        constexpr RestorePoint(Allocator &allocator)
            : m_allocator(allocator), m_handle(allocator.impl_arenaNewScope())
        {
        }

        void *m_handle;
        Allocator &m_allocator;
    };

    template <typename T>
        requires std::is_invocable_r_v<void, T> and
                 std::is_move_constructible_v<T> and
                 std::is_trivially_destructible_v<T>
    constexpr void pushDestructor(T &&destructorCallableObject) NOEXCEPT
    {
        struct Destructor : public DestructorBase
        {
            T callable;

            Destructor(T &&callableParam) : callable(std::move(callableParam))
            {
                this->destructorFunction = [](DestructorBase &self) {
                    static_cast<Destructor *>(&self)->callable();
                };
            }
        };

        Result maybeDestructor =
            this->make<Destructor>(std::move(destructorCallableObject));
        if (not maybeDestructor) [[unlikely]]
            return maybeDestructor.error();
        this->impl_arenaPushDestructor(maybeDestructor.unwrap());
    }

    [[nodiscard]] constexpr RestorePoint beginScope() NOEXCEPT
    {
        return RestorePoint(*this);
    }

    template <typename T, typename... Args>
        requires(std::is_constructible_v<T, Args...> and
                 std::is_trivially_destructible_v<T>)
    [[nodiscard]] constexpr Result<T &, alloc::Error>
    make(Args &&...args) NOEXCEPT
    {
        auto allocationResult = allocate(alloc::Request{
            .numBytes = sizeof(T),
            .alignment = alignof(T),
        });
        if (allocationResult.isError()) [[unlikely]]
            return allocationResult.error();

        u8 *objectStart = allocationResult.value().data();

        uassert(u64(objectStart) % alignof(T) == 0,
                "Misaligned memory produced by allocator");

        T *made = std::launder(reinterpret_cast<T *>(objectStart));

        std::construct_at(made, std::forward<Args>(args)...);

        return *made;
    }

  protected:
    struct DestructorBase
    {
        DestructorBase *prev;
        void (*destructorFunction)(DestructorBase &self);
    };

    constexpr virtual void
    impl_arenaPushDestructor(DestructorBase &entry) NOEXCEPT {};

    [[nodiscard]] constexpr virtual Result<Bytes, alloc::Error>
    impl_allocate(const alloc::Request &) NOEXCEPT = 0;

    [[nodiscard]] constexpr virtual void *impl_arenaNewScope() NOEXCEPT
    {
        return nullptr;
    };

    constexpr virtual void impl_arenaRestoreScope(void *handle) NOEXCEPT {}

    constexpr virtual void impl_deallocate(void *memory,
                                           size_t size_hint) NOEXCEPT = 0;

    [[nodiscard]] constexpr virtual Result<Bytes, alloc::Error>
    impl_reallocate(const alloc::ReallocateRequest &options) NOEXCEPT = 0;
};

template <typename T>
concept AllocatorType = requires {
    requires std::is_base_of_v<Allocator, T>;
    requires std::is_convertible_v<T &, Allocator &>;
};

/// Basic allocator implementation for doing malloc() and free()
class CAllocator : public Allocator
{
  public:
    CAllocator() NOEXCEPT = default;
    CAllocator(Allocator *) NOEXCEPT;

    CAllocator(CAllocator &&other) = delete;
    CAllocator &operator=(CAllocator &&other) = delete;

    CAllocator &operator=(const CAllocator &) = delete;
    CAllocator(const CAllocator &) = delete;

  protected:
    [[nodiscard]] constexpr Result<Bytes, alloc::Error>
    impl_allocate(const alloc::Request &request) NOEXCEPT final
    {
        uassert(
            request.alignment <= 16,
            "Alignment requested larger than malloc, TODO allow aligning more");
        if (request.alignment > 16) {
            return alloc::Error::Unsupported;
        }
        void *allocated = ::malloc(request.numBytes);
        if (not allocated) {
            return alloc::Error::OOM;
        }
        return Bytes(static_cast<u8 *>(allocated), request.numBytes);
    }

    constexpr void impl_deallocate(void *memory,
                                   size_t size_hint) NOEXCEPT final
    {
        ::free(memory);
    }

    constexpr Result<Bytes, alloc::Error>
    impl_reallocate(const alloc::ReallocateRequest &options) NOEXCEPT final
    {
        if (not options.is_valid()) [[unlikely]]
            return alloc::Error::Usage;
        uassert(options.alignment <= 16, "alignment too big for realloc");
        uassert(!options.inPlaceOrElseFail,
                "realloc doesn't support inPlaceOrElseFail");
        if (options.inPlaceOrElseFail || options.alignment > 16) [[unlikely]]
            return alloc::Error::Unsupported;

        void *result =
            ::realloc(options.memory.data(), options.calculatePreferredSize());
        if (!result) [[unlikely]]
            return alloc::Error::OOM;
        return Bytes(static_cast<u8 *>(result),
                     options.calculatePreferredSize());
    }
};

#endif
