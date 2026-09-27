# Candle

Candle is a from-scratch C++23 game engine for Windows, built on Vulkan. It is the successor to
[Ember](https://github.com/ConnorShore/Ember), an OpenGL engine with an editor, ECS, physics, audio,
animation and Lua scripting, and it exists to do the things Ember's architecture could not grow into:

- **Multithreading driven** Ember was a single threaded engine while Candle strives to be a fully   multithreaded engine.
- **Vulkan instead of OpenGL.** Candle looks to more modern tech (Vulkan) to achieve better graphics and performance and bringing modern graphics features to the engine.

## Requirements

| Tool | Version | Notes |
|---|---|---|
| Windows | 10 or 11, x64 | The only supported platform |
| Visual Studio | 2026 | With the **Desktop development with C++** workload |
| Python | 3.10+ | Drives the init and build scripts |
| Git | any recent | Dependencies are submodules |
| [Vulkan SDK](https://vulkan.lunarg.com/sdk/home) | 1.4.357.0 | Must set `VULKAN_SDK`; open a new terminal after installing |

Premake is not a prerequisite — the init script downloads a pinned, checksum-verified copy.

## Getting started

**1. Clone with submodules.** SDL3, Dear ImGui (docking branch) and GLM are git submodules.

```bat
git clone --recursive https://github.com/ConnorShore/Candle.git
cd Candle
```

If you already cloned without `--recursive`:

```bat
git submodule update --init --recursive
```

**2. Initialize the repository.** Downloads Premake into `vendor/premake/bin/` and verifies the
Vulkan SDK is installed. It exits with an error and a download link if `VULKAN_SDK` is missing.

```bat
python scripts/Base/InitializeRepo.py
```

**3. Generate, build and test.**

```bat
scripts\Windows\Build.bat                    REM clean, generate, and build Debug
python scripts/Base/Build.py test            REM run the test suite
```

Then open `Candle.slnx` in Visual Studio. `Sandbox` is the startup project.

## Building

`scripts\Windows\Build.bat` forwards its arguments to `scripts/Base/Build.py`. Commands run **in the
order typed**; with no arguments it runs `clean generate build`.

| Command | What it does |
|---|---|
| `clean` | Deletes `bin/`, `.vs/` and every generated `.slnx`/`.vcxproj` (never inside submodules) |
| `generate` | Runs Premake to produce `Candle.slnx` and the project files |
| `build` | Builds the solution with MSBuild. Does **not** generate first |
| `test` | Runs `Candle-Test`. Does **not** build first — use `build test` |

A configuration — `Debug` (default), `Release`, `Profile` or `Dist` — can appear anywhere in the
argument list. Arguments starting with `--` are passed through to the test runner.

```bat
scripts\Windows\Build.bat build Release
scripts\Windows\Build.bat generate build Profile
scripts\Windows\Build.bat build test Release --filter=unit
```

Re-run `generate` whenever you add or remove source files or edit a `premake5.lua` — Premake globs
sources at generation time, and the Visual Studio projects are not resynced afterwards.

`Build.bat` ends in `pause`. From scripts, hooks or CI, call `python scripts/Base/Build.py` directly.

### Configurations

| Configuration | Define | Purpose |
|---|---|---|
| Debug | `CDL_DEBUG` | Unoptimized, symbols, asserts on |
| Release | `CDL_RELEASE` | Optimized, asserts on |
| Profile | `CDL_PROFILE` + `CDL_RELEASE` | Optimized with symbols, for profiling |
| Dist | `CDL_DIST` | Shipping: static runtime, no symbols, no asserts, no console |

### Tests

`Candle-Test` is a headless test executable — it needs no window or GPU, so it runs in CI.

```bat
python scripts/Base/Build.py build test                     REM everything
python scripts/Base/Build.py build test --filter=unit       REM skip multi-threaded stress tests
python scripts/Base/Build.py build test --run=RingBuffer    REM tests whose name contains a substring
```

| Dependency | Version | Used for |
|---|---|---|
| [SDL3](https://github.com/libsdl-org/SDL) | `release-3.4.16` | Window, input, gamepads (audio, camera, GPU, 2D render stripped) |
| [Dear ImGui](https://github.com/ocornut/imgui) | `v1.92.9b-docking` | Debug UI and the future editor |
| [GLM](https://github.com/g-truc/glm) | `1.0.3` | Math |
