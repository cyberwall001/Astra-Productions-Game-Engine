# Blender bridge

Astra Studio talks to Blender through a **folder contract** instead of a socket. That is a
deliberate choice:

- it works with any Blender version (2.90 → 4.x) without shipping a compiled addon,
- Blender can run on another machine and write to a shared folder,
- nothing breaks if either side is closed, and
- it is fully testable headlessly, which is how CI verifies it.

```
Blender ──"Send selection to Astra"──►  <engine data>/engine/bridge/inbox/*.obj + *.json
                                                    │
                          Astra Studio ──"Tools ▸ Import from Blender"──►  mesh library + actor
```

Imported files move to `bridge/imported/`, so the same mesh is never imported twice.

## Where the inbox lives

| OS | Path |
| --- | --- |
| Windows | `%LOCALAPPDATA%\Astra Productions\engine\bridge\inbox` |
| Linux | `$XDG_DATA_HOME/astra-productions/engine/bridge/inbox` (usually `~/.local/share/...`) |
| macOS | `~/Library/Application Support/Astra Productions/engine/bridge/inbox` |

`ASTRA_DATA_DIR` overrides the whole tree for both sides, which is handy for tests and portable
installs. The panel in Blender prints the resolved path, and Astra shows it in
`Tools ▸ Blender bridge folder...`.

## Installing the addon

1. `Edit ▸ Preferences ▸ Add-ons ▸ Install...` and pick `tools/blender/astra_bridge.py`.
2. Tick **Astra Productions Bridge**.
3. Open the 3D View sidebar (`N`) and switch to the **Astra** tab.

## Sending meshes

1. Select one or more mesh objects.
2. Choose the material the imported actor should use (`Lit`, `Checker`, `Hologram`, `Emissive`).
3. Press **Send selection to Astra**.

The exporter writes a selected-only OBJ (UVs and normals included, modifiers applied, `-Z`/`Y`
axis convention) plus a small sidecar:

```json
{
  "name": "Crate",
  "material": "Hologram",
  "source": "blender",
  "blender": "4.2.1",
  "objects": ["Crate", "Crate_lid"],
  "triangles": 312,
  "placeholder": false
}
```

A sidecar with `"placeholder": true` marks a job that is still being written (or an aborted
export). The engine skips those instead of importing half a mesh.

**Send test cube** creates and sends a cube in one click, so you can prove the wiring before
touching a real scene. **Write status** publishes `bridge/status.json`, which is what the
*Blender* section of Astra's World Settings window reads (`Connected - Blender 4.2.1 / Level`).

## Importing in Astra

- `Tools ▸ Import from Blender` imports everything waiting in the inbox.
- The toolbar's **World Settings** window has a Blender section with the connection status, the
  number of waiting meshes, and an **Import from Blender** button (enabled only when something is
  waiting).
- Each mesh joins the level's mesh library and spawns its own actor, tagged `blender`, laid out
  in a ring so a batch import does not stack.
- The import is one undo step, so `Ctrl+Z` backs the whole batch out.

## Running Blender headlessly

```bash
blender --background my_scene.blend --python tools/blender/astra_bridge.py -- --send
```

`--send` exports the current selection once and exits, which makes the bridge scriptable in CI.

## Verifying it without Blender

```bash
./build/astra_selfcheck --filter bridge
```

`bridge.blender` writes a mesh into the inbox with the same writer the addon uses, imports it into
a scene, and checks the actor, the mesh library, the hologram material, the move-aside behaviour,
placeholder skipping and malformed-OBJ handling. `astra_uipreview` also drives a real import and
captures `docs/screenshots/18-blender-import.png`.
