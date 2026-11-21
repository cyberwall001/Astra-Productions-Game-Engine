// asset/MeshBuilder.h - procedural geometry shared by the renderer, the importer and the
// scripting host.
//
// Everything is generated as a scene::MeshAsset so there is exactly one mesh type in the
// engine: primitives, imported OBJ files and meshes assembled from scripts all take the
// same path through the viewport, the ray tracer and the JSON writer.
#pragma once

#include <string>

#include "astra/scene/Scene.h"

namespace astra::asset {

using scene::MeshAsset;
using scene::MeshShape;

MeshAsset MakeCube();
MeshAsset MakeSphere(int segments = 24, int rings = 14);
MeshAsset MakeCylinder(int segments = 24);
MeshAsset MakeCone(int segments = 24);
MeshAsset MakePlane(int divisions = 8);
MeshAsset MakeTorus(int segments = 28, int rings = 14, float tube = 0.28f);
// A single triangle - the smallest thing a script can hand to the mesh library.
MeshAsset MakeTriangle();

// Cached primitive for a built-in shape. Custom meshes live in the scene instead, so the
// caller resolves those itself.
const MeshAsset& Primitive(MeshShape shape);

// Scales/normalises a mesh into a unit box so actor scale keeps its usual meaning.
void FitToUnitBox(MeshAsset& mesh);
void Recenter(MeshAsset& mesh);

}  // namespace astra::asset
