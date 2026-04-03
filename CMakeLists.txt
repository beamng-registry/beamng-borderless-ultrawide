cmake_minimum_required(VERSION 3.15)
project(beamng-borderless-ultrawide LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# ── Fetch MinHook ────────────────────────────────────────────────────────────
include(FetchContent)
FetchContent_Declare(
    minhook
    GIT_REPOSITORY https://github.com/TsudaKageyu/minhook.git
    GIT_TAG        v1.3.4
)
FetchContent_MakeAvailable(minhook)

# ── Hook DLL ─────────────────────────────────────────────────────────────────
add_library(borderless SHARED
    src/dllmain.cpp
    src/hooks.cpp
    src/config.cpp
)
target_include_directories(borderless PRIVATE src)
target_link_libraries(borderless PRIVATE minhook)
set_target_properties(borderless PROPERTIES
    OUTPUT_NAME "borderless"
    PREFIX ""
    SUFFIX ".dll"
)

# ── Injector ─────────────────────────────────────────────────────────────────
add_executable(injector injector/main.cpp)
target_link_libraries(injector PRIVATE shlwapi)
