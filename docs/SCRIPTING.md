# AstraScript

AstraScript is the engine's scripting language. It runs inside Astra Studio (the script workspace
or the Output Log console), it can be run headlessly against a level with `astra_selfcheck`, and
it is deliberately small enough that a level designer can learn it in an afternoon.

- **Script editor:** `Ctrl+Enter` (or the `</>` toolbar button) opens the script workspace. The
  left column lists every `.astra` file in the open project, the middle is the editor with a line
  gutter and syntax highlighting, the right is the console plus any images the script produced.
- **Repositories without an `astra.project.json`** open straight into the script workspace, so a
  code repo still gives you somewhere useful to be.
- **Running** saves the buffer, executes it against the open level, and leaves everything it
  spawned in the level. Running a script is a single undo step.

## Language

```astra
// comments run to the end of the line
/* and block comments work too */

let name = "Astra"          // `let` and `var` both create mutable locals
var count = 3
let total = count * 4 + 1   // 13

if (total > 10) {
    print("big:", total)
} else {
    print("small")
}

fn double(x) {
    return x * 2
}

for (i in range(5)) {        // also `for i in range(5)`
    if (i == 2) continue
    print(double(i))
}

let items = ["a", "b", "c"]
print(join(items, "-"), len(items))   // a-b-c 3
```

Values are `nil`, `bool`, `number`, `string`, `array` and a `mesh`/`image` handle (a small
integer). Operators: `+ - * / %`, comparisons, `&& || !`. Strings support `\n`, `\t` and `\\`.

## Built-in functions

| Group | Functions |
| --- | --- |
| Core | `print(...)`, `format(fmt, ...)` (`{}` placeholders), `str(x)`, `num(x)`, `len(v)`, `range(n)` |
| Math | `floor`, `ceil`, `round`, `abs`, `min`, `max`, `sqrt`, `pow`, `sin`, `cos`, `tan`, `clamp`, `lerp`, `random`, `PI`, `TAU` |
| Strings | `join(array, sep)`, `upper`, `lower`, `substr`, `trim`, `split(text, sep)` |
| Scene | `actorCount()`, `actorName(id)`, `findActor(name)`, `spawnActor(name, shape, x, y, z)`, `destroyActor(id)`, `getPosition(id)`, `setPosition`, `setRotation`, `setScale`, `setColor`, `getTag`, `setTag`, `setMaterial(id, shader, r, g, b)` |
| Mesh | `meshCube()`, `meshSphere(segments, rings)`, `meshCylinder(segments)`, `meshCone(segments)`, `meshPlane(divisions)`, `meshTorus(...)`, `meshVertex(mesh, x, y, z)`, `meshTriangle(mesh, a, b, c)`, `meshRecenter(mesh)`, `meshFitUnit(mesh)`, `meshRegister(mesh, name)` → `meshId`, `attachMesh(actor, meshId)`, `meshCount()` |
| Image | `imageNew(w, h)`, `imageWidth`, `imageHeight`, `imageFill`, `imageSet`, `imageGet`, `imageRect`, `imageCircle`, `imageLine`, `imageGradient`, `imageChecker`, `imageNoise`, `imageBlur`, `imageGamma`, `imageSave(img, path)`, `imageLoad(path)` |
| IO | `readFile(path)`, `writeFile(path, text)`, `fileExists(path)` |
| Time | `time()` |

Relative paths resolve against the script's folder, so a script can ship next to the images it
writes.

## Example: author a mesh and a row of pillars

```astra
let mesh = meshCylinder(20)
meshRecenter(mesh)
let id = meshRegister(mesh, "Script pillar")

for (i in range(8)) {
    let angle = i * TAU / 8.0
    let actor = spawnActor(format("Pillar {}", i), "custom", cos(angle) * 6.0, 1.5, sin(angle) * 6.0)
    attachMesh(actor, id)
    setScale(actor, 0.7, 3.0, 0.7)
}
print("actors:", actorCount())
```

## Example: bake a texture from code

```astra
let img = imageNew(128, 128)
imageGradient(img, 0.1, 0.2, 0.5, 0.9, 0.6, 0.2)
imageNoise(img, 0.06)
imageCircle(img, 64, 64, 28, 1.0, 0.95, 0.6)
imageBlur(img, 1)
imageSave(img, "baked/terrain.png")
```

## Running headlessly

```bash
./build/astra_selfcheck --filter script        # language + host coverage
```

The `script.language` test parses and runs a script that exercises every built-in group, including
a mesh registration and a PNG round-trip, so the language cannot silently regress.
