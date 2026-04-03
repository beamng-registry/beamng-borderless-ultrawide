# BeamNG Borderless Ultrawide

Force BeamNG.drive into a true borderless window spanning multiple monitors.

The project consists of two parts:

| Component | Description |
|-----------|-------------|
| **borderless.dll** | Hooks `AdjustWindowRectEx`, `MoveWindow` and `SetWindowPos` inside the game process to strip window borders and reposition/resize the window to cover all monitors. |
| **injector.exe** | Tiny helper that injects `borderless.dll` into a running BeamNG.drive process. |

## How it works

When loaded into the game process the DLL installs [MinHook](https://github.com/TsudaKageyu/minhook) inline hooks on three Win32 APIs:

| API | What the hook does |
|-----|--------------------|
| `AdjustWindowRectEx` | Returns immediately **without** inflating the rectangle, so the engine never adds borders. |
| `MoveWindow` | Strips `WS_OVERLAPPEDWINDOW` styles, adds `WS_POPUP`, and forces the window to the position/size from `config.ini`. |
| `SetWindowPos` | Same style fix; also clears `SWP_NOMOVE` / `SWP_NOSIZE` flags and forces the configured position/size. |

## Installation

### Option 1. Download a release

1. Open the [Releases](https://github.com/beamng-registry/beamng-borderless-ultrawide/releases) page.
2. Download `borderless.dll` and `injector.exe`, or grab the release `.zip` bundle.
3. Copy `config.ini` next to `borderless.dll` and edit the values for your monitor layout.
4. Launch **BeamNG.drive** in windowed mode.
5. Run `injector.exe` as Administrator to inject the DLL into the game.

### Option 2. Build it yourself (Windows + CMake ≥ 3.15 + MSVC)

```bash
cmake -B build -A x64
cmake --build build --config Release
```

Output:

* `build/Release/borderless.dll`
* `build/Release/injector.exe`

### 2. Configure

Copy `config.ini` next to `borderless.dll` and edit the values for your monitor layout:

```ini
[Window]
x=-1920
y=0
width=5760
height=1080
debug=0
```

The defaults above assume **three 1920 × 1080** monitors arranged side-by-side with the centre monitor as primary.

### 3. Inject

1. Launch **BeamNG.drive** normally (windowed mode, any resolution).
2. Run the injector **as Administrator**:

```bash
injector.exe
```

The injector automatically finds the `BeamNG.drive.x64.exe` process.  
You can also pass a PID explicitly: `injector.exe 12345`.

### 4. Debugging

Set `debug=1` in `config.ini`.  A `borderless.log` file will be written next to the DLL with details about every intercepted API call.

## Project structure

```
├── CMakeLists.txt          # Build system (fetches MinHook via FetchContent)
├── config.ini              # Default configuration
├── src/
│   ├── dllmain.cpp         # DLL entry point
│   ├── hooks.h / hooks.cpp # MinHook-based API hooks
│   └── config.h / config.cpp  # INI configuration loader
└── injector/
    └── main.cpp            # DLL injector utility
```

## Requirements

* Windows 10/11 x64
* CMake ≥ 3.15
* MSVC (Visual Studio 2019+ recommended)
* BeamNG.drive (64-bit)

## License

MIT
