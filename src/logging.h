#ifndef __UHANDERS_LOGGING_H__
#define __UHANDERS_LOGGING_H__

#include "macros.h"

#include <fmt/core.h>

#include <cstdarg>
#include <utility>

// NOTE: these levels match up with the raylib log levels, there are
// static_asserts for this in logging.cpp
enum class LogLevel
{
    Debug = 2, // Debug logging, used for internal debugging, it should be
               // disabled on release builds
    Info,      // Info logging, used for program execution info
    Warn,      // Warning logging, used on recoverable failures
    Error,     // Error logging, used on unrecoverable failures
    Fatal      // Fatal logging, used to abort program: exit(EXIT_FAILURE)
};

enum class LoggingCategory
{
    Renderer,
    Physics,
    Gameplay,
    Assets,
    Pool,
    Hotreload,
    Serialization,
    Raylib,
};

namespace detail {
void printLevelPrefix(LogLevel level) NOEXCEPT;
void printCategoryPrefix(LoggingCategory category) NOEXCEPT;
void printCurrentTimePrefix() NOEXCEPT;

/// Like logging normally but doesn't require the user to not put format
/// specifiers in the string. normal LOGWARN(...) macros will have weird errors
/// if you put unecessary curly braces in the format string, because it expects
/// those to be for arguments. so if you do LOGWARN_MSG it will invoke this
/// function instead and just print the string, normal style, and you don't
/// have to deal with that. the downside being that you can't add format
/// arugments / format anything
template <usize N>
inline void logMsgImpl(LogLevel level, LoggingCategory category,
                       const char (&constantString)[N]) NOEXCEPT
{
    printCurrentTimePrefix();
    printLevelPrefix(level);
    printCategoryPrefix(category);
    // can't make a format string from the constantString which is now only
    // considered runtime-known, so print it as an arg. and this fixes the
    // problem of not being able to put curly braces in the message
    fmt::println("{}", fmt::string_view(constantString, N - 1));
}

template <typename... Args>
inline void logImpl(LogLevel level, LoggingCategory category,
                    fmt::format_string<Args...> formatString,
                    Args &&...args) NOEXCEPT
{
    printCurrentTimePrefix();
    printLevelPrefix(level);
    printCategoryPrefix(category);
    fmt::println(std::forward<fmt::format_string<Args...>>(formatString),
                 std::forward<Args>(args)...);
}

// TODO: probably have log levels
#ifndef NO_LOGGING
#define LOG(level, category, format, ...)                                  \
    ::detail::logImpl(LogLevel::#level, LoggingCategory::category, format, \
                      __VA_ARGS__)
#define LOGINFO(category, format, ...)                                   \
    ::detail::logImpl(LogLevel::Info, LoggingCategory::category, format, \
                      __VA_ARGS__)
#define LOGWARN(category, format, ...)                                   \
    ::detail::logImpl(LogLevel::Warn, LoggingCategory::category, format, \
                      __VA_ARGS__)
#define LOGERROR(category, format, ...)                                   \
    ::detail::logImpl(LogLevel::Error, LoggingCategory::category, format, \
                      __VA_ARGS__)
#define LOGFATAL(category, format, ...)                                   \
    ::detail::logImpl(LogLevel::Fatal, LoggingCategory::category, format, \
                      __VA_ARGS__)

#define LOG_MSG(level, category, format) \
    ::detail::logMsgImpl(LogLevel::#level, LoggingCategory::category, format)
#define LOGINFO_MSG(category, format) \
    ::detail::logMsgImpl(LogLevel::Info, LoggingCategory::category, format)
#define LOGWARN_MSG(category, format) \
    ::detail::logMsgImpl(LogLevel::Warn, LoggingCategory::category, format)
#define LOGERROR_MSG(category, format) \
    ::detail::logMsgImpl(LogLevel::Error, LoggingCategory::category, format)
#define LOGFATAL_MSG(category, format) \
    ::detail::logMsgImpl(LogLevel::Fatal, LoggingCategory::category, format)
#else
#define LOG(level, category, format, ...)
#define LOGINFO(category, format, ...)
#define LOGWARN(category, format, ...)
#define LOGERROR(category, format, ...)
#define LOGFATAL(category, format, ...)

#define LOG_MSG(level, category, format)
#define LOGINFO_MSG(category, format)
#define LOGWARN_MSG(category, format)
#define LOGERROR_MSG(category, format)
#define LOGFATAL_MSG(category, format)
#endif

// used by raylib
void logCallback(int logLevel, const char *text, va_list args);

} // namespace detail

#endif
