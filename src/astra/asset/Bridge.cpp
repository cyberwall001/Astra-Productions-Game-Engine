#include "astra/asset/Bridge.h"

#include <algorithm>
#include <cmath>
#include <cstdio>

#include "astra/asset/ObjLoader.h"
#include "astra/core/FileIO.h"
#include "astra/core/Paths.h"
#include "astra/data/Json.h"

namespace astra::asset {
namespace {

std::string BridgeRoot() { return paths::EnsureDataDir("bridge"); }

std::string BaseName(const std::string& path) {
    const std::size_t slash = path.find_last_of("/\\");
    return slash == std::string::npos ? path : path.substr(slash + 1);
}

std::string DirectoryOf(const std::string& path) {
    const std::size_t slash = path.find_last_of("/\\");
    return slash == std::string::npos ? std::string() : path.substr(0, slash);
}

std::string Stem(const std::string& fileName) {
    const std::size_t dot = fileName.find_last_of('.');
    return dot == std::string::npos ? fileName : fileName.substr(0, dot);
}

bool HasSuffix(const std::string& text, const char* suffix) {
    const std::size_t length = std::char_traits<char>::length(suffix);
    return text.size() >= length && text.compare(text.size() - length, length, suffix) == 0;
}

}  // namespace

std::string InboxDirectory() {
    const std::string directory = paths::Join(BridgeRoot(), "inbox");
    fs::CreateDirectories(directory);
    return directory;
}

std::string StatusPath() { return paths::Join(BridgeRoot(), "status.json"); }

std::string BlenderStatus() {
    std::string text;
    if (!fs::ReadTextFile(StatusPath(), text)) return "Blender not connected";
    const json::Value status = json::Value::ParseOrNull(text, nullptr);
    if (!status.IsObject()) return "Blender not connected";
    const std::string version = status["blender"].AsString("");
    const std::string scene = status["scene"].AsString("");
    const double stamp = status["timestamp"].AsNumber(0.0);
    if (stamp <= 0.0) return "Blender connected";
    std::string label = version.empty() ? "Blender connected" : "Connected - Blender " + version;
    if (!scene.empty()) label += " / " + scene;
    return label;
}

std::vector<BridgeJob> ScanInbox() {
    std::vector<BridgeJob> jobs;
    const std::string directory = InboxDirectory();
    for (const std::string& entry : fs::ListDirectory(directory)) {
        if (!HasSuffix(entry, ".obj")) continue;
        BridgeJob job;
        job.objPath = paths::Join(directory, entry);
        job.name = Stem(entry);

        const std::string metaPath = paths::Join(directory, Stem(entry) + ".json");
        if (fs::Exists(metaPath)) {
            job.metaPath = metaPath;
            std::string text;
            if (fs::ReadTextFile(metaPath, text)) {
                const json::Value meta = json::Value::ParseOrNull(text, nullptr);
                if (meta.IsObject()) {
                    const std::string name = meta["name"].AsString("");
                    if (!name.empty()) job.name = name;
                    job.material = meta["material"].AsString("");
                    job.placeholder = meta["placeholder"].AsBool(false);
                }
            }
        }
        jobs.push_back(std::move(job));
    }
    std::sort(jobs.begin(), jobs.end(),
              [](const BridgeJob& a, const BridgeJob& b) { return a.name < b.name; });
    return jobs;
}

bool ImportJob(scene::Scene& scene, const BridgeJob& job, const scene::Vec3& spawn,
               BridgeImportResult& result) {
    ObjLoadResult loaded = LoadObjFile(job.objPath, true, true);
    if (!loaded.ok || loaded.mesh.Empty()) {
        result.skipped += 1;
        result.errors.push_back(job.name + ": " +
                                (loaded.error.empty() ? "empty mesh" : loaded.error));
        return false;
    }

    scene::MeshAsset mesh = std::move(loaded.mesh);
    if (mesh.name.empty() || mesh.name == "Imported mesh") mesh.name = job.name;
    if (!job.name.empty()) mesh.name = job.name;

    const std::uint32_t meshId = scene.AddMesh(std::move(mesh));
    const scene::EntityId actor = scene.CreateEntity(job.name, scene::kInvalidEntity);
    if (scene::Entity* entity = scene.Find(actor)) {
        entity->Add(scene::Component::Mesh | scene::Component::Material);
        entity->mesh.shape = scene::MeshShape::Custom;
        entity->mesh.meshId = meshId;
        entity->mesh.size = scene::Vec3(1.0f, 1.0f, 1.0f);
        entity->transform.position = spawn;
        entity->tag = "blender";
        if (const scene::MeshAsset* stored = scene.Mesh(meshId)) {
            if (stored->albedo.x > 0.0f) entity->material.baseColor = stored->albedo;
        }
        if (job.material == "Hologram") {
            entity->material.shader = scene::MaterialShader::Hologram;
            entity->material.opacity = 0.75f;
            entity->material.emissive = 0.8f;
        } else if (job.material == "Checker") {
            entity->material.shader = scene::MaterialShader::Checker;
        } else if (job.material == "Emissive") {
            entity->material.shader = scene::MaterialShader::Emissive;
            entity->material.emissive = 1.0f;
        }
        result.actorNames.push_back(entity->name);
    }

    result.imported += 1;
    result.triangles += loaded.faces;
    scene.MarkDirty();
    return true;
}

BridgeImportResult ImportInbox(scene::Scene& scene, bool moveImported) {
    BridgeImportResult result;
    const std::vector<BridgeJob> jobs = ScanInbox();
    const std::string importedDirectory = paths::Join(BridgeRoot(), "imported");
    fs::CreateDirectories(importedDirectory);

    int slot = 0;
    for (const BridgeJob& job : jobs) {
        if (job.placeholder) {
            result.skipped += 1;
            continue;
        }
        // Spawn in a loose ring so a batch of imports does not stack on one another.
        const float angle = static_cast<float>(slot) * 0.9f;
        const scene::Vec3 spawn(std::cos(angle) * 4.0f, 1.0f, std::sin(angle) * 4.0f);
        ImportJob(scene, job, spawn, result);
        ++slot;

        if (moveImported) {
            const std::string objName = BaseName(job.objPath);
            std::rename(job.objPath.c_str(), paths::Join(importedDirectory, objName).c_str());
            if (!job.metaPath.empty()) {
                std::rename(job.metaPath.c_str(),
                            paths::Join(importedDirectory, BaseName(job.metaPath)).c_str());
            }
        }
    }
    return result;
}

bool WriteBridgeJob(const std::string& name, const scene::MeshAsset& mesh, const std::string& material,
                    std::string* error) {
    if (mesh.Empty()) {
        if (error) *error = "mesh has no triangles";
        return false;
    }
    const std::string directory = InboxDirectory();
    std::string stem = name.empty() ? "astra_mesh" : name;
    for (char& c : stem) {
        if (c == '/' || c == '\\' || c == ':') c = '_';
    }

    // OBJ payload.
    std::string obj = "# Astra Productions bridge export\n";
    obj += "o " + stem + "\n";
    char line[160];
    for (const scene::MeshVertex& vertex : mesh.vertices) {
        std::snprintf(line, sizeof(line), "v %.6f %.6f %.6f\n",
                      static_cast<double>(vertex.position.x), static_cast<double>(vertex.position.y),
                      static_cast<double>(vertex.position.z));
        obj += line;
    }
    for (const scene::MeshVertex& vertex : mesh.vertices) {
        std::snprintf(line, sizeof(line), "vt %.6f %.6f\n", static_cast<double>(vertex.u),
                      static_cast<double>(vertex.v));
        obj += line;
    }
    for (const scene::MeshVertex& vertex : mesh.vertices) {
        std::snprintf(line, sizeof(line), "vn %.6f %.6f %.6f\n",
                      static_cast<double>(vertex.normal.x), static_cast<double>(vertex.normal.y),
                      static_cast<double>(vertex.normal.z));
        obj += line;
    }
    for (std::size_t i = 0; i + 2 < mesh.indices.size(); i += 3) {
        std::snprintf(line, sizeof(line), "f %u %u %u\n", mesh.indices[i] + 1,
                      mesh.indices[i + 1] + 1, mesh.indices[i + 2] + 1);
        obj += line;
    }

    json::Value meta = json::Value::MakeObject();
    meta["name"] = json::Value(stem);
    meta["material"] = json::Value(material.empty() ? std::string("Lit") : material);
    meta["source"] = json::Value(std::string("bridge"));
    meta["triangles"] = json::Value(static_cast<double>(mesh.indices.size() / 3));
    meta["placeholder"] = json::Value(false);

    if (!fs::WriteTextFileAtomic(paths::Join(directory, stem + ".obj"), obj)) {
        if (error) *error = "could not write " + stem + ".obj";
        return false;
    }
    if (!fs::WriteTextFileAtomic(paths::Join(directory, stem + ".json"), meta.Dump())) {
        if (error) *error = "could not write " + stem + ".json";
        return false;
    }
    return true;
}

}  // namespace astra::asset
