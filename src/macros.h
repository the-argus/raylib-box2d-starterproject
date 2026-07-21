#ifndef __UHANDERS_MACROS_H__
#define __UHANDERS_MACROS_H__

#include <cassert>
#include <cstdint>
#include <cstdio>  // fprintf
#include <cstdlib> // abort

#define uabort(msg)                                                         \
    do {                                                                    \
        (void)::fprintf(stderr,                                                   \
                  "Aborted program: (%s). Occurred at file %s, line %d.\n", \
                  __FILE__, __LINE__);                                      \
        ::abort();                                                          \
    }

#define _ON_OR_TWO_ARG_DISPATCH(_1, _2, NAME, ...) NAME

#if defined(ASSERTS_DISABLED)
#define _UASSERT_CONDITION_ONLY(condition)
#define _UASSERT_CONDITION_AND_MESSAGE(condition, message)
#else
#define _UASSERT_CONDITION_ONLY(condition)                                   \
    do {                                                                     \
        if (!(condition)) {                                                  \
            (void)::fprintf(stderr, "Assertion failed: (%s), file %s, line %d.\n", \
                      #condition, __FILE__, __LINE__);                       \
            ::abort();                                                       \
        }                                                                    \
    } while (0)

#define _UASSERT_CONDITION_AND_MESSAGE(condition, message)                \
    do {                                                                  \
        if (!(condition)) {                                               \
            (void)::fprintf(                                                    \
                stderr,                                                   \
                "Assertion failed: (%s), file %s, line %d, reason: %s\n", \
                #condition, __FILE__, __LINE__, message);                 \
            ::abort();                                                    \
        }                                                                 \
    } while (0)
#endif

#define uassert(...)                                                     \
    _ON_OR_TWO_ARG_DISPATCH(__VA_ARGS__, _UASSERT_CONDITION_AND_MESSAGE, \
                            _UASSERT_CONDITION_ONLY)(__VA_ARGS__)

// could be disabled, like for testing and wanting to throw
#define NOEXCEPT noexcept

using u64 = uint64_t;
using u32 = uint32_t;
using u16 = uint16_t;
using u8 = uint8_t;
using i64 = int64_t;
using i32 = int32_t;
using i16 = int16_t;
using i8 = int8_t;

// only for emscripten really where specifically integer needs to match pointer
// length
using usize = size_t;

using f32 = float;
using f64 = double;

static_assert(sizeof(f32) == 4);
static_assert(sizeof(f64) == 8);

#ifndef HOTRELOAD_LIB_PATH
#define HOTRELOAD_LIB_PATH "(HOTRELOAD DISABLED)"
#define NO_HOTRELOAD
#endif

#if defined(_WIN32) && !defined(NO_HOTRELOAD)
#define HOTRELOAD_EXPORT __declspec(dllexport)
#else
#define HOTRELOAD_EXPORT
#endif

#endif
