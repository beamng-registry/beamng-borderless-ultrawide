#include "config.h"

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <Windows.h>

// Forward-declare the DLL module handle set in dllmain.cpp.
extern HMODULE g_hModule;

std::wstring GetConfigPath()
{
    wchar_t dllPath[MAX_PATH]{};
    GetModuleFileNameW(g_hModule, dllPath, MAX_PATH);

    // Replace the DLL filename with config.ini
    std::wstring path(dllPath);
    auto pos = path.find_last_of(L"\\/");
    if (pos != std::wstring::npos)
        path = path.substr(0, pos + 1);
    path += L"config.ini";
    return path;
}

Config LoadConfig()
{
    Config cfg;
    std::wstring ini = GetConfigPath();
    const wchar_t* section = L"Window";

    cfg.x      = GetPrivateProfileIntW(section, L"x",      cfg.x,      ini.c_str());
    cfg.y      = GetPrivateProfileIntW(section, L"y",      cfg.y,      ini.c_str());
    cfg.width  = GetPrivateProfileIntW(section, L"width",  cfg.width,  ini.c_str());
    cfg.height = GetPrivateProfileIntW(section, L"height", cfg.height, ini.c_str());
    cfg.debug  = GetPrivateProfileIntW(section, L"debug",  0,          ini.c_str()) != 0;

    return cfg;
}
