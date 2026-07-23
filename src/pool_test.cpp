#include "pool.h"

#include <cstring>
#include <random>
#include <set>
#include <vector>

namespace {

struct Trivial
{
    u32 a;
    u32 b;
    f32 c;
};
static_assert(std::is_trivial_v<Trivial>);
using TrivialHandle = Pool<Trivial>::Handle;

struct Tracked
{
    int *destructorCounter = nullptr;
    int value = 0;

    Tracked() = default;
    explicit Tracked(int *counter, int v) : destructorCounter(counter), value(v)
    {
    }

    Tracked(Tracked &&other) noexcept
        : destructorCounter(std::exchange(other.destructorCounter, nullptr)),
          value(other.value)
    {
    }
    Tracked &operator=(Tracked &&other) noexcept
    {
        if (this != &other) {
            destructorCounter = std::exchange(other.destructorCounter, nullptr);
            value = other.value;
        }
        return *this;
    }

    Tracked(const Tracked &) = delete;
    Tracked &operator=(const Tracked &) = delete;

    ~Tracked()
    {
        if (destructorCounter != nullptr) {
            (*destructorCounter)++;
        }
    }
};
using TrackedHandle = Pool<Tracked>::Handle;

void testValidItem()
{
    auto &pool = *Pool<Trivial>::instance();

    auto handle = pool.make(Trivial{});
    uassert(bool(handle));
    uassert(pool.isValid(handle));

    Trivial *item = pool.get(handle);
    uassert(item != nullptr);

    u8 bytes[sizeof(Trivial)];
    ::memcpy(bytes, item, sizeof(Trivial));
    for (usize i = 0; i < sizeof(Trivial); ++i) {
        uassert(bytes[i] == 0);
    }

    TrivialHandle handleFromItem = pool.handleForItem(*item);
    uassert(handleFromItem == handle);
    pool.clear();
}

void testDestroy()
{
    int destructorCount = 0;
    auto &pool = *Pool<Tracked>::instance();

    auto handle = pool.make(&destructorCount, 42);
    uassert(pool.isValid(handle));
    uassert(destructorCount == 0);

    const bool destroyed = pool.destroy(handle);
    uassert(destroyed);
    uassert(destructorCount == 1);
    uassert(not pool.isValid(handle));
    uassert(pool.get(handle) == nullptr);

    const bool destroyedAgain = pool.destroy(handle);
    uassert(not destroyedAgain, "destroying twice should do nothing");
    uassert(destructorCount == 1);
    pool.clear();
}

void testBadArgsForHandleForItem()
{
    auto &pool = *Pool<Trivial>::instance();

    auto real = pool.make(Trivial{});
    uassert(pool.isValid(real));

    Trivial dummy{1, 2, 3.0f};
    auto handle = pool.handleForItem(dummy);
    uassert(not pool.isValid(handle), "handleForItem should return invalid "
                                      "handle for object outside the pool");
    pool.clear();
}

void testHandlesWhenDestroyingElements()
{
    int destructorCount = 0;

    auto &pool = *Pool<Tracked>::instance();

    auto first = pool.make(&destructorCount, 1);
    auto end = pool.make(&destructorCount, 2);
    uassert(pool.isValid(first) and pool.isValid(end));

    Tracked *endActual = pool.get(end);
    uassert(endActual != nullptr);

    const bool destroyed = pool.destroy(end);
    uassert(destroyed);

    // trying to get a handle to this deleted thing which is just off the end of
    // the buffer should give an invalid handle.
    auto invalidHandle = pool.handleForItem(*endActual);
    uassert(not pool.isValid(invalidHandle));

    auto end2 = pool.make(&destructorCount, 3);
    uassert(not pool.isValid(invalidHandle),
            "reviving the particular spot where an item was last removed "
            "should not revive handles to that item.");

    pool.destroy(first);
    uassert(not end);
    uassert(end2);
    pool.clear();
}

void testAllocateManySize()
{
    auto &pool = *Pool<Trivial>::instance();

    constexpr usize N = 5000;
    std::vector<TrivialHandle> handles;
    handles.reserve(N);
    for (usize i = 0; i < N; ++i) {
        auto h = pool.make(Trivial{static_cast<u32>(i), 0, 0.0f});
        uassert(h);
        handles.push_back(h);
    }

    uassert(pool.items().size() == N, "not all items pushed back?");
    pool.clear();
}

void testRandomCreateAndDestroy()
{
    auto &pool = *Pool<Trivial>::instance();
    auto &trackedPool = *Pool<Tracked>::instance();

    int destructorCount = 0;

    std::mt19937 rng(0xDEADBEEF);

    // maintain expected items and expected order in these
    std::vector<std::pair<int, TrackedHandle>> trackedLive;
    std::set<int> expected;

    int nextValue = 0;

    auto addSome = [&](int count) {
        for (int i = 0; i < count; ++i) {
            const int v = nextValue++;
            auto h = trackedPool.make(&destructorCount, v);
            uassert(h);
            trackedLive.emplace_back(v, h);
            expected.insert(v);
        }
    };

    auto removeSome = [&](int count) {
        for (int i = 0; i < count and not trackedLive.empty(); ++i) {
            std::uniform_int_distribution<usize> pick(0,
                                                      trackedLive.size() - 1);
            const usize idx = pick(rng);
            auto [v, h] = trackedLive[idx];
            uassert(trackedPool.isValid(h));
            uassert(trackedPool.destroy(h));
            uassert(not trackedPool.isValid(h));
            expected.erase(v);
            trackedLive[idx] = trackedLive.back();
            trackedLive.pop_back();
        }
    };

    auto verify = [&]() {
        auto span = trackedPool.items();
        uassert(span.size() == expected.size(),
                "items().size() must match expected set size");
        std::set<int> got;
        for (const Tracked &t : span) {
            const bool inserted = got.insert(t.value).second;
            uassert(inserted);
        }
        uassert(got == expected);
        for (auto &[v, h] : trackedLive) {
            uassert(trackedPool.isValid(h));
            const Tracked *t = trackedPool.get(h);
            uassert(t != nullptr);
            uassert(t->value == v);
        }
    };

    addSome(2000);
    verify();
    removeSome(1500);
    verify();
    addSome(3000);
    verify();

    removeSome(static_cast<int>(trackedLive.size()));
    verify();
    uassert(pool.items().size() == 0);
    uassert(expected.empty());
    pool.clear();
}

} // namespace

int main()
{
    testValidItem();
    fmt::println("testValidItem done");
    testDestroy();
    fmt::println("testDestroy done");
    testBadArgsForHandleForItem();
    fmt::println("testBadArgsForHandleForItem done");
    testHandlesWhenDestroyingElements();
    fmt::println("testHandlesWhenDestroyingElements done");
    testAllocateManySize();
    fmt::println("testAllocateManySize done");
    testRandomCreateAndDestroy();
    fmt::println("testRandomCreateAndDestroy done");

    return 0;
}
