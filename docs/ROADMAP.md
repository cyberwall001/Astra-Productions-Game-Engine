# Astra Engine roadmap

Milestone 1 shipped the **studio shell**; milestone 2 added the **level viewport** — a CPU
rasteriser with orbit navigation, static meshes, view modes, picking and a Content Browser.
Milestone 3 (this repository state) makes it a usable engine: **play in editor**, **AstraScript**,
an **offline path tracer**, and a **Blender bridge**. The engine is a general-purpose toolchain
again: weather is one optional system, not the premise.

## Milestone 1 — Studio shell ✅

- [x] Own C++17 engine skeleton: crypto, JSON, filesystem, time, logging, paths.
- [x] Access-code licensing (offline HMAC codes, single-use per install, role + duration).
- [x] Owner tool (`AstraKeyForge.exe`) with passphrase gate, batch minting, ledger, exports.
- [x] Local account registry: PBKDF2 accounts, lockout, remember-me sessions.
- [x] Loading screen → boot sequence → sign-in → registration → project hub → editor.
- [x] GitHub repository sync (WinHTTP) with DPAPI-protected token and a demo fixture.
- [x] Scene model with weather components, storm template, reflection-driven inspector.
- [x] Editor shell: hierarchy, inspector, viewport placeholder, output log, statistics,
      command palette, settings, toasts, keyboard shortcuts.
- [x] Headless verification: 164-assertion self-check + ImGui smoke test + PNG capture.
- [x] Windows packaging script and CI artifact.

## Milestone 2 — Level viewport ✅

- [x] Level viewport with an orbit camera: orbit / pan / zoom / frame, `Q W E R` tools.
- [x] Procedural static meshes (cube, sphere, cylinder, cone, plane) with a Mesh component,
      JSON round-trip and Details editing.
- [x] CPU rasteriser: near-plane clipping, painter sort, hemisphere ambient + sun + fog + rim.
- [x] View modes (Shaded / Unlit / Wireframe / Normals), fading world grid, ground shadows,
      actor icons, selection box, axis gizmo.
- [x] Screen-space picking, click-to-select from the viewport or the outliner.
- [x] Unreal-style shell: World Outliner · Level Viewport · Details · tabbed Content Browser /
      Output Log, play/simulate transport, live draw statistics.
- [x] Neutral sandbox level by default; the storm rig survives as a template.
- [x] Verified headlessly: 216-assertion self-check (including the renderer and picking).

## Milestone 3 — Playable, scriptable, importable ✅

- [x] First-person pawn (`game::Player`): WASD, sprint, jump, gravity, mouse-look, eye height.
- [x] Play in editor (`F5` / transport): spawns at the Player Start, possesses the viewport,
      hides editor chrome, shows a session HUD; `Esc` stops.
- [x] AstraScript: tokenizer, parser, interpreter (locals, loops, functions, arrays) plus a host
      API for scene, mesh, image, IO and math — 60+ built-ins.
- [x] Script workspace: file column, highlighted editor with a line gutter, run/save shortcuts,
      console, generated-image preview. Non-Astra repositories open here instead of erroring.
- [x] CPU path tracer: BVH, soft sun shadows, 4-ray AO, metallic reflections, Russian roulette,
      ACES tonemap, denoise, multi-threaded, `Render` panel with live stats and PNG export.
- [x] Volumetric fog: single-scattering height fog ray-marched through the level with sun
      shadow rays per step (god rays), driven by the world settings' fog density.
- [x] Baked-scene cache: the tracer rebuilds its BVH only when the level's content hash changes.
- [x] Blender bridge: folder-contract addon (`tools/blender/astra_bridge.py`), OBJ + sidecar
      inbox, one-click import into the mesh library, status heartbeat.
- [x] Mesh/material/particle data layer with a scene mesh library, eight material shaders and
      five particle presets, all editable from Details.
- [x] Undo/redo, duplicate and delete across the whole editor (level snapshots).
- [x] Rendering: soft sun-aware ground shadows, volumetric light shafts, parallax cloud deck,
      height fog, per-vertex surface shading, alpha.
- [x] Verified headlessly: 21 suites / 437 assertions, plus `astra_uipreview` interaction tests
      and screenshots for play mode, the render panel, the script workspace and the bridge.

## Milestone 4 — GPU renderer and content pipeline

- [ ] D3D12 backend (Vulkan behind a flag), render-graph skeleton, GPU timing.
- [ ] Viewport renders to a texture with MSAA; the CPU rasteriser stays as a fallback.
- [ ] Sky + volumetric cloud pass on the GPU (the CPU volumetrics shipped in milestone 3).
- [ ] GPU path tracer behind the same `TraceSettings` contract; the CPU tracer stays the
      fallback and keeps reporting itself as unavailable on GPU-less builds.
- [x] Material assets (`*.astramat`) with hot reload and a Material Editor window (milestone 3).
- [ ] Prefab system and content browser backed by `assets/` and `content/`.
- [ ] Asset import pipeline (FBX/glTF via assimp, textures via stb/basis).
- [ ] Ed25519 access codes: Studio keeps only the public key.

## Milestone 5 — Gameplay systems

- [ ] Deterministic fixed-step simulation + replay files.
- [ ] Weather simulation: supercell lifecycle, hail, wind, road wetness.
- [ ] Vehicle + chase-cam controller; player HUD.
- [ ] Scenario scripting (timeline + triggers) and a scenario editor.
- [ ] Packaging: build and ship a game from the editor (`Build → Windows x64`).

## Milestone 6 — Team workflow

- [ ] Accounts backed by the studio server (optional; offline stays the default).
- [ ] Repo-aware collaboration: branch ↔ scene diff, sign-off, review notes.
- [ ] Crash reporting (opt-in, self-hosted), telemetry-free by default.

## Deliberate non-goals

- Importing Unity/Unreal projects.
- A node graph for weather authoring (the field model is data-driven instead; revisit only if
  designers ask for it).
- Web/console targets before Windows is excellent.
