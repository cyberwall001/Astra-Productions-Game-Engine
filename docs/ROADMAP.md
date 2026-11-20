# Astra Engine roadmap

Milestone 1 (this repository state) is the **studio shell**: a real Windows application with
a loading screen, boot sequence, persistent sign-in, access-code registration, a GitHub-backed
project hub and the editor frame — with a deliberately empty viewport.

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

## Milestone 2 — Viewport and renderer

- [ ] D3D12 backend (Vulkan behind a flag), render-graph skeleton, GPU timing.
- [ ] Viewport panel: render to texture, orbit camera, grid, gizmos, selection outline.
- [ ] Sky + volumetric cloud pass driven by the existing weather fields.
- [ ] Prefab system and content browser over `assets/` and `content/`.
- [ ] Asset import pipeline (FBX/glTF via assimp, textures via stb/basis).
- [ ] Undo/redo command stack for scene edits.
- [ ] Ed25519 access codes: Studio keeps only the public key.

## Milestone 3 — Storm-chasing gameplay systems

- [ ] Deterministic fixed-step simulation + replay files.
- [ ] Weather simulation: supercell lifecycle, hail, wind, road wetness.
- [ ] Vehicle + chase-cam controller; player HUD.
- [ ] Scenario scripting (timeline + triggers) and a scenario editor.
- [ ] Packaging: build and ship a game from the editor (`Build → Windows x64`).

## Milestone 4 — Team workflow

- [ ] Accounts backed by the studio server (optional; offline stays the default).
- [ ] Repo-aware collaboration: branch ↔ scene diff, sign-off, review notes.
- [ ] Crash reporting (opt-in, self-hosted), telemetry-free by default.

## Deliberate non-goals

- Importing Unity/Unreal projects.
- A node graph for weather authoring (the field model is data-driven instead; revisit only if
  designers ask for it).
- Web/console targets before Windows is excellent.
