#ifdef __EMSCRIPTEN__
#include <emscripten/emscripten.h>
#endif

#include "game_lib.h"
#include "logging.h"

#include <fmt/format.h>

#include <rlImGui.h>
#include <rlImGuiColors.h>

#include <imgui.h>
#include <imgui_impl_raylib.h>

#include <chrono>
#ifndef NO_HOTRELOAD
#include <filesystem>
#endif

using TimePoint = std::chrono::time_point<std::chrono::system_clock>;
using Duration =
    decltype(std::declval<TimePoint>() - std::declval<TimePoint>());

struct AppState
{
    GameLib gameLib{HOTRELOAD_LIB_PATH};
    TimePoint lastHotreloadRecompileTime = std::chrono::system_clock::now();

    void reloadIfNeeded();
};

constexpr Duration recompilationTimeout = std::chrono::seconds(2);
constexpr float render_size[] = {800, 600};
constexpr size_t fps = 60;

static std::unique_ptr<AppState> state;

#ifdef __EMSCRIPTEN__
EM_JS(void, emsc_show_restart, (void), {
    /* See the shell.html file for what this does to the webpage */
    if (Module.showRestart)
        Module.showRestart();
});
EMSCRIPTEN_KEEPALIVE extern "C" void emsc_set_window_size(int width, int height)
{
    SetWindowSize(width, height);
}
#endif

void deinit()
{
    rlImGuiShutdown();
    CloseWindow();
}

void update()
{
    state->reloadIfNeeded();

    const bool shouldContinue = state->gameLib.frame();

    if (!shouldContinue) {
        deinit();
#ifdef __EMSCRIPTEN__
        emscripten_cancel_main_loop();
        emsc_show_restart();
#else
        state = {}; // main loop checks if this is null and then exits
#endif
    }
}

#ifndef NO_HOTRELOAD
[[nodiscard]] static std::optional<std::filesystem::file_time_type>
getMostRecentModifyTime(const char *sourceRootPath);
[[nodiscard]] static bool scanForSourceChanges(const char *sourceRootPath,
                                               const char *gameLibPath,
                                               TimePoint lastRecompilationTime);
#endif
int main()
{
    SetTraceLogCallback(detail::logCallback);

    state = std::make_unique<AppState>();

    SetConfigFlags(FLAG_MSAA_4X_HINT | FLAG_VSYNC_HINT | FLAG_WINDOW_RESIZABLE);
    InitWindow(420, 720, "Underhanders");

    SetTargetFPS(60); // physics timestep is 1/60 so everything is fine here

    rlImGuiSetup(true);

    if (not state->gameLib.firstLoad()) {
        LOGERROR_MSG(Hotreload,
                     "Failed to load gamelib " HOTRELOAD_LIB_PATH " initially");
        return EXIT_FAILURE;
    }

#ifdef __EMSCRIPTEN__
    emscripten_set_main_loop(
        update,
        /* fps=0 means vsync */
        0,
        /* simulate_infinite_loop=1 means the browser does
         * requestAnimationFrame, basically needed for vsync as well*/
        1);
#else
    while (!WindowShouldClose() && state) {
        update();
    }
#endif
}

#ifndef NO_HOTRELOAD
static std::optional<std::filesystem::file_time_type>
getMostRecentModifyTime(const char *sourceRootPath)
{
    std::optional<std::filesystem::file_time_type> mostRecent;
    for (const auto &dirent :
         std::filesystem::recursive_directory_iterator(sourceRootPath)) {
        if (not dirent.is_regular_file())
            continue;

        const auto path = dirent.path();
        const auto extension = path.extension();

        if (extension != ".cpp" and extension != ".cppm" and extension != ".h")
            continue;

        std::error_code errorCode;
        const auto lastWriteTime =
            std::filesystem::last_write_time(path, errorCode);

        if (errorCode) {
            LOGWARN(Hotreload,
                    "Unable to read last write time of {}, got error: {}",
                    path.string(), errorCode.message());
            continue;
        }

        if (not mostRecent or lastWriteTime > mostRecent.value())
            mostRecent = lastWriteTime;
    }
    return mostRecent;
}

static bool scanForSourceChanges(const char *sourceRootPath,
                                 const char *gameLibPath,
                                 TimePoint lastRecompilationTime)
{
    const auto sourceLastWriteTime = getMostRecentModifyTime(sourceRootPath);
    if (not sourceLastWriteTime) {
        LOGERROR(Hotreload, "Unable to read any source files at directory {}",
                 sourceRootPath);
        return false;
    }

    using T = std::remove_cvref_t<decltype(*sourceLastWriteTime)>;
    return std::chrono::clock_cast<std::chrono::system_clock, T::clock,
                                   T::duration>(*sourceLastWriteTime) >
           lastRecompilationTime;
}
#endif

void AppState::reloadIfNeeded()
{
#ifndef NO_HOTRELOAD
    if (scanForSourceChanges(HOTRELOAD_SOURCE_ROOT, HOTRELOAD_LIB_PATH,
                             this->lastHotreloadRecompileTime)) {
        std::error_code errorCode;
        const auto dllLastWriteTime =
            std::filesystem::last_write_time(HOTRELOAD_LIB_PATH, errorCode);
        gameLib.unloadIfLoaded();
        std::ignore =
            std::system("cmake --build " HOTRELOAD_BUILD_DIR
                        " --target " HOTRELOAD_LIB_TARGET " --config Debug");
        if (!errorCode) {
            const auto recompiledDllLastWriteTime =
                std::filesystem::last_write_time(HOTRELOAD_LIB_PATH, errorCode);
            if (!errorCode) {
                if (dllLastWriteTime != recompiledDllLastWriteTime) {
                    if (this->gameLib.reload()) {
                        LOGINFO(Hotreload,
                                "Successfully hotreloaded library {}",
                                HOTRELOAD_LIB_PATH);
                    }
                    this->lastHotreloadRecompileTime =
                        std::chrono::system_clock::now();
                } else {
                    LOGERROR_MSG(Hotreload, "Did not see a change in the game "
                                            "DLL, aborting hot reload");
                }
            } else {
                LOGERROR(Hotreload,
                         "Failed to read write time of file {}, got error: {}",
                         HOTRELOAD_LIB_PATH, errorCode.message().c_str());
            }
        } else {
            LOGERROR(Hotreload,
                     "Failed to read write time of file {}, got error: {}",
                     HOTRELOAD_LIB_PATH, errorCode.message());
        }
    }
#endif
}
