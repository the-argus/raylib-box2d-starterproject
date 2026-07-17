#include "defer.h"
#include "game_lib.h"
#include "logging.h"

#include <fmt/format.h>

// include raylib earlier and removed USE_LIBTYPE_SHARED. rlImGui is using
// these raylib flags, which makes sense, but I want to link imgui statically
// and raylib dynamically, in debug mode. This causes linker errors because
// USE_LIBTYPE_SHARED is a public flag from raylib, which causes the rlimgui
// headers to declare declspec dllimport symbols.
#include <raylib.h>
#undef USE_LIBTYPE_SHARED
#ifdef BUILD_LIBTYPE_SHARED
#error "BUILD_LIBTYPE_SHARED is defined for the executable?"
#endif

#include <rlImGui.h>
#include <rlImGuiColors.h>

#include <imgui.h>
#include <imgui_impl_raylib.h>

#include <chrono>
#include <filesystem>

using TimePoint = std::chrono::time_point<std::chrono::system_clock>;
using Duration =
    decltype(std::declval<TimePoint>() - std::declval<TimePoint>());

struct AppState
{
    GameLib gameLib;
    TimePoint lastHotreloadRecompileTime = std::chrono::system_clock::now();

    void reloadIfNeeded();
};

constexpr Duration recompilationTimeout = std::chrono::seconds(2);
constexpr float render_size[] = {800, 600};
constexpr size_t fps = 60;

#ifndef HOTRELOAD_LIB_PATH
#define HOTRELOAD_LIB_PATH "(HOTRELOAD DISABLED)"
#define NO_HOTRELOAD
#else
[[nodiscard]] static std::optional<std::filesystem::file_time_type>
getMostRecentModifyTime(const char *sourceRootPath);
[[nodiscard]] static bool scanForSourceChanges(const char *sourceRootPath,
                                               const char *gameLibPath,
                                               TimePoint lastRecompilationTime);
#endif
int main()
{
    SetTraceLogCallback(detail::logCallback);

    AppState state{GameLib(HOTRELOAD_LIB_PATH)};

    if (not state.gameLib.firstLoad()) {
        LOGERROR_MSG(Hotreload,
                     "Failed to load gamelib " HOTRELOAD_LIB_PATH " initially");
        return EXIT_FAILURE;
    }

    InitWindow(420, 720, "Underhanders");
    defer closeWindow = [] { CloseWindow(); };

    rlImGuiSetup(true);
    defer closeRlImGui = [] { rlImGuiShutdown(); };

    while (!WindowShouldClose()) {
        state.reloadIfNeeded();
        // ImGui_ImplSDLRenderer3_NewFrame();
        // ImGui_ImplSDL3_NewFrame();
        // ImGui::NewFrame();

        const bool shouldContinue = state.gameLib.frame();

        // ImGui::Render();
        // ImGui_ImplSDLRenderer3_RenderDrawData(ImGui::GetDrawData(),
        // renderer);

        if (!shouldContinue) {
            break;
        }
    }
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
    return std::chrono::clock_cast<std::chrono::system_clock, T::clock, T::duration>(*sourceLastWriteTime) > lastRecompilationTime;
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
        std::ignore =
            std::system(fmt::format("cmake --build {} --target {}",
                                    HOTRELOAD_BUILD_DIR, HOTRELOAD_LIB_TARGET)
                            .c_str());
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
