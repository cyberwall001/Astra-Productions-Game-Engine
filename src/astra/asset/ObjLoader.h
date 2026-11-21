// asset/ObjLoader.h - Wavefront OBJ import.
//
// OBJ is the interchange format the Blender bridge writes, so this is deliberately small:
// v / vn / vt / f (with negative indices and n-gons), plus `o` and `g` for naming. Materials
// stay out of it - the importer reports what it dropped so the UI can say so.
#pragma once

#include <string>
#include <string_view>
#include <vector>

#include "astra/asset/MeshBuilder.h"

namespace astra::asset {

struct ObjLoadResult {
    bool ok = false;
    MeshAsset mesh;
    std::string error;
    std::string warning;
    int objects = 0;
    int groups = 0;
    int faces = 0;
    int ignoredMaterialLines = 0;
};

// Parses OBJ text. `name` is used when the file declares no object name.
ObjLoadResult ParseObj(std::string_view text, std::string name = "Imported mesh");

// Reads a file and parses it. `fitToUnitBox` normalises imported scale so a Blender model
// drops into the level at a sane size; `recenter` moves the pivot to the model centre.
ObjLoadResult LoadObjFile(const std::string& path, bool fitToUnitBox = true,
                          bool recenter = true);

}  // namespace astra::asset
