#ifndef __UHANDERS_GAME_LIB_H__
#define __UHANDERS_GAME_LIB_H__

#include "macros.h"

#ifndef NO_HOTRELOAD
#include "dlload.h"
#include "logging.h"
#endif

#include <imgui.h>

namespace hotreload {

/// Stuff that needs to be restored / consistent across the DLL boundary
struct GlobalContext
{
    ImGuiContext *imguiContext;
    ImGuiMemAllocFunc imguiAlloc;
    ImGuiMemFreeFunc imguiFree;
    void *imguiAllocUsrData;
};

using FrameCallback = bool (*)(void *ctx);
using DeinitCallback = void (*)(void *ctx);
using HotReloadedCallback = bool (*)(const GlobalContext *);
using InitCallback = void *(*)();
} // namespace hotreload

#ifdef NO_HOTRELOAD
extern "C"
{
    HOTRELOAD_EXPORT extern void *init();
    HOTRELOAD_EXPORT extern void deinit(void *);
    HOTRELOAD_EXPORT extern void
    onHotReload(const hotreload::GlobalContext *context);
    HOTRELOAD_EXPORT extern bool frame(void *context);
}
#endif

class GameLib
{
  private:
    const char *m_libPath;
#ifndef NO_HOTRELOAD
    dlload::native::handle m_library = nullptr;
    hotreload::FrameCallback m_frameCallback = nullptr;
    hotreload::DeinitCallback m_deinitCallback = nullptr;
    hotreload::InitCallback m_initCallback = nullptr;
    hotreload::HotReloadedCallback m_onHotReloadCallback = nullptr;
#endif
    void *m_gameContext = nullptr;

  public:
    GameLib() = delete;
    constexpr GameLib(const char *libPath) : m_libPath(libPath) {}

    GameLib(GameLib &&) = delete;
    GameLib(const GameLib &) = delete;
    GameLib &operator=(GameLib &&) = delete;
    GameLib &operator=(const GameLib &) = delete;
    ~GameLib() { unloadIfLoaded(); }

    void unloadIfLoaded()
    {
#ifndef NO_HOTRELOAD
        if (m_library) {
            if (m_deinitCallback) {
                m_deinitCallback(m_gameContext);
            }
            dlload::close(m_library);
            m_library = nullptr;
            m_frameCallback = nullptr;
            m_deinitCallback = nullptr;
            m_initCallback = nullptr;
            m_onHotReloadCallback = nullptr;
        }
#else
        ::deinit(m_gameContext);
#endif
    }

    [[nodiscard]] bool firstLoad()
    {
        uassert(not m_gameContext, "firstLoad called multiple times");
        const bool status = reload();

        if (status)
            m_gameContext =
#ifdef NO_HOTRELOAD
                ::init();
#else
                m_initCallback();
#endif

        return status;
    }

    /// Returns true on success and false on failure
    [[nodiscard]] bool reload()
    {
#ifndef NO_HOTRELOAD
        unloadIfLoaded();

        m_library = dlload::open(m_libPath);
        if (!m_library) {
            LOGERROR(Hotreload, "Failed to hotreload library {}, got error: {}",
                     m_libPath, dlload::error());
            return false;
        }

        const auto load = [this]<typename FuncPtr>(FuncPtr &functionPointer,
                                                   const char *symbolName) {
            functionPointer =
                FuncPtr(dlload::get_function(m_library, symbolName));
            if (!functionPointer) {
                LOGERROR(Hotreload,
                         "Failed to hotreload symbol %s, got error: %s",
                         symbolName, dlload::error());
            }
            return bool(functionPointer);
        };

        if (not load(m_frameCallback, "frame"))
            return false;
        if (not load(m_initCallback, "init"))
            return false;
        if (not load(m_deinitCallback, "deinit"))
            return false;
        if (not load(m_onHotReloadCallback, "onHotReload"))
            return false;
        else {
            hotreload::GlobalContext ctx{
                .imguiContext = ImGui::GetCurrentContext(),
            };
            ImGui::GetAllocatorFunctions(&ctx.imguiAlloc, &ctx.imguiFree,
                                         &ctx.imguiAllocUsrData);
            m_onHotReloadCallback(&ctx);
        }
#endif
        return true;
    }

    [[nodiscard]] bool frame()
    {
#ifndef NO_HOTRELOAD
        if (m_frameCallback) {
            return m_frameCallback(m_gameContext);
        }
        return true;
#else
        return ::frame(m_gameContext);
#endif
    }
};

#endif
