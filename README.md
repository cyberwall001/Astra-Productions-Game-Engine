# Astra Productions Game Engine

[![Latest release](https://img.shields.io/github/v/release/cyberwall001/Astra-Productions-Game-Engine?label=release&color=FF5F5A)](https://github.com/cyberwall001/Astra-Productions-Game-Engine/releases/latest)
[![Windows build](https://github.com/cyberwall001/Astra-Productions-Game-Engine/actions/workflows/windows.yml/badge.svg)](https://github.com/cyberwall001/Astra-Productions-Game-Engine/actions/workflows/windows.yml)

**Download:** every merge to `main` publishes a fresh `v0.x` build — grab
`astra-studio-<version>-windows-x64.zip` from
[Releases](https://github.com/cyberwall001/Astra-Productions-Game-Engine/releases/latest).

A storm-chasing game engine and studio, written from scratch in C++17 for Windows.
No Unity, no Unreal, no wrapper: the window layer, crypto, account registry, offline
access-code licensing, GitHub project hub and the whole UI toolkit live in this repository.

**Milestone 1 — studio shell.** The application boots, signs in, registers accounts with
one-time access codes, lists the repositories you have access to and opens the editor frame
with a live scene graph. The game viewport is intentionally not wired up yet.

![Astra Studio editor](docs/screenshots/09-editor.png)

## What is in the box

| | |
| --- | --- |
| **Astra Studio** (`AstraStudio.exe`) | loading screen → boot sequence → sign-in → registration → project hub → editor |
| **Astra KeyForge** (`AstraKeyForge.exe`) | owner-only tool that mints single-use engine access codes |
| **Headless tools** | `astra_selfcheck` (164 assertions) and `astra_uipreview` (renders every screen to PNG) |

### Launcher flow

| Loading | Boot sequence | Sign in |
| --- | --- | --- |
| ![Loading](docs/screenshots/01-boot-loading.png) | ![Boot](docs/screenshots/02-boot-sequence.png) | ![Sign in](docs/screenshots/03-sign-in.png) |

| Access code | Registration | Project hub |
| --- | --- | --- |
| ![Access code](docs/screenshots/04-register-code.png) | ![Account](docs/screenshots/06-register-account.png) | ![Hub](docs/screenshots/07-project-hub.png) |

### Studio

| Editor | Settings | Command palette |
| --- | --- | --- |
| ![Editor](docs/screenshots/09-editor.png) | ![Settings](docs/screenshots/10-settings.png) | ![Palette](docs/screenshots/12-command-palette.png) |

### Owner tool

| Passphrase gate | Issue codes | Ledger |
| --- | --- | --- |
| ![Gate](docs/screenshots/13-keyforge-gate.png) | ![Generate](docs/screenshots/14-keyforge-generate.png) | ![Ledger](docs/screenshots/15-keyforge-ledger.png) |

*(Every screenshot above is rendered from the real UI code by `astra_uipreview`, not mocked up.)*

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
src/astra/scene       entities, components, storm presets, reflection
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
tools/                packaging, screenshot capture, PNG recompression
```

## Documentation

* [docs/ARCHITECTURE.md](docs/ARCHITECTURE.md) — layers, threading, on-disk layout, verification strategy
* [docs/SECURITY.md](docs/SECURITY.md) — access codes, accounts, secrets at rest, network surface
* [docs/UI_GUIDE.md](docs/UI_GUIDE.md) — design tokens, components, layout rules, motion
* [docs/BUILD_WINDOWS.md](docs/BUILD_WINDOWS.md) — prerequisites, presets, packaging, troubleshooting
* [docs/ROADMAP.md](docs/ROADMAP.md) — viewport, renderer, gameplay, team workflow
* [docs/RELEASING.md](docs/RELEASING.md) — how every merge to `main` becomes a `v0.x` release

## Dependencies

Dear ImGui (MIT) and stb_image (public domain) are vendored under `extern/`. GLFW 3.4 (zlib
licence) is fetched by CMake on first configure. Everything else — crypto, JSON, HTTP, UI,
licensing — is implemented here. Fonts are Roboto (Apache-2.0) and Cousine (Apache-2.0); see
`assets/fonts/LICENSE.txt`.

## Licence

MIT — see [LICENSE](LICENSE). © 2026 Cyberwall / Astra Productions.
