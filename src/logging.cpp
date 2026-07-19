#include "logging.h"
#include "macros.h"

#include <raylib.h>

#include <fmt/chrono.h>
#include <fmt/color.h>
#include <fmt/core.h>

#include <array>
#include <chrono>

static_assert(static_cast<int>(LogLevel::Debug) == LOG_DEBUG);
static_assert(static_cast<int>(LogLevel::Info) == LOG_INFO);
static_assert(static_cast<int>(LogLevel::Warn) == LOG_WARNING);
static_assert(static_cast<int>(LogLevel::Error) == LOG_ERROR);
static_assert(static_cast<int>(LogLevel::Fatal) == LOG_FATAL);

static const char *loggingCategoryToString(LoggingCategory category) NOEXCEPT
{
    switch (category) {
    case LoggingCategory::Hotreload:
        return "Hotreload";
    case LoggingCategory::Physics:
        return "Physics";
    case LoggingCategory::Gameplay:
        return "Physics";
    case LoggingCategory::Renderer:
        return "Renderer";
    case LoggingCategory::Serialization:
        return "Serialization";
    case LoggingCategory::Raylib:
        return "Raylib";
    case LoggingCategory::Assets:
        return "Assets";
    }
    return "Unknown Category";
}

namespace detail {
void printLevelPrefix(LogLevel level) NOEXCEPT
{
    using enum LogLevel;
    switch (level) {
    case Debug: {
        const char *str = "[DEBUG] ";
        fmt::print(fmt::emphasis::bold | fmt::fg(fmt::color::lime), "{}", str);
    } break;
    case Info: {
        const char *str = "[INFO-] ";
        fmt::print(fmt::emphasis::bold | fmt::fg(fmt::color::white), "{}", str);
    } break;
    case Warn: {
        const char *str = "[WARN-] ";
        fmt::print(fmt::emphasis::bold | fmt::fg(fmt::color::orange), "{}",
                   str);
    } break;
    case Error: {
        const char *str = "[ERROR] ";
        fmt::print(fmt::emphasis::bold | fmt::fg(fmt::color::red), "{}", str);
    } break;
    case Fatal: {
        const char *str = "[FATAL] ";
        fmt::print(fmt::emphasis::bold | fmt::fg(fmt::color::red), "{}", str);
    } break;
    default: {
        fmt::println("[UNKNOWN] ");
    } break;
    }
}

void printCategoryPrefix(LoggingCategory category) NOEXCEPT
{
    fmt::print("[{}] ", loggingCategoryToString(category));
}

void printCurrentTimePrefix() NOEXCEPT
{
    // time is always UTC
    const auto currentTime = std::chrono::system_clock::now();
    fmt::print("[{:%H:%M:%S}] ", currentTime);
}

void logCallback(int logLevel, const char *text, va_list args)
{
    uassert(logLevel != LOG_NONE);
    uassert(logLevel != LOG_ALL);

    printCurrentTimePrefix();
    printLevelPrefix(static_cast<LogLevel>(logLevel));
    printCategoryPrefix(LoggingCategory::Raylib);

    std::array<char, 1024> buf{}; // filled with zeroes
    std::ignore = std::vsnprintf(buf.data(), buf.size(), text, args);
    buf.back() = '\0'; // jusssst in case
    fmt::print("{}\n", buf.data());
}
} // namespace detail
