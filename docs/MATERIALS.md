# Material assets

Astra materials are plain-text files (`*.astramat`) that live in `<project>/materials`. They
are ordinary text on purpose: any editor can open them, git can diff them, and the studio
watches them for changes so a saved file shows up in the viewport without a restart.

## File format

```ini
# Astra material asset - edit in any text editor, the studio hot reloads it.
name = Brushed Steel
shader = Lit                     # Lit, Unlit, Checker, Stripes, Noise, Gradient, Hologram, Emissive
baseColor = 0.620 0.640 0.680    # linear RGB, 0..1
accentColor = 0.300 0.320 0.360  # second colour used by the patterned shaders
uvScale = 4
roughness = 0.32                 # 0.02 .. 1
metallic = 0.9                   # 0 .. 1
emissive = 0                     # 0 .. 4
opacity = 1
fresnelPower = 3
```

Rules the loader follows:

- `#` starts a comment; blank lines are ignored.
- `key = value` and `key: value` both work.
- Unknown keys are ignored, so a file written by a newer build still loads.
- Missing keys keep their defaults; out-of-range numbers are clamped, not rejected.
- A file that cannot be parsed is skipped with a warning in the Output Log — it never takes
  the rest of the library down with it.

When a project has no `materials` folder, the engine seeds six starter materials (Brushed
Steel, Painted Concrete, Hologram Panel, Warning Stripes, Emissive Cyan, Tile Checker) so the
picker is never empty.

## Where to edit them

- **Tools ▸ Material editor** — list, live preview, fields, `Save`, `Assign to selection`,
  `Reload`. Saving writes the file; everything else re-reads it.
- **Any text editor** — save the file and the editor picks it up within a second (hot reload),
  no restart and no reload button needed.
- **AstraScript** — `materialNames()` lists them, `applyMaterial(actor, name)` assigns one.

## Library API

```cpp
#include "astra/asset/Material.h"

astra::asset::MaterialLibrary library;
library.Scan(astra::asset::MaterialLibrary::DefaultDirectory());  // or <project>/materials
library.ReloadChanged();                                          // returns reloaded count
astra::asset::MaterialAsset& steel = *library.FindMutable("Brushed Steel");
steel.roughness = 0.5f;
library.Save(steel);                                              // writes the .astramat
astra::asset::MaterialLibrary::ApplyTo(scene, actorId, "Brushed Steel", library);
```

The library is loaded when a project opens and polled every 0.75 s in the editor.
