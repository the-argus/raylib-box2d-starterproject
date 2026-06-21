#ifndef __UHANDERS_DLLOAD_H__
#define __UHANDERS_DLLOAD_H__

//
// Stuff ripped from https://github.com/maddouri/dynalo
//
// USAGE
//
// LOADABLE_EXPORT <return type> LOADABLE_CALL <function name>(<function
// argument types...>)
//

#if !defined(LOADABLE_DEMANGLE)
#if defined(__cplusplus)
#define LOADABLE_DEMANGLE extern "C"
#else
#define LOADABLE_DEMANGLE
#endif
#endif

#ifdef WIN32
#define LOADABLE_EXPORT LOADABLE_DEMANGLE __declspec(dllexport)
#else
#define LOADABLE_EXPORT LOADABLE_DEMANGLE
#endif

#if !defined(LOADABLE_CALL)
#ifdef _MSC_VER
#define LOADABLE_CALL __cdecl
#else
#define LOADABLE_CALL
#endif
#endif

#if defined(__linux__) || defined(__linux) || defined(linux) || defined(_LINUX)
#define LOADABLE_PLATFORM_LINUX
#elif defined(_WIN32) || defined(_WIN64)
#define LOADABLE_PLATFORM_WINDOWS
#elif defined(__APPLE__)
#define LOADABLE_PLATFORM_MACOS
#else
#error "OS Not Supported"
#endif

#if defined(LOADABLE_PLATFORM_WINDOWS)
#define NOGDICAPMASKS     // CC_*, LC_*, PC_*, CP_*, TC_*, RC_
#define NOVIRTUALKEYCODES // VK_*
#define NOWINMESSAGES     // WM_*, EM_*, LB_*, CB_*
#define NOWINSTYLES       // WS_*, CS_*, ES_*, LBS_*, SBS_*, CBS_*
#define NOSYSMETRICS      // SM_*
#define NOMENUS           // MF_*
#define NOICONS           // IDI_*
#define NOKEYSTATES       // MK_*
#define NOSYSCOMMANDS     // SC_*
#define NORASTEROPS       // Binary and Tertiary raster ops
#define NOSHOWWINDOW      // SW_*
#define OEMRESOURCE       // OEM Resource values
#define NOATOM            // Atom Manager routines
#define NOCLIPBOARD       // Clipboard routines
#define NOCOLOR           // Screen colors
#define NOCTLMGR          // Control and Dialog routines
#define NODRAWTEXT        // DrawText() and DT_*
#define NOGDI             // All GDI defines and routines
#define NOKERNEL          // All KERNEL defines and routines
#define NOUSER            // All USER defines and routines
//#define NONLS             // All NLS defines and routines
#define NOMB              // MB_* and MessageBox()
#define NOMEMMGR          // GMEM_*, LMEM_*, GHND, LHND, associated routines
#define NOMETAFILE        // typedef METAFILEPICT
#define NOMINMAX          // Macros min(a,b) and max(a,b)
#define NOMSG             // typedef MSG and associated routines
#define NOOPENFILE        // OpenFile(), OemToAnsi, AnsiToOem, and OF_*
#define NOSCROLL          // SB_* and scrolling routines
#define NOSERVICE         // All Service Controller routines, SERVICE_ equates, etc.
#define NOSOUND           // Sound driver routines
#define MMNOSOUND         // More sound driver routines
#define NOTEXTMETRIC      // typedef TEXTMETRIC and associated routines
#define NOWH              // SetWindowsHook and WH_*
#define NOWINOFFSETS      // GWL_*, GCL_*, associated routines
#define NOCOMM            // COMM driver routines
#define NOKANJI           // Kanji support stuff.
#define NOHELP            // Help engine interface.
#define NOPROFILER        // Profiler interface.
#define NODEFERWINDOWPOS  // DeferWindowPos routines
#define NOMCX             // Modem Configuration Extensions

typedef struct tagMSG *LPMSG;

#include <Windows.h>
#elif defined(LOADABLE_PLATFORM_MACOS)
#include <dlfcn.h>
#elif defined(LOADABLE_PLATFORM_LINUX)
#include <dlfcn.h>
#endif

