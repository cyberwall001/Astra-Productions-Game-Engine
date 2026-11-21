# Astra Engine architecture

Astra Engine is a from-scratch C++17 engine and studio aimed at storm-chasing games.
Nothing in this repository wraps an off-the-shelf engine: the window layer, crypto,
account registry, access-code licensing, GitHub integration, scene model and the whole
UI toolkit are written here. Dear ImGui is used only as an immediate-mode UI *drawing*
library; every widget the user sees (fields, cards, segmented controls, steppers,
sparklines, vector icons) is implemented in `src/astra/ui`.

Milestone 1 shipped the **studio shell** (launcher, accounts, project hub, editor layout),
milestone 2 the **level viewport**, and milestone 3 the **runtime view**: play-in-editor,
AstraScript, an offline CPU path tracer and the Blender bridge. See [ROADMAP.md](ROADMAP.md).

## Layers

```
┌─────────────────────────────────────────────────────────────────────┐
│ src/studio/main.cpp        src/keygen/main.cpp        (entry points) │
├─────────────────────────────────────────────────────────────────────┤
│ src/platform/GlfwApplication.cpp   GLFW window + GL3 backends        │
├─────────────────────────────────────────────────────────────────────┤
│ src/astra/app        AstraApp (launcher/editor flow), KeyForgeApp    │
│ src/astra/app/screens  Boot, SignIn, Register, ProjectHub, Editor,   │
│                        EditorPanels, EditorWindows, ScriptEditor,    │
│                        Overlays                                      │
├─────────────────────────────────────────────────────────────────────┤
│ src/astra/ui         Theme, Icons, Widgets, Branding                 │
├─────────────────────────────────────────────────────────────────────┤
│ src/astra/script     AstraScript tokenizer, parser, interpreter, host │
│ src/astra/render     orbit camera, CPU rasteriser, picking, tracer   │
│ src/astra/asset      mesh builders, OBJ import, Blender bridge, materials │
│ src/astra/image      CPU image buffers, noise, blur, PNG IO          │
│ src/astra/game       first-person pawn (play in editor)              │
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
* The path tracer splits scanlines across `std::thread` workers (hardware-concurrency, capped)
  and returns a finished `image::Buffer`; nothing in the renderer touches scene state while
  tracing, so a trace can never tear the level.
* Nothing else is concurrent. The rasteriser stays single-threaded on purpose: every frame is
  drawn into the ImGui draw list, which is not thread-safe.

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
| `engine/bridge/inbox/` | OBJ + JSON jobs written by the Blender addon |
| `engine/bridge/imported/` | jobs already imported (move-aside, never re-imported) |
| `engine/bridge/status.json` | Blender heartbeat: version, scene name, timestamp |
| `engine/renders/` | path-traced stills saved from the Render panel |

A project folder always contains `astra.project.json`, `scenes/`, `assets/`, `content/`,
and `materials/` (authored `*.astramat` files; seeded on first open and hot reloaded).
New projects open on the **storm template**: `Chase Environment → {Chase Cam, Key Light,
Supercell 03 / Mesocyclone → {Wall Cloud, Vortex Core, Precipitation Curtain}}, Chase
Vehicle`. A project is never an empty viewport on first launch.

## Render module (`src/astra/render`)

There is no GPU pass in this milestone. The viewport rasterises on the CPU straight into the
ImGui draw list, which keeps one code path for the GLFW build and the headless preview tool and
makes the editor fully screenshot-testable.

```
Camera.h/.cpp        orbit camera: target/yaw/pitch/distance/fov, Orbit/Pan/Zoom/Frame, Basis()
Math.h               Vec3 maths, Euler rotation, perspective helpers
ViewportTypes.h/.cpp ViewMode + Options + Stats (ImGui-free, so AstraApp can own them)
ViewportRenderer.h   Rect, ActorBounds, Draw, Pick, FrameSelection
ViewportRenderer.cpp procedural meshes, projector, clipping, painter sort, shading, chrome
```

`Draw()` is one pass: read the environment (sun direction/colour from a Directional Light,
sky and fog from a Sky Atmosphere), build a `Projector` from the camera, emit the sky and the
fading ground grid, transform and shade every visible mesh, clip each triangle against the near
plane (Sutherland–Hodgman), painter-sort the surviving triangles by depth and push them into the
draw list through `PrimReserve`/`PrimVtx`. Chrome — ground shadows, actor icons, the weather
volume outline, the selection box and the axis gizmo — is drawn on top.

Meshes are procedural (`MeshShape::Cube | Sphere | Cylinder | Cone | Plane`) with per-axis
scale, albedo, visibility, shadow and two-sided flags. A `MeshComponent` is just data, so a
level round-trips through JSON with its geometry intact and the Details panel edits it through
the same reflection model as every other component.

`Pick()` is screen-space: it tests the mouse against each actor's projected bounding circle and
returns the nearest hit, which is enough for box and primitive selection and needs no depth
buffer. `FrameSelection()` reuses `Camera::Frame`, so `F` behaves like the Unreal focus key.

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
