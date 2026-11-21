// asset/Material.cpp - parsing and library management for *.astramat assets.
#include "astra/asset/Material.h"

#include <algorithm>
#include <cctype>
#include <cstdio>
#include <sstream>

#include "astra/core/FileIO.h"
#include "astra/core/Log.h"
#include "astra/core/Paths.h"
#include "astra/core/Strings.h"

namespace astra::asset {
namespace {

std::string Trim(std::string_view text) {
    std::size_t start = 0;
    std::size_t end = text.size();
    while (start < end && std::isspace(static_cast<unsigned char>(text[start]))) ++start;
    while (end > start && std::isspace(static_cast<unsigned char>(text[end - 1]))) --end;
    return std::string(text.substr(start, end - start));
}

// File-safe material name: "Brushed Steel" -> "Brushed_Steel".
std::string Slug(std::string_view text) {
    std::string out;
    out.reserve(text.size());
    for (char c : text) {
        const unsigned char uc = static_cast<unsigned char>(c);
        if (std::isalnum(uc)) {
            out.push_back(c);
        } else if (std::isspace(uc) || c == '-' || c == '.') {
            if (!out.empty() && out.back() != '_') out.push_back('_');
        }
    }
    while (!out.empty() && out.back() == '_') out.pop_back();
    return out.empty() ? std::string("material") : out;
}

std::string Lower(std::string_view text) {
    std::string out(text);
    std::transform(out.begin(), out.end(), out.begin(), [](unsigned char c) {
        return static_cast<char>(std::tolower(c));
    });
    return out;
}

bool ParseVec3(const std::string& text, scene::Vec3& out) {
    float values[3] = {0.0f, 0.0f, 0.0f};
    int count = 0;
    std::istringstream stream(text);
    std::string token;
    while (count < 3 && stream >> token) {
        try {
            values[count] = std::stof(token);
        } catch (...) {
            return false;
        }
        ++count;
    }
    if (count != 3) return false;
    out = scene::Vec3(values[0], values[1], values[2]);
    return true;
}

std::string FormatVec3(const scene::Vec3& value) {
    char buffer[96];
    std::snprintf(buffer, sizeof(buffer), "%.3f %.3f %.3f", value.x, value.y, value.z);
    return buffer;
}

std::string FormatFloat(float value) {
    char buffer[32];
    std::snprintf(buffer, sizeof(buffer), "%g", static_cast<double>(value));
    return buffer;
}

scene::MaterialShader ShaderFromName(std::string_view name) {
    int count = 0;
    const char* const* names = scene::MaterialShaderNames(count);
    for (int i = 0; i < count; ++i) {
        if (str::EqualsIgnoreCase(names[i], name)) return static_cast<scene::MaterialShader>(i);
    }
    return scene::MaterialShader::Lit;
}

std::string ShaderName(scene::MaterialShader shader) { return scene::MaterialShaderName(shader); }

}  // namespace

scene::MaterialComponent MaterialAsset::ToComponent() const {
    scene::MaterialComponent component;
    component.shader = shader;
    component.baseColor = baseColor;
    component.accentColor = accentColor;
    component.uvScale = uvScale;
    component.roughness = roughness;
    component.metallic = metallic;
    component.emissive = emissive;
    component.opacity = opacity;
    component.fresnelPower = fresnelPower;
    return component;
}

void MaterialAsset::FromComponent(const scene::MaterialComponent& component) {
    shader = component.shader;
    baseColor = component.baseColor;
    accentColor = component.accentColor;
    uvScale = component.uvScale;
    roughness = component.roughness;
    metallic = component.metallic;
    emissive = component.emissive;
    opacity = component.opacity;
    fresnelPower = component.fresnelPower;
}

std::string SerializeMaterial(const MaterialAsset& asset) {
    std::ostringstream out;
    out << "# Astra material asset - edit in any text editor, the studio hot reloads it.\n"
        << "# Colours are linear RGB triples in 0..1.\n"
        << "name = " << asset.name << "\n"
        << "shader = " << ShaderName(asset.shader) << "\n"
        << "baseColor = " << FormatVec3(asset.baseColor) << "\n"
        << "accentColor = " << FormatVec3(asset.accentColor) << "\n"
        << "uvScale = " << FormatFloat(asset.uvScale) << "\n"
        << "roughness = " << FormatFloat(asset.roughness) << "\n"
        << "metallic = " << FormatFloat(asset.metallic) << "\n"
        << "emissive = " << FormatFloat(asset.emissive) << "\n"
        << "opacity = " << FormatFloat(asset.opacity) << "\n"
        << "fresnelPower = " << FormatFloat(asset.fresnelPower) << "\n";
    return out.str();
}

bool ParseMaterial(std::string_view text, MaterialAsset& out, std::string* error) {
    std::istringstream stream{std::string(text)};
    std::string line;
    int lineNumber = 0;
    MaterialAsset parsed;

    while (std::getline(stream, line)) {
        ++lineNumber;
        const std::string trimmed = Trim(line);
        if (trimmed.empty() || trimmed[0] == '#') continue;

        const std::size_t separator = trimmed.find_first_of("=:");
        if (separator == std::string::npos) {
            if (error) *error = "line " + std::to_string(lineNumber) + ": expected key = value";
            return false;
        }
        const std::string key = Lower(Trim(std::string_view(trimmed).substr(0, separator)));
        const std::string value = Trim(std::string_view(trimmed).substr(separator + 1));

        auto number = [&](float& target) {
            try {
                target = std::stof(value);
            } catch (...) {
                return false;
            }
            return true;
        };

        bool ok = true;
        if (key == "name") {
            parsed.name = value.empty() ? "New Material" : value;
        } else if (key == "shader") {
            parsed.shader = ShaderFromName(value);
        } else if (key == "basecolor" || key == "base") {
            ok = ParseVec3(value, parsed.baseColor);
        } else if (key == "accentcolor" || key == "accent") {
            ok = ParseVec3(value, parsed.accentColor);
        } else if (key == "uvscale") {
            ok = number(parsed.uvScale);
        } else if (key == "roughness") {
            ok = number(parsed.roughness);
        } else if (key == "metallic") {
            ok = number(parsed.metallic);
        } else if (key == "emissive") {
            ok = number(parsed.emissive);
        } else if (key == "opacity") {
            ok = number(parsed.opacity);
        } else if (key == "fresnelpower" || key == "fresnel") {
            ok = number(parsed.fresnelPower);
        }
        // Unknown keys are deliberately ignored: a file from a newer build still loads.

        if (!ok) {
            if (error) *error = "line " + std::to_string(lineNumber) + ": bad value for " + key;
            return false;
        }
    }

    parsed.uvScale = std::max(0.01f, parsed.uvScale);
    parsed.roughness = std::clamp(parsed.roughness, 0.02f, 1.0f);
    parsed.metallic = std::clamp(parsed.metallic, 0.0f, 1.0f);
    parsed.emissive = std::max(0.0f, parsed.emissive);
    parsed.opacity = std::clamp(parsed.opacity, 0.0f, 1.0f);
    parsed.fresnelPower = std::max(0.25f, parsed.fresnelPower);
    out = parsed;
    return true;
}

const std::vector<MaterialAsset>& StarterMaterials() {
    static const std::vector<MaterialAsset> kStarters = [] {
        auto make = [](const char* name, scene::MaterialShader shader, scene::Vec3 base,
                       scene::Vec3 accent, float roughness, float metallic, float emissive) {
            MaterialAsset asset;
            asset.name = name;
            asset.shader = shader;
            asset.baseColor = base;
            asset.accentColor = accent;
            asset.roughness = roughness;
            asset.metallic = metallic;
            asset.emissive = emissive;
            return asset;
        };
        std::vector<MaterialAsset> list;
        list.push_back(make("Brushed Steel", scene::MaterialShader::Lit,
                            scene::Vec3(0.62f, 0.64f, 0.68f), scene::Vec3(0.30f, 0.32f, 0.36f),
                            0.32f, 0.9f, 0.0f));
        list.push_back(make("Painted Concrete", scene::MaterialShader::Noise,
                            scene::Vec3(0.58f, 0.56f, 0.53f), scene::Vec3(0.34f, 0.33f, 0.31f),
                            0.86f, 0.0f, 0.0f));
        list.push_back(make("Hologram Panel", scene::MaterialShader::Hologram,
                            scene::Vec3(0.24f, 0.72f, 0.98f), scene::Vec3(0.06f, 0.20f, 0.34f),
                            0.18f, 0.0f, 1.35f));
        list.push_back(make("Warning Stripes", scene::MaterialShader::Stripes,
                            scene::Vec3(0.94f, 0.72f, 0.12f), scene::Vec3(0.16f, 0.14f, 0.12f),
                            0.55f, 0.0f, 0.0f));
        list.push_back(make("Emissive Cyan", scene::MaterialShader::Emissive,
                            scene::Vec3(0.14f, 0.86f, 0.94f), scene::Vec3(0.04f, 0.28f, 0.32f),
                            0.40f, 0.0f, 2.0f));
        list.push_back(make("Tile Checker", scene::MaterialShader::Checker,
                            scene::Vec3(0.80f, 0.80f, 0.82f), scene::Vec3(0.26f, 0.27f, 0.30f),
                            0.50f, 0.1f, 0.0f));
        return list;
    }();
    return kStarters;
}

std::string MaterialLibrary::DefaultDirectory() {
    return paths::EnsureDataDir("engine/materials");
}

bool MaterialLibrary::CreateStarterSet(const std::string& directory) const {
    if (!fs::CreateDirectories(directory)) return false;
    for (const MaterialAsset& starter : StarterMaterials()) {
        const std::string path =
            paths::Join(directory, Slug(starter.name) + ".astramat");
        if (fs::Exists(path)) continue;
        if (!fs::WriteTextFileAtomic(path, SerializeMaterial(starter))) return false;
    }
    return true;
}

void MaterialLibrary::Scan(const std::string& directory) {
    directory_ = directory;
    materials_.clear();
    fs::CreateDirectories(directory_);
    if (fs::ListDirectory(directory_).empty()) CreateStarterSet(directory_);

    for (const std::string& file : fs::ListDirectory(directory_)) {
        // fs::Extension keeps the dot (".astramat").
        const std::string extension = Lower(fs::Extension(file));
        if (extension != ".astramat") continue;
        const std::string path = paths::Join(directory_, file);
        std::string text;
        if (!fs::ReadTextFile(path, text)) continue;
        MaterialAsset asset;
        std::string error;
        if (!ParseMaterial(text, asset, &error)) {
            log::Warn("materials", file + ": " + error);
            continue;
        }
        if (asset.name.empty()) asset.name = fs::FileName(path);
        asset.sourcePath = path;
        asset.loadedStamp = fs::ModifiedTime(path);
        materials_.push_back(std::move(asset));
    }
    if (materials_.empty()) {
        // A folder that was wiped still yields a usable library.
        for (const MaterialAsset& starter : StarterMaterials()) materials_.push_back(starter);
    }
    log::Info("materials", "loaded " + std::to_string(materials_.size()) + " material(s) from " +
                               directory_);
}

int MaterialLibrary::ReloadChanged() {
    int reloaded = 0;
    for (MaterialAsset& asset : materials_) {
        if (asset.sourcePath.empty()) continue;
        const std::uint64_t stamp = fs::ModifiedTime(asset.sourcePath);
        if (stamp == 0 || stamp == asset.loadedStamp) continue;
        std::string text;
        if (!fs::ReadTextFile(asset.sourcePath, text)) continue;
        MaterialAsset parsed;
        std::string error;
        if (!ParseMaterial(text, parsed, &error)) {
            log::Warn("materials", fs::FileName(asset.sourcePath) + ": " + error);
            asset.loadedStamp = stamp;  // do not spam the log every frame
            continue;
        }
        parsed.sourcePath = asset.sourcePath;
        parsed.loadedStamp = stamp;
        if (parsed.name.empty()) parsed.name = asset.name;
        asset = parsed;
        ++reloaded;
    }
    return reloaded;
}

bool MaterialLibrary::Save(MaterialAsset& asset) {
    if (directory_.empty()) directory_ = DefaultDirectory();
    if (asset.sourcePath.empty()) {
        asset.sourcePath =
            paths::Join(directory_, Slug(asset.name) + ".astramat");
    }
    if (!fs::WriteTextFileAtomic(asset.sourcePath, SerializeMaterial(asset))) return false;
    asset.loadedStamp = fs::ModifiedTime(asset.sourcePath);
    if (Find(asset.name) == nullptr) materials_.push_back(asset);
    return true;
}

const MaterialAsset* MaterialLibrary::Find(std::string_view name) const {
    for (const MaterialAsset& asset : materials_) {
        if (str::EqualsIgnoreCase(asset.name, name)) return &asset;
    }
    return nullptr;
}

MaterialAsset* MaterialLibrary::FindMutable(std::string_view name) {
    for (MaterialAsset& asset : materials_) {
        if (str::EqualsIgnoreCase(asset.name, name)) return &asset;
    }
    return nullptr;
}

MaterialAsset& MaterialLibrary::Add(std::string name) {
    MaterialAsset asset;
    asset.name = std::move(name);
    asset.sourcePath = paths::Join(directory_.empty() ? DefaultDirectory() : directory_,
                                   Slug(asset.name) + ".astramat");
    materials_.push_back(std::move(asset));
    return materials_.back();
}

bool MaterialLibrary::ApplyTo(scene::Scene& scene, scene::EntityId entity, std::string_view name,
                              const MaterialLibrary& library) {
    const MaterialAsset* asset = library.Find(name);
    if (asset == nullptr) return false;
    scene::Entity* target = scene.Find(entity);
    if (target == nullptr) return false;
    if (!target->Has(scene::Component::Material)) scene.AddComponent(entity, scene::Component::Material);
    if (!target->Has(scene::Component::Material)) return false;
    target->material = asset->ToComponent();
    return true;
}

}  // namespace astra::asset
