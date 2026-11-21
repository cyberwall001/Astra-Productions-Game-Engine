# Building Astra Studio on Windows

## Requirements

| Tool | Version | Notes |
| --- | --- | --- |
| Windows | 10 1809+ / 11 | x64 |
| Visual Studio 2022 | 17.6+ | "Desktop development with C++" workload (MSVC v143 + Windows 10/11 SDK) |
| CMake | 3.20+ | bundled with VS 2022 works (`cmake --version` in a Developer Prompt) |
| Git | any | GLFW is fetched with `FetchContent` on first configure |
| GPU driver | OpenGL 3.2 core | Intel/AMD/NVIDIA all qualify |

No vcpkg, no Conan, no Python. The only third-party code is vendored in `extern/`
(Dear ImGui, stb_image) plus GLFW, which CMake downloads once.

## Build

From a **Developer PowerShell for VS 2022** in the repository root:

```powershell
cmake --preset windows-msvc
cmake --build --preset windows-msvc-release
ctest --preset windows-msvc
```

Outputs:

```
build/windows-msvc/Release/AstraStudio.exe      launcher + editor
build/windows-msvc/Release/AstraKeyForge.exe    owner-only access code tool
build/windows-msvc/Release/assets/...           fonts, branding, demo workspace
```

The build copies `assets/` next to the executables automatically.

### Cross-compiling from Linux (no MSVC, no Windows runner)

When the Windows CI job cannot start, the same targets cross-compile from Linux with
[Zig](https://ziglang.org)'s bundled mingw-w64 toolchain:

```bash
pip install ziglang cmake
tools/package_windows_zig.sh          # -> dist/astra-studio-<version>-windows-x64.zip
```

The script fetches GLFW 3.4, configures with `-DCMAKE_SYSTEM_NAME=Windows -G "Unix Makefiles"`,
builds `astra_studio` + `astra_keyforge` and stages the same layout as
`tools/package_windows.ps1` (exe + `assets/` + docs + README).

Differences from the official build, which matter if you are debugging a crash:

* mingw-w64 instead of MSVC — the C++ runtime and libc are linked statically, so the archive
  needs no extra DLLs, but MSVC-only code paths are compiled by GCC semantics;
* the resulting archive is a preview; the Release workflow's MSVC build remains the official
  artifact on the Releases page.

### Without presets

```powershell
cmake -S . -B build -G "Visual Studio 17 2022" -A x64
cmake --build build --config Release
ctest --test-dir build -C Release
```

### Offline / air-gapped

If the machine cannot reach GitHub, point CMake at a local GLFW checkout:

```powershell
cmake --preset windows-msvc -DASTRA_GLFW_DIR=C:/src/glfw
```

## Package the download

```powershell
pwsh tools/package_windows.ps1 -Configuration Release
```

Produces `dist/astra-studio-<version>-windows-x64.zip` containing both executables,
`assets/`, the docs and a `README.txt` with first-run instructions. CI
(`.github/workflows/windows.yml`) runs the same script and uploads the zip as
`astra-studio-windows-x64`, so a tagged build is download-ready without a local toolchain.

## Headless tools (any OS)

`astra_selfcheck` and `astra_uipreview` build without GLFW, OpenGL or a display:

```bash
cmake --preset headless && cmake --build --preset headless
./build/headless/astra_selfcheck
./build/headless/astra_uipreview --smoke
./build/headless/astra_uipreview --out docs/screenshots
python3 tools/recompress_png.py docs/screenshots
```

## Troubleshooting

**`Could not find a package configuration file for glfw3` / FetchContent fails**
First configure needs network access for `github.com/glfw/glfw`. Retry, or pass
`-DASTRA_GLFW_DIR=<local checkout>`.

**Window opens but the UI is tiny / blurry**
The window reads the monitor content scale at startup. If you change Windows scaling while
the app is running, restart it — font atlases are baked per scale.

**`GLFW error 65542: WGL: The driver does not appear to support OpenGL`**
The GPU driver only exposes OpenGL 1.1 (common on RDP sessions and some VMs). Run locally on
real hardware, or update the driver.

**Fonts look wrong / missing glyphs**
`assets/fonts` must sit next to the executable (the build and the packaging script copy it).
The glyph range is extended for en/em dashes and quotes in `ui::Theme`; add new characters
there if UI copy introduces them.

**`AstraStudio.exe` opens and closes immediately**
Run it from a terminal; the log prints startup failures. The most common cause is a
read-only `%LOCALAPPDATA%\Astra Productions`.

**Tests fail on a fresh clone**
`astra_selfcheck` deletes and recreates `/tmp/astra-selfcheck-data` (or the Windows temp
equivalent) on every run, so failures are real. Run with `--filter licensing` to isolate.

**KeyForge says "Owner passphrase rejected"**
The passphrase is per-machine, stored in `keyforge/owner.json`. Delete that file to start
over — the issue ledger is separate and survives (`keyforge/issued_codes.json`).
