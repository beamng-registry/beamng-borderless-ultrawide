#include "hooks.h"

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <Windows.h>
#include <cstdio>

#include "MinHook.h"

// ---------------------------------------------------------------------------
// Globals
// ---------------------------------------------------------------------------
static Config g_cfg;
static FILE*  g_log = nullptr;

// Original function pointers (filled by MinHook)
typedef BOOL(WINAPI* PFN_AdjustWindowRectEx)(LPRECT, DWORD, BOOL, DWORD);
typedef BOOL(WINAPI* PFN_MoveWindow)(HWND, int, int, int, int, BOOL);
typedef BOOL(WINAPI* PFN_SetWindowPos)(HWND, HWND, int, int, int, int, UINT);

static PFN_AdjustWindowRectEx fpAdjustWindowRectEx = nullptr;
static PFN_MoveWindow        fpMoveWindow         = nullptr;
static PFN_SetWindowPos      fpSetWindowPos       = nullptr;

// Track the main game window handle
static HWND g_gameHwnd = nullptr;

// Re-entrancy guard (prevents loops when MakeBorderless triggers internal calls)
static thread_local bool g_inHook = false;

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------
static void Log(const char* fmt, ...)
{
    if (!g_log) return;
    va_list ap;
    va_start(ap, fmt);
    vfprintf(g_log, fmt, ap);
    va_end(ap);
    fflush(g_log);
}

static bool IsGameWindow(HWND hWnd)
{
    if (!hWnd) return false;

    // If we already know the game window, compare directly.
    if (g_gameHwnd) return hWnd == g_gameHwnd;

    // Heuristic: top-level window in our process, not a child.
    DWORD pid = 0;
    GetWindowThreadProcessId(hWnd, &pid);
    if (pid != GetCurrentProcessId()) return false;

    LONG style = GetWindowLongW(hWnd, GWL_STYLE);
    if (style & WS_CHILD) return false;

    // Accept any top-level window that has typical game styles.
    return true;
}

static bool IsAtTargetRect(HWND hWnd)
{
    RECT rc;
    return GetWindowRect(hWnd, &rc) &&
           rc.left == g_cfg.x && rc.top == g_cfg.y &&
           (rc.right - rc.left) == g_cfg.width &&
           (rc.bottom - rc.top) == g_cfg.height;
}

static void MakeBorderless(HWND hWnd)
{
    constexpr LONG kRemoveStyle = WS_CAPTION | WS_THICKFRAME |
                                  WS_MINIMIZEBOX | WS_MAXIMIZEBOX | WS_SYSMENU;
    constexpr LONG kRemoveExStyle = WS_EX_DLGMODALFRAME | WS_EX_CLIENTEDGE |
                                    WS_EX_STATICEDGE | WS_EX_WINDOWEDGE;

    LONG style = GetWindowLongW(hWnd, GWL_STYLE);
    LONG desired = (style & ~kRemoveStyle) | WS_POPUP;
    if (style != desired)
        SetWindowLongW(hWnd, GWL_STYLE, desired);

    LONG exStyle = GetWindowLongW(hWnd, GWL_EXSTYLE);
    LONG desiredEx = exStyle & ~kRemoveExStyle;
    if (exStyle != desiredEx)
        SetWindowLongW(hWnd, GWL_EXSTYLE, desiredEx);
}

// ---------------------------------------------------------------------------
// Hooked functions
// ---------------------------------------------------------------------------

static BOOL WINAPI HookedAdjustWindowRectEx(
    LPRECT lpRect, DWORD dwStyle, BOOL bMenu, DWORD dwExStyle)
{
    // Return TRUE without modifying the rect so the engine thinks there are
    // no borders.  This prevents it from inflating the client area.
    Log("[hook] AdjustWindowRectEx intercepted – returning rect unchanged\n");
    return TRUE;
}