namespace dlload {

namespace native {
#if defined(LOADABLE_PLATFORM_WINDOWS)
using handle = HMODULE;

inline handle invalid_handle() { return nullptr; }

namespace name {
inline const char *prefix() { return ""; }
inline const char *suffix() { return ""; }
inline const char *extension() { return "dll"; }
} // namespace name

#elif defined(LOADABLE_PLATFORM_MACOS)

using handle = void *;

inline handle invalid_handle() { return nullptr; }

namespace name {
inline const char *prefix() { return "lib"; }
inline const char *suffix() { return ::; }
inline const char *extension() { return "dylib"; }
} // namespace name

#elif defined(LOADABLE_PLATFORM_LINUX)
using handle = void *;

inline handle invalid_handle() { return nullptr; }

namespace name {
inline const char *prefix() { return "lib"; }
inline const char *suffix() { return ""; }
inline const char *extension() { return "so"; }
} // namespace name
#endif
} // namespace native

inline native::handle open(const char *path)
{
#if defined(LOADABLE_PLATFORM_WINDOWS)
    native::handle h = ::LoadLibrary(path);
#if 0
	// helps debugging
    if (h == nullptr) {
        DWORD err = ::GetLastError();
        char *msg = nullptr;
        ::FormatMessageA(
            FORMAT_MESSAGE_ALLOCATE_BUFFER |
            FORMAT_MESSAGE_FROM_SYSTEM |
            FORMAT_MESSAGE_IGNORE_INSERTS,
            nullptr,
            err,
            MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
            reinterpret_cast<LPSTR>(&msg),
            0,
            nullptr);
        std::fprintf(stderr, "LoadLibrary(\"%s\") failed (error %lu): %s\n",
                     path, err, msg ? msg : "(no message)");
        if (msg) ::LocalFree(msg);
    }
#endif
    return h;
#else
    return ::dlopen(path, RTLD_LAZY);
#endif
}

inline const char *error()
{
#if defined(LOADABLE_PLATFORM_WINDOWS)
    // LPVOID lpMsgBuf;
    // DWORD dw = GetLastError();

    // if (::FormatMessage(FORMAT_MESSAGE_ALLOCATE_BUFFER |
    //                         FORMAT_MESSAGE_FROM_SYSTEM |
    //                         FORMAT_MESSAGE_IGNORE_INSERTS,
    //                     NULL, dw, MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
    //                     (LPTSTR)&lpMsgBuf, 0, NULL) == 0) {
    //     return "There was an error formatting an error related to dlload.h";
    // }
    // ::LocalFree(lpMsgBuf);

    // TODO: have this
    return "Unable to format error on windows platform";
#else
    return ::dlerror();
#endif
}

inline void close(native::handle handle)
{
    // ignoring return values of close functions, which can error?
#if defined(LOADABLE_PLATFORM_WINDOWS)
    // https://msdn.microsoft.com/en-us/library/windows/desktop/ms683152(v=vs.85).aspx
    ::FreeLibrary(handle);
#else
    ::dlclose(handle);
#endif
}

/// Returns nullptr if function cannot be found
inline void *get_function(native::handle handle, const char *function)
{
#if defined(LOADABLE_PLATFORM_WINDOWS)
    FARPROC ptr = ::GetProcAddress(handle, function);
#if 0
	// helps debugging
	if (ptr == nullptr) {
        DWORD err = ::GetLastError();
        char *msg = nullptr;
        ::FormatMessageA(
            FORMAT_MESSAGE_ALLOCATE_BUFFER |
            FORMAT_MESSAGE_FROM_SYSTEM |
            FORMAT_MESSAGE_IGNORE_INSERTS,
            nullptr,
            err,
            MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
            reinterpret_cast<LPSTR>(&msg),
            0,
            nullptr);
        std::fprintf(stderr, "GetProcAddress(\"%s\") failed (error %lu): %s\n",
                     function, err, msg ? msg : "(no message)");
        if (msg) ::LocalFree(msg);
    }
#endif
    return *reinterpret_cast<void **>(&ptr);
#else
    void *ptr = ::dlsym(handle, function);
    return static_cast<void *>(ptr);
#endif
}

} // namespace dlload

#endif
