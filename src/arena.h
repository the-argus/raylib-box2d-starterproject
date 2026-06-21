#ifndef __UHANDER_ARENA_H__
#define __UHANDER_ARENA_H__

#include "allocator.h"
#include "macros.h"

class Arena : public Allocator
{
  public:
    explicit Arena(Bytes static_buffer) NOEXCEPT;
    explicit Arena(Allocator &backing_allocator) NOEXCEPT;

    Arena(Arena &&other) = delete;
    Arena &operator=(Arena &&other) = delete;

    Arena &operator=(const Arena &) = delete;
    Arena(const Arena &) = delete;

    ~Arena() NOEXCEPT { destroy(); }

    void clear() NOEXCEPT;

  protected:
    [[nodiscard]] Result<Bytes, alloc::Error>
    impl_allocate(const alloc::Request &) NOEXCEPT final;

    [[nodiscard]] void *impl_arenaNewScope() NOEXCEPT override;
    void impl_arenaRestoreScope(void *handle) NOEXCEPT override;

    void impl_arenaPushDestructor(DestructorBase &entry) NOEXCEPT override;

    constexpr void impl_deallocate(void *memory,
                                   size_t size_hint) NOEXCEPT final
    {
        // deallocating with an arena is a no-op
        return;
    }

    Result<Bytes, alloc::Error>
    impl_reallocate(const alloc::ReallocateRequest &options) NOEXCEPT final;

  private:
    enum class DestructorListClearMode
    {
        ClearAll,
        StopAfterCurrentScope,
    };

    void destroy() NOEXCEPT;
    void callAllDestructors(DestructorListClearMode mode) NOEXCEPT;

    Bytes m_memory;
    u64 m_firstAvailableByteIndex;
    Allocator *m_backing = nullptr;
    DestructorBase *m_lastPushedDestructor = nullptr;
};

#endif
