# Astra Engine roadmap

Milestone 1 shipped the **studio shell**; milestone 2 (this repository state) adds the **level
viewport** — a CPU rasteriser with orbit navigation, static meshes, view modes, picking and a
Content Browser. The engine is a general-purpose toolchain again: weather is one optional
system, not the premise.

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

## Milestone 3 — GPU renderer and content pipeline

- [ ] D3D12 backend (Vulkan behind a flag), render-graph skeleton, GPU timing.
- [ ] Viewport renders to a texture with MSAA; the CPU rasteriser stays as a fallback.
- [ ] Sky + volumetric cloud pass driven by the existing weather fields.
- [ ] Prefab system and content browser backed by `assets/` and `content/`.
- [ ] Asset import pipeline (FBX/glTF via assimp, textures via stb/basis).
- [ ] Undo/redo command stack for scene edits.
- [ ] Ed25519 access codes: Studio keeps only the public key.

## Milestone 4 — Gameplay systems

- [ ] Deterministic fixed-step simulation + replay files.
- [ ] Weather simulation: supercell lifecycle, hail, wind, road wetness.
- [ ] Vehicle + chase-cam controller; player HUD.
- [ ] Scenario scripting (timeline + triggers) and a scenario editor.
- [ ] Packaging: build and ship a game from the editor (`Build → Windows x64`).

## Milestone 5 — Team workflow

- [ ] Accounts backed by the studio server (optional; offline stays the default).
- [ ] Repo-aware collaboration: branch ↔ scene diff, sign-off, review notes.
- [ ] Crash reporting (opt-in, self-hosted), telemetry-free by default.

## Deliberate non-goals

- Importing Unity/Unreal projects.
- A node graph for weather authoring (the field model is data-driven instead; revisit only if
  designers ask for it).
- Web/console targets before Windows is excellent.
