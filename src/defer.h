#ifndef __UHANDERS_DEFER_H__
#define __UHANDERS_DEFER_H__

#include <type_traits> // is_invocable_v is_void_v
#include <utility>     // forward

template <typename Callable> class defer
{
    Callable statement;
    bool shouldCall = true;

    static_assert(std::is_invocable_v<Callable>,
                  "Cannot call the given lambda with no arguments.");
    static_assert(std::is_void_v<decltype(std::declval<Callable>()())>,
                  "Cannot return a value from a deferred block.");

  public:
    constexpr defer(Callable &&f) : statement(std::forward<Callable>(f)) {}

    // you cannot move or copy or really mess with a Defer at all
    defer &operator=(const defer &) = delete;
    defer(const defer &) = delete;
    defer &operator=(defer &&) = delete;
    defer(defer &&) = delete;

    constexpr void cancel() { shouldCall = false; }

    constexpr ~defer()
    {
        if (shouldCall)
            statement();
    }
};
#endif