static BOOL WINAPI HookedMoveWindow(
    HWND hWnd, int X, int Y, int nWidth, int nHeight, BOOL bRepaint)
{
    if (!g_inHook && IsGameWindow(hWnd))
    {
        g_inHook = true;

        if (!g_gameHwnd)
        {
            g_gameHwnd = hWnd;
            Log("[hook] Game window detected: 0x%p\n", hWnd);
        }

        MakeBorderless(hWnd);

        // Skip the call entirely if the window is already at the target rect.
        if (IsAtTargetRect(hWnd))
        {
            Log("[hook] MoveWindow suppressed – already at target rect\n");
            g_inHook = false;
            return TRUE;
        }

        Log("[hook] MoveWindow(%d,%d,%d,%d) -> (%d,%d,%d,%d)\n",
            X, Y, nWidth, nHeight,
            g_cfg.x, g_cfg.y, g_cfg.width, g_cfg.height);

        BOOL ret = fpMoveWindow(hWnd,
                            g_cfg.x, g_cfg.y,
                            g_cfg.width, g_cfg.height,
                            bRepaint);
        g_inHook = false;
        return ret;
    }

    return fpMoveWindow(hWnd, X, Y, nWidth, nHeight, bRepaint);
}

static BOOL WINAPI HookedSetWindowPos(
    HWND hWnd, HWND hWndInsertAfter,
    int X, int Y, int cx, int cy, UINT uFlags)
{
    if (!g_inHook && IsGameWindow(hWnd))
    {
        g_inHook = true;

        if (!g_gameHwnd)
        {
            g_gameHwnd = hWnd;
            Log("[hook] Game window detected (SetWindowPos): 0x%p\n", hWnd);
        }

        MakeBorderless(hWnd);

        // Strip NOMOVE / NOSIZE so our coordinates take effect.
        uFlags &= ~(SWP_NOMOVE | SWP_NOSIZE);

        // Skip the call entirely if the window is already at the target rect.
        if (IsAtTargetRect(hWnd))
        {
            Log("[hook] SetWindowPos suppressed – already at target rect\n");
            g_inHook = false;
            return TRUE;
        }

        // Suppress WM_WINDOWPOSCHANGING to reduce message-loop pressure.
        uFlags |= SWP_NOSENDCHANGING;

        Log("[hook] SetWindowPos flags=0x%X -> pos(%d,%d) size(%d,%d)\n",
            uFlags, g_cfg.x, g_cfg.y, g_cfg.width, g_cfg.height);

        BOOL ret = fpSetWindowPos(hWnd, hWndInsertAfter,
                              g_cfg.x, g_cfg.y,
                              g_cfg.width, g_cfg.height,
                              uFlags);
        g_inHook = false;
        return ret;
    }

    return fpSetWindowPos(hWnd, hWndInsertAfter, X, Y, cx, cy, uFlags);
}

// ---------------------------------------------------------------------------
// Public API
// ---------------------------------------------------------------------------

bool InstallHooks(const Config& cfg)
{
    g_cfg = cfg;

    if (cfg.debug)
    {
        std::wstring logPath = GetConfigPath();
        // Replace config.ini → borderless.log
        auto pos = logPath.find_last_of(L"\\/");
        if (pos != std::wstring::npos)
            logPath = logPath.substr(0, pos + 1);
        logPath += L"borderless.log";
        g_log = _wfopen(logPath.c_str(), L"w");
    }

    Log("[init] Target rect: x=%d y=%d w=%d h=%d\n",
        cfg.x, cfg.y, cfg.width, cfg.height);

    if (MH_Initialize() != MH_OK)
    {
        Log("[init] MH_Initialize failed\n");
        return false;
    }

    auto hook = [](LPCWSTR mod, LPCSTR fn, LPVOID detour, LPVOID* original) -> bool {
        LPVOID target = (LPVOID)GetProcAddress(GetModuleHandleW(mod), fn);
        if (!target) return false;
        if (MH_CreateHook(target, detour, original) != MH_OK) return false;
        if (MH_EnableHook(target) != MH_OK) return false;
        return true;
    };

    bool ok = true;
    ok &= hook(L"user32.dll", "AdjustWindowRectEx",
               (LPVOID)HookedAdjustWindowRectEx, (LPVOID*)&fpAdjustWindowRectEx);
    ok &= hook(L"user32.dll", "MoveWindow",
               (LPVOID)HookedMoveWindow, (LPVOID*)&fpMoveWindow);
    ok &= hook(L"user32.dll", "SetWindowPos",
               (LPVOID)HookedSetWindowPos, (LPVOID*)&fpSetWindowPos);

    Log("[init] Hooks installed: %s\n", ok ? "OK" : "PARTIAL FAILURE");
    return ok;
}

void RemoveHooks()
{
    MH_DisableHook(MH_ALL_HOOKS);
    MH_Uninitialize();

    if (g_log)
    {
        fclose(g_log);
        g_log = nullptr;
    }

    g_gameHwnd = nullptr;
}
