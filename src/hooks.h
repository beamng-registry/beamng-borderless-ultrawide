#pragma once

#include "config.h"

// Install MinHook-based API hooks. Call once from DLL_PROCESS_ATTACH.
bool InstallHooks(const Config& cfg);

// Remove all hooks. Call from DLL_PROCESS_DETACH.
void RemoveHooks();
