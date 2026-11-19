# Astra Engine architecture

Astra Engine is a from-scratch C++17 engine and studio aimed at storm-chasing games.
Nothing in this repository wraps an off-the-shelf engine: the window layer, crypto,
account registry, access-code licensing, GitHub integration, scene model and the whole
UI toolkit are written here. Dear ImGui is used only as an immediate-mode UI *drawing*
library; every widget the user sees (fields, cards, segmented controls, steppers,
sparklines, vector icons) is implemented in `src/astra/ui`.

Milestone 1 ships the **studio shell**: launcher, accounts, project hub, editor layout.
There is deliberately no game viewport yet — see [ROADMAP.md](ROADMAP.md).

## Layers

```
┌─────────────────────────────────────────────────────────────────────┐
│ src/studio/main.cpp        src/keygen/main.cpp        (entry points) │
├─────────────────────────────────────────────────────────────────────┤
│ src/platform/GlfwApplication.cpp   GLFW window + GL3 backends        │
├─────────────────────────────────────────────────────────────────────┤
│ src/astra/app        AstraApp (launcher/editor flow), KeyForgeApp    │
│ src/astra/app/screens  Boot, SignIn, Register, ProjectHub, Editor,   │
│                        EditorPanels, Overlays                        │
├─────────────────────────────────────────────────────────────────────┤
│ src/astra/ui         Theme, Icons, Widgets, Branding                 │
├─────────────────────────────────────────────────────────────────────┤
│ src/astra/scene      entities, components, storm presets, reflection │
│ src/astra/project    astra.project.json, scene IO, recent projects   │
│ src/astra/github     transport iface, WinHTTP, demo fixture          │
│ src/astra/auth       accounts, sessions, registration                │
│ src/astra/licensing  access codes, master key, issue ledger          │
│ src/astra/crypto     SHA-256, HMAC, PBKDF2, base32, RNG, DPAPI       │
│ src/astra/data       JSON reader/writer                              │
│ src/astra/core       types, strings, paths, file IO, time, log       │
└─────────────────────────────────────────────────────────────────────┘
```

Dependencies point downward only. `core` knows nothing about UI; `ui` knows nothing
about GitHub; `app` orchestrates.

## Threading

* UI and scene edits run on the main thread, inside `AstraApp::Draw`.
* GitHub fetches run on one worker thread (`github::RepoFetcher`). Results are published
  as an immutable `RepoSnapshot` and collected by `Poll()` on the UI thread, so no lock is
  ever held while drawing.
* There is no other concurrency. It is a deliberate constraint for milestone 1: the
  renderer and the asset pipeline will introduce their own job system later.

## Data on disk

Everything lives under `%LOCALAPPDATA%\Astra Productions\` (override with
`ASTRA_DATA_ROOT`, assets with `ASTRA_ASSETS_DIR`):

| Path | Contents |
| --- | --- |
| `engine/settings.json` | UI scale, density, GitHub login, recent projects, demo workspace flag |
| `engine/accounts.json` | local account registry (PBKDF2 hashes, roles, masked access codes) |
| `engine/session.json` | remember-me token + expiry, otherwise absent |
| `engine/licensing/used_codes.json` | codes redeemed on this machine (single-use enforcement) |
| `engine/recent_projects.json` | hub sidebar "Recent projects" |
| `engine/workspace/<repo>/` | project folder materialised from a GitHub repository |
| `engine/projects/<name>/` | local projects created in the hub |
| `keyforge/owner.json` | KeyForge owner passphrase (salt + PBKDF2 hash) — owner tool only |
| `keyforge/issued_codes.json` | the issue ledger |
| `keyforge/exports/` | batch exports (`codes-<stamp>.txt` / `.json`) |

A project folder always contains `astra.project.json`, `scenes/`, `assets/`, `content/`.
New projects open on the **storm template**: `Chase Environment → {Chase Cam, Key Light,
Supercell 03 / Mesocyclone → {Wall Cloud, Vortex Core, Precipitation Curtain}}, Chase
Vehicle`. A project is never an empty viewport on first launch.

## Scene model

`scene::Entity` carries a component bitmask (`Transform`, `Weather`, `Camera`, `Light`,
`Atmosphere`). Weather fields are plain data: storm preset, thermodynamic scores,
tornado probability, inflow-layer shear, hook-echo flags. The same data drives the
inspector, the console `weather` command and (in 0.2) the renderer.

`scene::BuildInspectorModel(entity)` is the reflection seam: it turns a component set into
labelled property groups (`Identity`, `Transform`, `Camera`, `Light`, `Weather Field`,
`Atmosphere`, …) with typed descriptors. The inspector is completely data-driven — adding
a component means adding a group builder, not editing UI code.

## UI architecture

* `ui::Theme` — palette, metrics (`U()` scaling), fonts, ImGui style.
* `ui::Widgets` — the component library (see [UI_GUIDE.md](UI_GUIDE.md)).
* `app::screens::Editor` — layout math only: menu bar, toolbar, panel rectangles, status
  bar, shortcuts. Panel content lives in `EditorPanels.cpp`.
* `app::screens::Overlays` — root window, screen routing, transition curtain, settings
  modal, toast host, command palette.

The root window is a single borderless full-screen window; every "panel" is a child window
placed by explicit rectangle math. That keeps layout deterministic (no docking surprises),
keeps hit-testing simple, and makes the headless renderer able to capture exact frames.

## Verification without a GPU

Because the shipped target is Windows and CI must still verify the UI, `tools/` contains:

* `astra_selfcheck` — 164 assertions across crypto vectors (SHA-256, HMAC, PBKDF2),
  base32/Crockford round-trips, JSON, access-code lifecycle, account/registration flows,
  scene round-trip, project IO, GitHub parsing, settings persistence.
* `astra_uipreview` — builds a real ImGui context, drives a real `AstraApp` into every
  screen, and rasterises the draw data with `SoftRaster` (CPU, bilinear, scissor-correct)
  into `framebuffer → PNG`. `--smoke` runs every screen at three window sizes and fails on
  any ImGui assertion or unbalanced style/font stack.

`tools/capture_screenshots.sh` regenerates `docs/screenshots` from the same code path the
.exe uses, which is how the images in the README stay honest.
