# Astra Productions Game Engine

[![Latest release](https://img.shields.io/github/v/release/cyberwall001/Astra-Productions-Game-Engine?label=release&color=FF5F5A)](https://github.com/cyberwall001/Astra-Productions-Game-Engine/releases/latest)
[![Windows build](https://github.com/cyberwall001/Astra-Productions-Game-Engine/actions/workflows/windows.yml/badge.svg)](https://github.com/cyberwall001/Astra-Productions-Game-Engine/actions/workflows/windows.yml)

**Download:** every merge to `main` publishes a fresh `v0.x` build — grab
`astra-studio-<version>-windows-x64.zip` from
[Releases](https://github.com/cyberwall001/Astra-Productions-Game-Engine/releases/latest).

A general-purpose game engine and studio, written from scratch in C++17 for Windows.
No Unity, no Unreal, no wrapper: the window layer, renderer, scene graph, crypto, account
registry, offline access-code licensing, GitHub project hub and the whole UI toolkit live in
this repository.

**Milestone 3 — play it, script it, light it.** Astra now runs the level: press **Play**
(`F5`) and the viewport possesses a first-person pawn (WASD, jump, sprint, mouse-look) with the
editor chrome hidden. Next to it sit three new subsystems:

- **AstraScript** — a real scripting language with a Blender-flavoured workspace: file column,
  syntax-highlighted editor, console, and script-generated images. Meshes and images can be
  authored from code. See [docs/SCRIPTING.md](docs/SCRIPTING.md).
- **Offline path tracer** — the *Render* panel ray traces the level on every core (BVH, soft sun
  shadows, ambient occlusion, metallic reflections, ACES tonemap) and saves a PNG.
- **Blender bridge** — a folder-based addon (`tools/blender/astra_bridge.py`) sends meshes
  straight into the level's mesh library. See [docs/BLENDER_BRIDGE.md](docs/BLENDER_BRIDGE.md).
* **Authored materials** — `*.astramat` text files in `<project>/materials`, hot reloaded while
  you edit them, with a Material Editor window and a scripted `applyMaterial()`. See
  [docs/MATERIALS.md](docs/MATERIALS.md).

Under the hood: real mesh/material/particle data, soft ground shadows, volumetric light shafts
and a cloud deck, undo/redo across the whole editor, and the UE panel vocabulary
(**World Outliner · Level Viewport · Details · Content Browser · Output Log**).

![Astra Studio editor](docs/screenshots/09-editor.png)

| Shaded | Wireframe |
| --- | --- |
| ![Shaded viewport](docs/screenshots/09-editor.png) | ![Wireframe viewport](docs/screenshots/09-editor-wireframe.png) |

Weather is now one optional system among many: the default level is a neutral sandbox, and
storm volumes are a template you can drop in from the Content Browser.

## What is in the box

| | |
| --- | --- |
| **Astra Studio** (`AstraStudio.exe`) | loading screen → boot sequence → sign-in → registration → project hub → editor |
| **Astra KeyForge** (`AstraKeyForge.exe`) | owner-only tool that mints single-use engine access codes |
| **Headless tools** | `astra_selfcheck` (373 assertions, 19 suites) and `astra_uipreview` (renders every screen to PNG and drives real interactions) |

### Launcher flow

| Loading | Boot sequence | Sign in |
| --- | --- | --- |
| ![Loading](docs/screenshots/01-boot-loading.png) | ![Boot](docs/screenshots/02-boot-sequence.png) | ![Sign in](docs/screenshots/03-sign-in.png) |

| Access code | Registration | Project hub |
| --- | --- | --- |
| ![Access code](docs/screenshots/04-register-code.png) | ![Account](docs/screenshots/06-register-account.png) | ![Hub](docs/screenshots/07-project-hub.png) |

### Studio

| Level viewport | Play in editor | Script workspace |
| --- | --- | --- |
| ![Editor](docs/screenshots/09-editor.png) | ![Play in editor](docs/screenshots/14-play-in-editor.png) | ![Script editor](docs/screenshots/16-script-editor.png) |

| Path tracer | World settings | Blender import |
| --- | --- | --- |
| ![Render panel](docs/screenshots/15-render-panel.png) | ![World settings](docs/screenshots/17-world-settings.png) | ![Blender import](docs/screenshots/18-blender-import.png) |

| Settings | Command palette | Path-traced still |
| --- | --- | --- |
| ![Settings](docs/screenshots/10-settings.png) | ![Palette](docs/screenshots/12-command-palette.png) | ![Ray traced](docs/screenshots/15-render-panel-raytraced.png) |

### Owner tool

| Passphrase gate | Issue codes | Ledger |
| --- | --- | --- |
| ![Gate](docs/screenshots/21-keyforge-gate.png) | ![Generate](docs/screenshots/19-keyforge-generate.png) | ![Ledger](docs/screenshots/20-keyforge-ledger.png) |

*(Every screenshot above is rendered from the real UI code by `astra_uipreview`, not mocked up.)*

## Viewport controls

| Input | Action |
| --- | --- |
| Right drag (or Alt + left drag) | Orbit the level camera |
| Middle drag | Pan |
| Wheel | Zoom (dolly towards the camera target) |
| Left click | Select the actor under the cursor (in the World Outliner too) |
| `F` | Frame the selected actor (or the whole level) |
| `Q` `W` `E` `R` | Select / Move / Rotate / Scale tool |
| `Delete` | Delete the selected actor |
| `Ctrl+D` | Duplicate the selected actor |
| `Ctrl+Z` / `Ctrl+Shift+Z` | Undo / redo (whole-level snapshots) |
| `Ctrl+S` | Save the level |
| `F5` / `Esc` | Play in editor / stop |

### Play mode

`F5` (or the ▶ transport button) spawns the pawn at the level's **Player Start** and hands it the
viewport. `WASD` walks, `Shift` sprints, `Space` jumps, right-drag looks around, `Esc` stops. The
HUD pill shows the session time; gizmos and actor icons are hidden while playing.

### Script, Render and World Settings

| Panel | How to open | What it does |
| --- | --- | --- |
| AstraScript workspace | `Ctrl+Enter` or the `</>` toolbar button | Edits and runs `.astra` scripts against the open level; shows console output and generated images |
| Render (path tracer) | camera toolbar button, or World Settings | Traces the level on the CPU with live stats (triangles, BVH nodes, rays, ms) and saves PNGs |
| World Settings | sliders toolbar button | Fog, cloud cover, rain, exposure, sun direction, viewport toggles, Blender bridge status |

The viewport header carries the view-mode picker (Shaded, Unlit, Wireframe, Normals) plus
toggles for grid, shadows and gizmos, and reports the CPU raster budget per frame. The bottom
drawer switches between the **Content Browser** (click an asset to place it) and the
**Output Log**.

## Access codes

Registration is gated by codes that only the studio owner can mint:

* 16 Crockford characters (`K7QM-3XPD-9RTA-B2VH`), signed with HMAC-SHA256 and fully
  verifiable offline.
* Each code carries its role (`User` / `Staff` / `Owner`) and lifetime (24 h … never) and is
  **single-use per install**.
* KeyForge requires an owner passphrase (PBKDF2, 200 000 iterations) on every launch, keeps
  an issue ledger, and exports batches to `.txt` / `.json`.

Details and the migration to Ed25519 public-key codes in 0.2: [docs/SECURITY.md](docs/SECURITY.md).

## Build

Windows 10/11 x64, Visual Studio 2022, CMake 3.20+:

```powershell
cmake --preset windows-msvc
cmake --build --preset windows-msvc-release
ctest --preset windows-msvc
pwsh tools/package_windows.ps1 -Configuration Release   # -> dist/astra-studio-<ver>-windows-x64.zip
```

Any OS with a C++17 compiler can build and run the headless tools:

```bash
cmake --preset headless && cmake --build --preset headless
./build/headless/astra_selfcheck
./build/headless/astra_uipreview --smoke
```

CI builds the Windows zip and uploads it as the `astra-studio-windows-x64` artifact on every
push. Step-by-step instructions and troubleshooting: [docs/BUILD_WINDOWS.md](docs/BUILD_WINDOWS.md).

## Repository layout

```
src/astra/core        types, strings, paths, file IO, time, logging
src/astra/crypto      SHA-256, HMAC, PBKDF2, base32, RNG, DPAPI wrapper
src/astra/data        JSON reader/writer
src/astra/licensing   access codes, master key, issue ledger
src/astra/auth        accounts, sessions, registration
src/astra/github      transport iface, WinHTTP, parsing, demo fixture
src/astra/render      orbit camera, CPU viewport rasteriser, picking, CPU path tracer
src/astra/scene       entities, components (transform/mesh/material/particles/light/camera/atmosphere), reflection
src/astra/asset       procedural mesh builders, OBJ import, Blender bridge inbox
src/astra/image       CPU image buffers: noise, blur, PNG read/write
src/astra/script      AstraScript tokenizer, parser, interpreter, host bindings
src/astra/game        first-person pawn used by play-in-editor
src/astra/project     astra.project.json, scene IO, recent projects
src/astra/ui          theme, icons, widget library, branding
src/astra/app         AstraApp flow, screens, editor panels, KeyForge
src/platform          GLFW + OpenGL3 window frontend
src/studio            AstraStudio.exe entry point
src/keygen            AstraKeyForge.exe entry point (GUI + --issue-code CLI)
src/tools             self-check suite and headless UI renderer
assets/               logo, fonts, demo workspace fixture
extern/               Dear ImGui 1.91.9b-docking, stb_image
docs/                 architecture, security, UI guide, build + release guides, roadmap, screenshots
tools/                packaging, screenshot capture, PNG recompression, Blender addon
```

## Documentation

* [docs/ARCHITECTURE.md](docs/ARCHITECTURE.md) — layers, threading, on-disk layout, verification strategy
* [docs/MATERIALS.md](docs/MATERIALS.md) — the `*.astramat` material format, hot reload and the Material Editor
* [docs/SCRIPTING.md](docs/SCRIPTING.md) — the AstraScript language, built-ins and examples
* [docs/BLENDER_BRIDGE.md](docs/BLENDER_BRIDGE.md) — installing the Blender addon and importing meshes
* [docs/SECURITY.md](docs/SECURITY.md) — access codes, accounts, secrets at rest, network surface
* [docs/UI_GUIDE.md](docs/UI_GUIDE.md) — design tokens, components, layout rules, motion
* [docs/BUILD_WINDOWS.md](docs/BUILD_WINDOWS.md) — prerequisites, presets, packaging, troubleshooting
* [docs/ROADMAP.md](docs/ROADMAP.md) — viewport milestones, gameplay, team workflow
* [docs/RELEASING.md](docs/RELEASING.md) — how every merge to `main` becomes a `v0.x` release

## Dependencies

Dear ImGui (MIT) and stb_image (public domain) are vendored under `extern/`. GLFW 3.4 (zlib
licence) is fetched by CMake on first configure. Everything else — crypto, JSON, HTTP, UI,
licensing — is implemented here. Fonts are Roboto (Apache-2.0) and Cousine (Apache-2.0); see
`assets/fonts/LICENSE.txt`.

## Licence

MIT — see [LICENSE](LICENSE). © 2026 Cyberwall / Astra Productions.
