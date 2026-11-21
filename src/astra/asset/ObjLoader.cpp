#include "astra/asset/ObjLoader.h"

#include <cstdlib>
#include <sstream>

#include "astra/core/FileIO.h"

namespace astra::asset {
namespace {

using scene::Vec3;

struct Corner {
    int position = -1;
    int normal = -1;
    int uv = -1;
};

int ResolveIndex(int raw, std::size_t count) {
    if (raw > 0) return raw - 1;
    if (raw < 0) return static_cast<int>(count) + raw;
    return -1;
}

// Splits "v/vt/vn" (any part may be missing).
Corner ParseCorner(const std::string& token) {
    Corner corner;
    int part = 0;
    std::string buffer;
    for (std::size_t i = 0; i <= token.size(); ++i) {
        const bool end = i == token.size() || token[i] == '/';
        if (end) {
            if (!buffer.empty()) {
                const int value = std::atoi(buffer.c_str());
                if (part == 0) corner.position = value;
                else if (part == 1) corner.uv = value;
                else corner.normal = value;
            }
            buffer.clear();
            ++part;
            continue;
        }
        buffer.push_back(token[i]);
    }
    return corner;
}

}  // namespace

ObjLoadResult ParseObj(std::string_view text, std::string name) {
    ObjLoadResult result;
    std::vector<Vec3> positions;
    std::vector<Vec3> normals;
    std::vector<float> uvs;  // packed u,v

    struct Triangle {
        Corner corners[3];
    };
    std::vector<Triangle> triangles;
    std::string currentName;

    std::istringstream stream{std::string(text)};
    std::string line;
    while (std::getline(stream, line)) {
        if (!line.empty() && line.back() == '\r') line.pop_back();
        if (line.size() < 2) continue;
        if (line[0] == '#') continue;

        std::istringstream parts(line);
        std::string tag;
        parts >> tag;
        if (tag == "v") {
            float x = 0, y = 0, z = 0;
            parts >> x >> y >> z;
            positions.emplace_back(x, y, z);
        } else if (tag == "vn") {
            float x = 0, y = 0, z = 0;
            parts >> x >> y >> z;
            normals.emplace_back(x, y, z);
        } else if (tag == "vt") {
            float u = 0, v = 0;
            parts >> u >> v;
            uvs.push_back(u);
            uvs.push_back(v);
        } else if (tag == "o" || tag == "g") {
            std::string objectName;
            std::getline(parts, objectName);
            if (tag == "o") ++result.objects; else ++result.groups;
            if (!objectName.empty() && objectName[0] == ' ') objectName.erase(0, 1);
            if (currentName.empty() && !objectName.empty()) currentName = objectName;
        } else if (tag == "f") {
            std::vector<Corner> face;
            std::string token;
            while (parts >> token) face.push_back(ParseCorner(token));
            if (face.size() < 3) continue;
            ++result.faces;
            // Fan triangulation: OBJ faces are convex in practice and this keeps the
            // importer allocation-free per face.
            for (std::size_t i = 1; i + 1 < face.size(); ++i) {
                Triangle triangle;
                triangle.corners[0] = face[0];
                triangle.corners[1] = face[i];
                triangle.corners[2] = face[i + 1];
                triangles.push_back(triangle);
            }
        } else if (tag == "mtllib" || tag == "usemtl") {
            ++result.ignoredMaterialLines;
        }
    }

    if (positions.empty()) {
        result.error = "OBJ has no vertex positions";
        return result;
    }
    if (triangles.empty()) {
        result.error = "OBJ has no faces";
        return result;
    }

    // Deduplicate identical corners so shared vertices stay shared (and normals smooth).
    MeshAsset& mesh = result.mesh;
    mesh.name = currentName.empty() ? name : currentName;

    struct Key {
        int position;
        int normal;
        int uv;
        bool operator==(const Key& other) const {
            return position == other.position && normal == other.normal && uv == other.uv;
        }
    };
    std::vector<Key> keys;

    for (const Triangle& triangle : triangles) {
        for (const Corner& corner : triangle.corners) {
            const int position = ResolveIndex(corner.position, positions.size());
            if (position < 0 || position >= static_cast<int>(positions.size())) continue;
            const int normal = ResolveIndex(corner.normal, normals.size());
            const int uv = ResolveIndex(corner.uv, uvs.size() / 2);
            const Key key{position, normal, uv};

            u32 index = 0;
            bool found = false;
            for (std::size_t i = 0; i < keys.size(); ++i) {
                if (keys[i] == key) {
                    index = static_cast<u32>(i);
                    found = true;
                    break;
                }
            }
            if (!found) {
                scene::MeshVertex vertex;
                vertex.position = positions[static_cast<std::size_t>(position)];
                if (normal >= 0 && normal < static_cast<int>(normals.size())) {
                    vertex.normal = normals[static_cast<std::size_t>(normal)];
                } else {
                    vertex.normal = Vec3(0.0f, 0.0f, 0.0f);
                }
                if (uv >= 0 && (uv * 2 + 1) < static_cast<int>(uvs.size())) {
                    vertex.u = uvs[static_cast<std::size_t>(uv) * 2];
                    vertex.v = uvs[static_cast<std::size_t>(uv) * 2 + 1];
                }
                keys.push_back(key);
                index = static_cast<u32>(mesh.vertices.size());
                mesh.vertices.push_back(vertex);
            }
            mesh.indices.push_back(index);
        }
    }

    if (mesh.indices.size() < 3) {
        result.error = "OBJ faces could not be triangulated";
        return result;
    }

    // Fill in missing normals rather than leaving the mesh flat-black.
    bool missingNormals = false;
    for (const auto& vertex : mesh.vertices) {
        if (vertex.normal.x == 0.0f && vertex.normal.y == 0.0f && vertex.normal.z == 0.0f) {
            missingNormals = true;
            break;
        }
    }
    if (missingNormals) {
        mesh.RecomputeNormals();
        result.warning = "OBJ had no normals - generated smooth normals";
    }
    if (result.ignoredMaterialLines > 0) {
        if (!result.warning.empty()) result.warning += "; ";
        result.warning += "OBJ materials are ignored (assign a material in Details)";
    }

    result.ok = true;
    return result;
}

ObjLoadResult LoadObjFile(const std::string& path, bool fitToUnitBox, bool recenter) {
    std::string text;
    if (!fs::ReadTextFile(path, text)) {
        ObjLoadResult failed;
        failed.error = "Could not read " + path;
        return failed;
    }

    std::string name = path;
    const std::size_t slash = name.find_last_of("/\\");
    if (slash != std::string::npos) name = name.substr(slash + 1);
    const std::size_t dot = name.find_last_of('.');
    if (dot != std::string::npos) name = name.substr(0, dot);

    ObjLoadResult result = ParseObj(text, name);
    if (!result.ok) return result;
    if (recenter) Recenter(result.mesh);
    if (fitToUnitBox) FitToUnitBox(result.mesh);
    return result;
}

}  // namespace astra::asset
