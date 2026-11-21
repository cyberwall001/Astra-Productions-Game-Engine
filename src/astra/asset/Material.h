// asset/Material.h - text-authored material assets (*.astramat).
//
// A material asset is a small, human-editable file that maps onto the engine's material
// component. Keeping it plain text means shaders can be authored in any editor, diffed in
// git, and hot reloaded while the studio is running - the same workflow a UE5 material
// instance gives you, minus the node graph for now.
#pragma once

#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

#include "astra/scene/Scene.h"

namespace astra::asset {

struct MaterialAsset {
    std::string name = "New Material";
    scene::MaterialShader shader = scene::MaterialShader::Lit;
    scene::Vec3 baseColor{0.72f, 0.74f, 0.80f};
    scene::Vec3 accentColor{0.24f, 0.28f, 0.38f};
    float uvScale = 4.0f;
    float roughness = 0.45f;
    float metallic = 0.0f;
    float emissive = 0.0f;
    float opacity = 1.0f;
    float fresnelPower = 3.0f;

    // Runtime bookkeeping (not part of the file body).
    std::string sourcePath;
    std::uint64_t loadedStamp = 0;

    scene::MaterialComponent ToComponent() const;
    void FromComponent(const scene::MaterialComponent& component);
};

// `key = value` lines, '#' starts a comment. Unknown keys are ignored so a hand-edited file
// never fails to load, and missing keys keep their defaults.
std::string SerializeMaterial(const MaterialAsset& asset);
bool ParseMaterial(std::string_view text, MaterialAsset& out, std::string* error = nullptr);

class MaterialLibrary {
public:
    // Default authoring folder: <project>/materials, or <DataRoot>/engine/materials.
    static std::string DefaultDirectory();

    // Scans *.astramat in `directory` (creating it and seeding the starter set when empty).
    void Scan(const std::string& directory);

    // Re-reads files whose timestamp changed. Returns the number of materials reloaded.
    int ReloadChanged();

    bool Save(MaterialAsset& asset);
    bool CreateStarterSet(const std::string& directory) const;

    const std::vector<MaterialAsset>& All() const { return materials_; }
    const std::string& Directory() const { return directory_; }

    const MaterialAsset* Find(std::string_view name) const;
    MaterialAsset* FindMutable(std::string_view name);

    // New material added to the library (not written until Save()).
    MaterialAsset& Add(std::string name);

    static bool ApplyTo(scene::Scene& scene, scene::EntityId entity, std::string_view name,
                        const MaterialLibrary& library);

private:
    std::vector<MaterialAsset> materials_;
    std::string directory_;
};

// The starter materials every new project gets, so the picker is never empty.
const std::vector<MaterialAsset>& StarterMaterials();

}  // namespace astra::asset
