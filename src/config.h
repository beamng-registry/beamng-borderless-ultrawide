#pragma once

#include <string>

struct Config {
    int x       = -1920;
    int y       = 0;
    int width   = 5760;   // 3 * 1920
    int height  = 1080;
    bool debug  = false;
};

// Load configuration from an INI file next to the DLL.
// Falls back to built-in defaults when the file is missing.
Config LoadConfig();

// Resolve the full path to the INI file (sits next to the DLL).
std::wstring GetConfigPath();
