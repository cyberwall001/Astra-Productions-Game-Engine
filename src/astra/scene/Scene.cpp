#include "astra/scene/Scene.h"

#include <algorithm>
#include <cmath>

#include "astra/core/Strings.h"
#include "astra/core/Version.h"
#include "astra/data/Json.h"

namespace astra::scene {

namespace {

// Local vector helpers: the data layer stays free of the renderer's maths header.
Vec3 VSub(const Vec3& a, const Vec3& b) { return Vec3(a.x - b.x, a.y - b.y, a.z - b.z); }
Vec3 VAdd(const Vec3& a, const Vec3& b) { return Vec3(a.x + b.x, a.y + b.y, a.z + b.z); }

Vec3 VCross(const Vec3& a, const Vec3& b) {
    return Vec3(a.y * b.z - a.z * b.y, a.z * b.x - a.x * b.z, a.x * b.y - a.y * b.x);
}

Vec3 VNormalize(const Vec3& a) {
    const float length =
        std::sqrt(a.x * a.x + a.y * a.y + a.z * a.z);
    if (length < 1e-6f) return Vec3(0.0f, 1.0f, 0.0f);
    return Vec3(a.x / length, a.y / length, a.z / length);
}

}  // namespace
namespace {

const char* const kStormPresets[] = {"Clear", "Shelf cloud", "Supercell", "Tornado",
                                     "Derecho", "Hail core"};
constexpr int kStormPresetCount = static_cast<int>(sizeof(kStormPresets) / sizeof(kStormPresets[0]));

const char* const kMeshShapes[] = {"Cube", "Sphere", "Cylinder", "Cone", "Plane", "Custom"};

const char* const kMaterialShaders[] = {"Lit",   "Unlit",  "Checker", "Stripes",
                                       "Noise", "Gradient", "Hologram", "Emissive"};

const char* const kParticlePresets[] = {"Smoke", "Sparks", "Rain", "Dust", "Fire"};
constexpr int kParticlePresetCount =
    static_cast<int>(sizeof(kParticlePresets) / sizeof(kParticlePresets[0]));

}  // namespace

const char* MeshShapeName(MeshShape shape) {
    const int index = static_cast<int>(shape);
    if (index < 0 || index >= kMeshShapeCount) return kMeshShapes[0];
    return kMeshShapes[index];
}

const char* const* MeshShapeNames(int& count) {
    count = kMeshShapeCount;
    return kMeshShapes;
}

const char* MaterialShaderName(MaterialShader shader) {
    const int index = static_cast<int>(shader);
    if (index < 0 || index >= kMaterialShaderCount) return kMaterialShaders[0];
    return kMaterialShaders[index];
}

const char* const* MaterialShaderNames(int& count) {
    count = kMaterialShaderCount;
    return kMaterialShaders;
}

const char* ParticlePresetName(ParticlePreset preset) {
    const int index = static_cast<int>(preset);
    if (index < 0 || index >= kParticlePresetCount) return kParticlePresets[0];
    return kParticlePresets[index];
}

const char* const* ParticlePresetNames(int& count) {
    count = kParticlePresetCount;
    return kParticlePresets;
}

void ApplyParticlePreset(ParticleComponent& p) {
    switch (p.preset) {
        case ParticlePreset::Sparks:
            p.rate = 90.0f; p.lifetime = 0.85f; p.startSize = 0.06f; p.endSize = 0.02f;
            p.speed = 6.5f; p.gravity = -9.2f; p.spread = 1.0f;
            p.color = Vec3(1.0f, 0.82f, 0.42f); p.colorEnd = Vec3(0.85f, 0.22f, 0.10f);
            p.opacity = 0.9f; p.additive = true; p.emitRadius = 0.1f;
            break;
        case ParticlePreset::Rain:
            p.rate = 320.0f; p.lifetime = 1.4f; p.startSize = 0.05f; p.endSize = 0.05f;
            p.speed = 14.0f; p.gravity = -22.0f; p.spread = 0.25f;
            p.color = Vec3(0.62f, 0.72f, 0.88f); p.colorEnd = Vec3(0.42f, 0.52f, 0.70f);
            p.opacity = 0.45f; p.additive = false; p.emitRadius = 6.0f;
            break;
        case ParticlePreset::Dust:
            p.rate = 26.0f; p.lifetime = 3.6f; p.startSize = 0.09f; p.endSize = 0.16f;
            p.speed = 0.35f; p.gravity = 0.02f; p.spread = 0.9f;
            p.color = Vec3(0.92f, 0.90f, 0.84f); p.colorEnd = Vec3(0.55f, 0.58f, 0.66f);
            p.opacity = 0.30f; p.additive = false; p.emitRadius = 1.6f;
            break;
        case ParticlePreset::Fire:
            p.rate = 120.0f; p.lifetime = 1.1f; p.startSize = 0.42f; p.endSize = 0.06f;
            p.speed = 2.4f; p.gravity = 1.4f; p.spread = 0.5f;
            p.color = Vec3(1.0f, 0.68f, 0.22f); p.colorEnd = Vec3(0.55f, 0.10f, 0.05f);
            p.opacity = 0.75f; p.additive = true; p.emitRadius = 0.35f;
            break;
        case ParticlePreset::Smoke:
        case ParticlePreset::Count:
        default:
            p.rate = 18.0f; p.lifetime = 4.2f; p.startSize = 0.5f; p.endSize = 2.2f;
            p.speed = 0.9f; p.gravity = 0.5f; p.spread = 0.55f;
            p.color = Vec3(0.60f, 0.62f, 0.68f); p.colorEnd = Vec3(0.22f, 0.24f, 0.30f);
            p.opacity = 0.36f; p.additive = false; p.emitRadius = 0.6f;
            break;
    }
}

// ---------------------------------------------------------------------------
// MeshAsset
// ---------------------------------------------------------------------------

void MeshAsset::RecomputeNormals() {
    for (auto& vertex : vertices) vertex.normal = Vec3(0.0f, 0.0f, 0.0f);
    for (std::size_t i = 0; i + 2 < indices.size(); i += 3) {
        const u32 i0 = indices[i];
        const u32 i1 = indices[i + 1];
        const u32 i2 = indices[i + 2];
        if (i0 >= vertices.size() || i1 >= vertices.size() || i2 >= vertices.size()) continue;
        const Vec3 a = vertices[i0].position;
        const Vec3 b = vertices[i1].position;
        const Vec3 c = vertices[i2].position;
        const Vec3 normal = VCross(VSub(b, a), VSub(c, a));
        vertices[i0].normal = VAdd(vertices[i0].normal, normal);
        vertices[i1].normal = VAdd(vertices[i1].normal, normal);
        vertices[i2].normal = VAdd(vertices[i2].normal, normal);
    }
    for (auto& vertex : vertices) vertex.normal = VNormalize(vertex.normal);
}

void MeshAsset::Bounds(Vec3& outMin, Vec3& outMax) const {
    if (vertices.empty()) {
        outMin = Vec3(0.0f, 0.0f, 0.0f);
        outMax = Vec3(0.0f, 0.0f, 0.0f);
        return;
    }
    outMin = vertices[0].position;
    outMax = vertices[0].position;
    for (const auto& vertex : vertices) {
        outMin.x = std::min(outMin.x, vertex.position.x);
        outMin.y = std::min(outMin.y, vertex.position.y);
        outMin.z = std::min(outMin.z, vertex.position.z);
        outMax.x = std::max(outMax.x, vertex.position.x);
        outMax.y = std::max(outMax.y, vertex.position.y);
        outMax.z = std::max(outMax.z, vertex.position.z);
    }
}

void MeshAsset::Transform(const Vec3& scale, const Vec3& translation) {
    for (auto& vertex : vertices) {
        vertex.position = Vec3(vertex.position.x * scale.x + translation.x,
                               vertex.position.y * scale.y + translation.y,
                               vertex.position.z * scale.z + translation.z);
        vertex.normal = VNormalize(Vec3(vertex.normal.x / (scale.x == 0.0f ? 1.0f : scale.x),
                                        vertex.normal.y / (scale.y == 0.0f ? 1.0f : scale.y),
                                        vertex.normal.z / (scale.z == 0.0f ? 1.0f : scale.z)));
    }
}

u32 Scene::AddMesh(MeshAsset asset) {
    if (asset.Empty()) return 0;
    for (auto& vertex : asset.vertices) vertex.normal = VNormalize(vertex.normal);
    meshes_.push_back(std::move(asset));
    dirty_ = true;
    return static_cast<u32>(meshes_.size());  // 1-based
}

const MeshAsset* Scene::Mesh(u32 id) const {
    if (id == 0 || id > meshes_.size()) return nullptr;
    return &meshes_[id - 1];
}

u32 Scene::FindMesh(std::string_view name) const {
    for (std::size_t i = 0; i < meshes_.size(); ++i) {
        if (meshes_[i].name == name) return static_cast<u32>(i + 1);
    }
    return 0;
}

void ApplyStormPreset(WeatherComponent& weather) {
    switch (weather.preset) {
        case StormPreset::Clear:
            weather.capeJkg = 400.0f;
            weather.stormRelativeHelicity = 20.0f;
            weather.precipMmPerHour = 0.0f;
            weather.windSpeedMps = 9.0f;
            weather.lightningPerMinute = 0.0f;
            break;
        case StormPreset::ShelfCloud:
            weather.capeJkg = 1200.0f;
            weather.stormRelativeHelicity = 90.0f;
            weather.precipMmPerHour = 6.0f;
            weather.windSpeedMps = 21.0f;
            weather.lightningPerMinute = 1.0f;
            break;
        case StormPreset::Supercell:
            weather.capeJkg = 2400.0f;
            weather.stormRelativeHelicity = 180.0f;
            weather.precipMmPerHour = 12.0f;
            weather.windSpeedMps = 28.0f;
            weather.lightningPerMinute = 3.0f;
            break;
        case StormPreset::Tornado:
            weather.capeJkg = 3200.0f;
            weather.stormRelativeHelicity = 310.0f;
            weather.precipMmPerHour = 34.0f;
            weather.windSpeedMps = 42.0f;
            weather.lightningPerMinute = 9.0f;
            break;
        case StormPreset::Derecho:
            weather.capeJkg = 2900.0f;
            weather.stormRelativeHelicity = 70.0f;
            weather.precipMmPerHour = 22.0f;
            weather.windSpeedMps = 38.0f;
            weather.lightningPerMinute = 5.0f;
            break;
        case StormPreset::HailCore:
            weather.capeJkg = 3600.0f;
            weather.stormRelativeHelicity = 160.0f;
            weather.precipMmPerHour = 45.0f;
            weather.windSpeedMps = 33.0f;
            weather.lightningPerMinute = 7.0f;
            break;
    }
}

namespace {

json::Value Vec3ToJson(const Vec3& v) {
    json::Value array = json::Value::MakeArray();
    array.Push(json::Value(v.x));
    array.Push(json::Value(v.y));
    array.Push(json::Value(v.z));
    return array;
}

Vec3 Vec3FromJson(const json::Value& value) {
    return Vec3(static_cast<float>(value.At(0).AsNumber()),
                static_cast<float>(value.At(1).AsNumber()),
                static_cast<float>(value.At(2).AsNumber()));
}

}  // namespace

const char* StormPresetName(StormPreset preset) {
    const int index = static_cast<int>(preset);
    return (index >= 0 && index < kStormPresetCount) ? kStormPresets[index] : "Clear";
}

const char* const* StormPresetNames(int& count) {
    count = kStormPresetCount;
    return kStormPresets;
}

float WeatherComponent::InflowLayerShear() const {
    return windSpeedMps * (0.45f + inflowDepthM / 4000.0f);
}

float WeatherComponent::TornadoProbability() const {
    const float capeTerm = std::min(1.0f, capeJkg / 4000.0f);
    const float srhTerm = std::min(1.0f, stormRelativeHelicity / 400.0f);
    return std::min(0.99f, capeTerm * 0.45f + srhTerm * 0.55f);
}

const char* LightTypeName(LightType type) {
    switch (type) {
        case LightType::Directional: return "Directional";
        case LightType::Point: return "Point";
        case LightType::Spot: return "Spot";
    }
    return "Directional";
}

void Entity::Remove(Component component) { components &= ~static_cast<u32>(component); }

Scene::Scene() { SetName("Untitled Scene"); }

void Scene::Clear() {
    entities_.clear();
    meshes_.clear();
    nextId_ = 1;
    dirty_ = false;
}

EntityId Scene::PushEntity(Entity entity) {
    entity.id = nextId_++;
    entities_.push_back(std::move(entity));
    return entities_.back().id;
}

EntityId Scene::CreateEntity(std::string name, EntityId parent) {
    Entity entity;
    entity.name = std::move(name);
    entity.parent = parent;
    entity.Add(Component::Transform);
    const EntityId id = PushEntity(std::move(entity));

    if (parent != kInvalidEntity) {
        if (Entity* parentEntity = Find(parent)) parentEntity->children.push_back(id);
    }
    dirty_ = true;
    return id;
}

bool Scene::DestroyEntity(EntityId id, bool includeChildren) {
    Entity* entity = Find(id);
    if (!entity) return false;

    std::vector<EntityId> toRemove;
    if (includeChildren) {
        std::vector<EntityId> stack = entity->children;
        while (!stack.empty()) {
            const EntityId childId = stack.back();
            stack.pop_back();
            toRemove.push_back(childId);
            if (const Entity* child = Find(childId)) {
                stack.insert(stack.end(), child->children.begin(), child->children.end());
            }
        }
    }
    toRemove.push_back(id);

    // Detach from the parent first.
    if (entity->parent != kInvalidEntity) {
        if (Entity* parent = Find(entity->parent)) {
            auto& siblings = parent->children;
            siblings.erase(std::remove(siblings.begin(), siblings.end(), id), siblings.end());
        }
    }

    for (EntityId remove : toRemove) {
        entities_.erase(std::remove_if(entities_.begin(), entities_.end(),
                                       [&](const Entity& e) { return e.id == remove; }),
                        entities_.end());
    }
    dirty_ = true;
    return true;
}

bool Scene::RenameEntity(EntityId id, std::string name) {
    Entity* entity = Find(id);
    if (!entity) return false;
    entity->name = std::move(name);
    dirty_ = true;
    return true;
}

bool Scene::Reparent(EntityId id, EntityId newParent) {
    if (id == newParent) return false;
    Entity* entity = Find(id);
    if (!entity) return false;

    // Refuse to create a cycle.
    EntityId walk = newParent;
    while (walk != kInvalidEntity) {
        if (walk == id) return false;
        const Entity* parent = Find(walk);
        walk = parent ? parent->parent : kInvalidEntity;
    }

    if (entity->parent != kInvalidEntity) {
        if (Entity* oldParent = Find(entity->parent)) {
            auto& siblings = oldParent->children;
            siblings.erase(std::remove(siblings.begin(), siblings.end(), id), siblings.end());
        }
    }
    entity->parent = newParent;
    if (newParent != kInvalidEntity) {
        if (Entity* parent = Find(newParent)) parent->children.push_back(id);
    }
    dirty_ = true;
    return true;
}

bool Scene::SetActive(EntityId id, bool active) {
    Entity* entity = Find(id);
    if (!entity) return false;
    entity->active = active;
    dirty_ = true;
    return true;
}

bool Scene::AddComponent(EntityId id, Component component) {
    Entity* entity = Find(id);
    if (!entity || entity->Has(component)) return false;
    entity->Add(component);
    dirty_ = true;
    return true;
}

bool Scene::RemoveComponent(EntityId id, Component component) {
    Entity* entity = Find(id);
    if (!entity || !entity->Has(component)) return false;
    entity->Remove(component);
    dirty_ = true;
    return true;
}

Entity* Scene::Find(EntityId id) {
    for (auto& entity : entities_) {
        if (entity.id == id) return &entity;
    }
    return nullptr;
}

const Entity* Scene::Find(EntityId id) const {
    for (const auto& entity : entities_) {
        if (entity.id == id) return &entity;
    }
    return nullptr;
}

Entity* Scene::FindByName(std::string_view name) {
    for (auto& entity : entities_) {
        if (entity.name == name) return &entity;
    }
    return nullptr;
}

std::vector<EntityId> Scene::Roots() const {
    std::vector<EntityId> roots;
    for (const auto& entity : entities_) {
        if (entity.parent == kInvalidEntity) roots.push_back(entity.id);
    }
    return roots;
}

std::size_t Scene::ActiveCount() const {
    return static_cast<std::size_t>(
        std::count_if(entities_.begin(), entities_.end(), [](const Entity& e) { return e.active; }));
}

namespace {

// FNV-1a over the fields that change what a frame looks like.
void HashBytes(std::uint64_t& hash, const void* data, std::size_t size) {
    const auto* bytes = static_cast<const unsigned char*>(data);
    for (std::size_t i = 0; i < size; ++i) {
        hash ^= bytes[i];
        hash *= 1099511628211ull;
    }
}

void HashFloat(std::uint64_t& hash, float value) { HashBytes(hash, &value, sizeof(value)); }

void HashVec3(std::uint64_t& hash, const Vec3& value) {
    HashFloat(hash, value.x);
    HashFloat(hash, value.y);
    HashFloat(hash, value.z);
}

}  // namespace

std::uint64_t Scene::ContentHash() const {
    std::uint64_t hash = 1469598103934665603ull;
    for (const Entity& entity : entities_) {
        HashBytes(hash, entity.name.data(), entity.name.size());
        HashBytes(hash, &entity.id, sizeof(entity.id));
        HashBytes(hash, &entity.parent, sizeof(entity.parent));
        const auto active = entity.active ? 1 : 0;
        HashBytes(hash, &active, sizeof(active));
        HashVec3(hash, entity.transform.position);
        HashVec3(hash, entity.transform.rotation);
        HashVec3(hash, entity.transform.scale);
        HashVec3(hash, entity.mesh.size);
        HashVec3(hash, entity.mesh.baseColor);
        HashBytes(hash, &entity.mesh.shape, sizeof(entity.mesh.shape));
        HashBytes(hash, &entity.mesh.meshId, sizeof(entity.mesh.meshId));
        HashBytes(hash, &entity.mesh.visible, sizeof(entity.mesh.visible));
        HashVec3(hash, entity.material.baseColor);
        HashVec3(hash, entity.material.accentColor);
        HashBytes(hash, &entity.material.shader, sizeof(entity.material.shader));
        HashFloat(hash, entity.material.roughness);
        HashFloat(hash, entity.material.metallic);
        HashFloat(hash, entity.material.emissive);
        HashFloat(hash, entity.material.opacity);
        HashFloat(hash, entity.material.uvScale);
        HashFloat(hash, entity.light.intensity);
        HashVec3(hash, entity.light.color);
        HashBytes(hash, &entity.light.type, sizeof(entity.light.type));
        HashFloat(hash, entity.atmosphere.fogDensity);
        HashFloat(hash, entity.atmosphere.cloudCover);
        HashFloat(hash, entity.atmosphere.rainIntensity);
        HashFloat(hash, entity.atmosphere.exposure);
        HashVec3(hash, entity.atmosphere.sunDirection);
    }
    for (const MeshAsset& mesh : meshes_) {
        HashBytes(hash, mesh.name.data(), mesh.name.size());
        HashBytes(hash, mesh.vertices.data(), mesh.vertices.size() * sizeof(MeshVertex));
        HashBytes(hash, mesh.indices.data(), mesh.indices.size() * sizeof(u32));
    }
    return hash;
}

std::string Scene::ToJson() const {
    json::Value root = json::Value::MakeObject();
    root.Set("schema", json::Value(kSceneSchemaVersion));
    root.Set("engine", json::Value(kEngineVersion));
    root.Set("name", json::Value(name_));

    json::Value list = json::Value::MakeArray();
    for (const auto& entity : entities_) {
        json::Value item = json::Value::MakeObject();
        item.Set("id", json::Value(static_cast<long long>(entity.id)));
        item.Set("name", json::Value(entity.name));
        item.Set("tag", json::Value(entity.tag));
        item.Set("active", json::Value(entity.active));
        item.Set("parent", json::Value(static_cast<long long>(entity.parent)));
        item.Set("components", json::Value(static_cast<long long>(entity.components)));

        json::Value transform = json::Value::MakeObject();
        transform.Set("position", Vec3ToJson(entity.transform.position));
        transform.Set("rotation", Vec3ToJson(entity.transform.rotation));
        transform.Set("scale", Vec3ToJson(entity.transform.scale));
        item.Set("transform", std::move(transform));

        if (entity.Has(Component::Mesh)) {
            json::Value mesh = json::Value::MakeObject();
            mesh.Set("shape", json::Value(MeshShapeName(entity.mesh.shape)));
            mesh.Set("size", Vec3ToJson(entity.mesh.size));
            mesh.Set("color", Vec3ToJson(entity.mesh.baseColor));
            mesh.Set("visible", json::Value(entity.mesh.visible));
            mesh.Set("shadows", json::Value(entity.mesh.castShadows));
            mesh.Set("twoSided", json::Value(entity.mesh.twoSided));
            mesh.Set("meshId", json::Value(static_cast<long long>(entity.mesh.meshId)));
            item.Set("mesh", std::move(mesh));
        }
        if (entity.Has(Component::Material)) {
            json::Value material = json::Value::MakeObject();
            material.Set("shader", json::Value(MaterialShaderName(entity.material.shader)));
            material.Set("base", Vec3ToJson(entity.material.baseColor));
            material.Set("accent", Vec3ToJson(entity.material.accentColor));
            material.Set("uvScale", json::Value(static_cast<double>(entity.material.uvScale)));
            material.Set("roughness", json::Value(static_cast<double>(entity.material.roughness)));
            material.Set("metallic", json::Value(static_cast<double>(entity.material.metallic)));
            material.Set("emissive", json::Value(static_cast<double>(entity.material.emissive)));
            material.Set("opacity", json::Value(static_cast<double>(entity.material.opacity)));
            material.Set("fresnel", json::Value(static_cast<double>(entity.material.fresnelPower)));
            item.Set("material", std::move(material));
        }
        if (entity.Has(Component::Particles)) {
            json::Value particles = json::Value::MakeObject();
            particles.Set("preset", json::Value(ParticlePresetName(entity.particles.preset)));
            particles.Set("rate", json::Value(static_cast<double>(entity.particles.rate)));
            particles.Set("lifetime", json::Value(static_cast<double>(entity.particles.lifetime)));
            particles.Set("startSize", json::Value(static_cast<double>(entity.particles.startSize)));
            particles.Set("endSize", json::Value(static_cast<double>(entity.particles.endSize)));
            particles.Set("speed", json::Value(static_cast<double>(entity.particles.speed)));
            particles.Set("gravity", json::Value(static_cast<double>(entity.particles.gravity)));
            particles.Set("spread", json::Value(static_cast<double>(entity.particles.spread)));
            particles.Set("color", Vec3ToJson(entity.particles.color));
            particles.Set("colorEnd", Vec3ToJson(entity.particles.colorEnd));
            particles.Set("opacity", json::Value(static_cast<double>(entity.particles.opacity)));
            particles.Set("additive", json::Value(entity.particles.additive));
            particles.Set("emitRadius", json::Value(static_cast<double>(entity.particles.emitRadius)));
            item.Set("particles", std::move(particles));
        }
        if (entity.Has(Component::Weather)) {
            json::Value weather = json::Value::MakeObject();
            weather.Set("preset", json::Value(StormPresetName(entity.weather.preset)));
            weather.Set("windSpeed", json::Value(static_cast<double>(entity.weather.windSpeedMps)));
            weather.Set("windDirection", json::Value(static_cast<double>(entity.weather.windDirectionDeg)));
            weather.Set("cape", json::Value(static_cast<double>(entity.weather.capeJkg)));
            weather.Set("srh", json::Value(static_cast<double>(entity.weather.stormRelativeHelicity)));
            weather.Set("precip", json::Value(static_cast<double>(entity.weather.precipMmPerHour)));
            weather.Set("temperature", json::Value(static_cast<double>(entity.weather.temperatureC)));
            weather.Set("dewpoint", json::Value(static_cast<double>(entity.weather.dewpointC)));
            weather.Set("inflowDepth", json::Value(static_cast<double>(entity.weather.inflowDepthM)));
            weather.Set("lightning", json::Value(static_cast<double>(entity.weather.lightningPerMinute)));
            item.Set("weather", std::move(weather));
        }
        if (entity.Has(Component::Camera)) {
            json::Value camera = json::Value::MakeObject();
            camera.Set("fov", json::Value(static_cast<double>(entity.camera.fieldOfViewDeg)));
            camera.Set("near", json::Value(static_cast<double>(entity.camera.nearPlane)));
            camera.Set("far", json::Value(static_cast<double>(entity.camera.farPlane)));
            camera.Set("primary", json::Value(entity.camera.primary));
            item.Set("camera", std::move(camera));
        }
        if (entity.Has(Component::Light)) {
            json::Value light = json::Value::MakeObject();
            light.Set("type", json::Value(LightTypeName(entity.light.type)));
            light.Set("color", Vec3ToJson(entity.light.color));
            light.Set("intensity", json::Value(static_cast<double>(entity.light.intensity)));
            light.Set("range", json::Value(static_cast<double>(entity.light.rangeM)));
            light.Set("shadows", json::Value(entity.light.castShadows));
            item.Set("light", std::move(light));
        }
        if (entity.Has(Component::Atmosphere)) {
            json::Value atmosphere = json::Value::MakeObject();
            atmosphere.Set("fog", json::Value(static_cast<double>(entity.atmosphere.fogDensity)));
            atmosphere.Set("cloudCover", json::Value(static_cast<double>(entity.atmosphere.cloudCover)));
            atmosphere.Set("rain", json::Value(static_cast<double>(entity.atmosphere.rainIntensity)));
            atmosphere.Set("exposure", json::Value(static_cast<double>(entity.atmosphere.exposure)));
            item.Set("atmosphere", std::move(atmosphere));
        }
        list.Push(std::move(item));
    }
    root.Set("entities", std::move(list));

    // Custom geometry lives beside the actors so a level is a single self-contained file.
    if (!meshes_.empty()) {
        json::Value library = json::Value::MakeArray();
        for (const auto& asset : meshes_) {
            json::Value entry = json::Value::MakeObject();
            entry.Set("name", json::Value(asset.name));
            entry.Set("albedo", Vec3ToJson(asset.albedo));
            json::Value vertices = json::Value::MakeArray();
            for (const auto& vertex : asset.vertices) {
                vertices.Push(json::Value(static_cast<double>(vertex.position.x)));
                vertices.Push(json::Value(static_cast<double>(vertex.position.y)));
                vertices.Push(json::Value(static_cast<double>(vertex.position.z)));
                vertices.Push(json::Value(static_cast<double>(vertex.normal.x)));
                vertices.Push(json::Value(static_cast<double>(vertex.normal.y)));
                vertices.Push(json::Value(static_cast<double>(vertex.normal.z)));
                vertices.Push(json::Value(static_cast<double>(vertex.u)));
                vertices.Push(json::Value(static_cast<double>(vertex.v)));
            }
            entry.Set("vertices", std::move(vertices));
            json::Value indices = json::Value::MakeArray();
            for (u32 index : asset.indices) {
                indices.Push(json::Value(static_cast<long long>(index)));
            }
            entry.Set("indices", std::move(indices));
            library.Push(std::move(entry));
        }
        root.Set("meshes", std::move(library));
    }
    return root.Dump(true);
}

bool Scene::FromJson(std::string_view text, std::string* error) {
    json::Value root;
    std::string parseError;
    if (!json::Value::Parse(text, root, &parseError)) {
        if (error) *error = "Scene JSON is invalid: " + parseError;
        return false;
    }
    if (!root["entities"].IsArray()) {
        if (error) *error = "Scene JSON has no entities array";
        return false;
    }

    Clear();
    name_ = root["name"].AsString("Untitled Scene");

    const json::Value& library = root["meshes"];
    if (library.IsArray()) {
        for (const auto& entry : library.Elements()) {
            MeshAsset asset;
            asset.name = entry["name"].AsString("Mesh");
            asset.albedo = Vec3FromJson(entry["albedo"]);
            const auto& vertices = entry["vertices"].Elements();
            asset.vertices.reserve(vertices.size() / 8);
            for (std::size_t i = 0; i + 7 < vertices.size(); i += 8) {
                MeshVertex vertex;
                vertex.position = Vec3(static_cast<float>(vertices[i].AsNumber()),
                                       static_cast<float>(vertices[i + 1].AsNumber()),
                                       static_cast<float>(vertices[i + 2].AsNumber()));
                vertex.normal = Vec3(static_cast<float>(vertices[i + 3].AsNumber()),
                                     static_cast<float>(vertices[i + 4].AsNumber()),
                                     static_cast<float>(vertices[i + 5].AsNumber()));
                vertex.u = static_cast<float>(vertices[i + 6].AsNumber());
                vertex.v = static_cast<float>(vertices[i + 7].AsNumber());
                asset.vertices.push_back(vertex);
            }
            for (const auto& index : entry["indices"].Elements()) {
                asset.indices.push_back(static_cast<u32>(index.AsInt()));
            }
            if (!asset.Empty()) meshes_.push_back(std::move(asset));
        }
    }

    for (const auto& item : root["entities"].Elements()) {
        Entity entity;
        entity.id = static_cast<EntityId>(item["id"].AsInt());
        entity.name = item["name"].AsString("Entity");
        entity.tag = item["tag"].AsString();
        entity.active = item["active"].AsBool(true);
        entity.parent = static_cast<EntityId>(item["parent"].AsInt());
        entity.components = static_cast<u32>(item["components"].AsInt());

        const json::Value& transform = item["transform"];
        entity.transform.position = Vec3FromJson(transform["position"]);
        entity.transform.rotation = Vec3FromJson(transform["rotation"]);
        entity.transform.scale = Vec3FromJson(transform["scale"]);

        const json::Value& mesh = item["mesh"];
        if (mesh.IsObject()) {
            entity.Add(Component::Mesh);
            const std::string shape = mesh["shape"].AsString("Cube");
            for (int64_t i = 0; i < kMeshShapeCount; ++i) {
                if (str::EqualsIgnoreCase(shape, kMeshShapes[i])) {
                    entity.mesh.shape = static_cast<MeshShape>(i);
                }
            }
            entity.mesh.size = Vec3FromJson(mesh["size"]);
            entity.mesh.baseColor = Vec3FromJson(mesh["color"]);
            entity.mesh.visible = mesh["visible"].AsBool(true);
            entity.mesh.castShadows = mesh["shadows"].AsBool(true);
            entity.mesh.twoSided = mesh["twoSided"].AsBool(false);
            entity.mesh.meshId = static_cast<u32>(mesh["meshId"].AsInt(0));
        }

        const json::Value& material = item["material"];
        if (material.IsObject()) {
            entity.Add(Component::Material);
            const std::string shader = material["shader"].AsString("Lit");
            for (int64_t i = 0; i < kMaterialShaderCount; ++i) {
                if (str::EqualsIgnoreCase(shader, kMaterialShaders[i])) {
                    entity.material.shader = static_cast<MaterialShader>(i);
                }
            }
            entity.material.baseColor = Vec3FromJson(material["base"]);
            entity.material.accentColor = Vec3FromJson(material["accent"]);
            entity.material.uvScale = static_cast<float>(material["uvScale"].AsNumber(4.0));
            entity.material.roughness = static_cast<float>(material["roughness"].AsNumber(0.45));
            entity.material.metallic = static_cast<float>(material["metallic"].AsNumber(0.0));
            entity.material.emissive = static_cast<float>(material["emissive"].AsNumber(0.0));
            entity.material.opacity = static_cast<float>(material["opacity"].AsNumber(1.0));
            entity.material.fresnelPower = static_cast<float>(material["fresnel"].AsNumber(3.0));
        }

        const json::Value& particles = item["particles"];
        if (particles.IsObject()) {
            entity.Add(Component::Particles);
            const std::string preset = particles["preset"].AsString("Dust");
            for (int64_t i = 0; i < kParticlePresetCount; ++i) {
                if (str::EqualsIgnoreCase(preset, kParticlePresets[i])) {
                    entity.particles.preset = static_cast<ParticlePreset>(i);
                }
            }
            entity.particles.rate = static_cast<float>(particles["rate"].AsNumber(24.0));
            entity.particles.lifetime = static_cast<float>(particles["lifetime"].AsNumber(2.4));
            entity.particles.startSize = static_cast<float>(particles["startSize"].AsNumber(0.35));
            entity.particles.endSize = static_cast<float>(particles["endSize"].AsNumber(0.9));
            entity.particles.speed = static_cast<float>(particles["speed"].AsNumber(1.6));
            entity.particles.gravity = static_cast<float>(particles["gravity"].AsNumber(-0.6));
            entity.particles.spread = static_cast<float>(particles["spread"].AsNumber(0.6));
            entity.particles.color = Vec3FromJson(particles["color"]);
            entity.particles.colorEnd = Vec3FromJson(particles["colorEnd"]);
            entity.particles.opacity = static_cast<float>(particles["opacity"].AsNumber(0.55));
            entity.particles.additive = particles["additive"].AsBool(false);
            entity.particles.emitRadius =
                static_cast<float>(particles["emitRadius"].AsNumber(0.4));
        }

        const json::Value& weather = item["weather"];
        if (weather.IsObject()) {
            entity.Add(Component::Weather);
            const std::string preset = weather["preset"].AsString();
            for (int i = 0; i < kStormPresetCount; ++i) {
                if (str::EqualsIgnoreCase(preset, kStormPresets[i])) {
                    entity.weather.preset = static_cast<StormPreset>(i);
                }
            }
            entity.weather.windSpeedMps = static_cast<float>(weather["windSpeed"].AsNumber(28.0));
            entity.weather.windDirectionDeg = static_cast<float>(weather["windDirection"].AsNumber(205.0));
            entity.weather.capeJkg = static_cast<float>(weather["cape"].AsNumber(2400.0));
            entity.weather.stormRelativeHelicity = static_cast<float>(weather["srh"].AsNumber(180.0));
            entity.weather.precipMmPerHour = static_cast<float>(weather["precip"].AsNumber(12.0));
            entity.weather.temperatureC = static_cast<float>(weather["temperature"].AsNumber(24.0));
            entity.weather.dewpointC = static_cast<float>(weather["dewpoint"].AsNumber(19.0));
            entity.weather.inflowDepthM = static_cast<float>(weather["inflowDepth"].AsNumber(1200.0));
            entity.weather.lightningPerMinute = static_cast<float>(weather["lightning"].AsNumber(3.0));
        }

        const json::Value& camera = item["camera"];
        if (camera.IsObject()) {
            entity.Add(Component::Camera);
            entity.camera.fieldOfViewDeg = static_cast<float>(camera["fov"].AsNumber(65.0));
            entity.camera.nearPlane = static_cast<float>(camera["near"].AsNumber(0.1));
            entity.camera.farPlane = static_cast<float>(camera["far"].AsNumber(2400.0));
            entity.camera.primary = camera["primary"].AsBool(true);
        }

        const json::Value& light = item["light"];
        if (light.IsObject()) {
            entity.Add(Component::Light);
            const std::string type = light["type"].AsString("Directional");
            entity.light.type = type == "Point" ? LightType::Point
                               : (type == "Spot" ? LightType::Spot : LightType::Directional);
            entity.light.color = Vec3FromJson(light["color"]);
            entity.light.intensity = static_cast<float>(light["intensity"].AsNumber(3.2));
            entity.light.rangeM = static_cast<float>(light["range"].AsNumber(60.0));
            entity.light.castShadows = light["shadows"].AsBool(true);
        }

        const json::Value& atmosphere = item["atmosphere"];
        if (atmosphere.IsObject()) {
            entity.Add(Component::Atmosphere);
            entity.atmosphere.fogDensity = static_cast<float>(atmosphere["fog"].AsNumber(0.35));
            entity.atmosphere.cloudCover = static_cast<float>(atmosphere["cloudCover"].AsNumber(0.82));
            entity.atmosphere.rainIntensity = static_cast<float>(atmosphere["rain"].AsNumber(0.4));
            entity.atmosphere.exposure = static_cast<float>(atmosphere["exposure"].AsNumber(1.05));
        }

        if (entity.id >= nextId_) nextId_ = entity.id + 1;
        entities_.push_back(std::move(entity));
    }

    // Rebuild child lists from parent pointers (keeps files human-editable).
    for (auto& entity : entities_) entity.children.clear();
    for (auto& entity : entities_) {
        if (entity.parent == kInvalidEntity) continue;
        if (Entity* parent = Find(entity.parent)) parent->children.push_back(entity.id);
    }
    dirty_ = false;
    return true;
}

Scene Scene::CreateEmptyScene(std::string name) {
    Scene scene;
    scene.SetName(std::move(name));
    scene.CreateEntity("Camera", kInvalidEntity);
    scene.Entities().back().Add(Component::Camera | Component::Light);
    scene.Entities().back().light.type = LightType::Directional;
    scene.dirty_ = false;
    return scene;
}

Scene Scene::CreateSampleLevel(std::string name) {
    Scene scene;
    scene.SetName(std::move(name));

    const EntityId lighting = scene.CreateEntity("Lighting");
    scene.Find(lighting)->Add(Component::Atmosphere);

    {
        Entity* sun = scene.Find(scene.CreateEntity("Directional Light", lighting));
        sun->Add(Component::Light);
        sun->light.type = LightType::Directional;
        sun->light.color = Vec3(1.0f, 0.94f, 0.86f);
        sun->light.intensity = 2.6f;
        sun->transform.rotation = Vec3(-48.0f, -34.0f, 0.0f);
    }
    {
        Entity* sky = scene.Find(scene.CreateEntity("Sky Atmosphere", lighting));
        sky->Add(Component::Atmosphere);
        sky->atmosphere.fogDensity = 0.18f;
        sky->atmosphere.cloudCover = 0.35f;
        sky->atmosphere.rainIntensity = 0.0f;
        sky->atmosphere.exposure = 1.0f;
        sky->atmosphere.sunDirection = Vec3(-0.42f, -0.78f, 0.46f);
    }

    const EntityId geometry = scene.CreateEntity("Geometry");

    {
        Entity* cube = scene.Find(scene.CreateEntity("Cube", geometry));
        cube->Add(Component::Mesh);
        cube->mesh.shape = MeshShape::Cube;
        cube->mesh.size = Vec3(2.0f, 2.0f, 2.0f);
        cube->mesh.baseColor = Vec3(0.86f, 0.36f, 0.34f);
        cube->transform.position = Vec3(-3.2f, 1.0f, 1.4f);
        cube->transform.rotation = Vec3(0.0f, 24.0f, 0.0f);
    }
    {
        Entity* sphere = scene.Find(scene.CreateEntity("Sphere", geometry));
        sphere->Add(Component::Mesh);
        sphere->mesh.shape = MeshShape::Sphere;
        sphere->mesh.size = Vec3(2.2f, 2.2f, 2.2f);
        sphere->mesh.baseColor = Vec3(0.55f, 0.66f, 0.92f);
        sphere->transform.position = Vec3(0.4f, 1.1f, -0.6f);
    }
    {
        Entity* cylinder = scene.Find(scene.CreateEntity("Cylinder", geometry));
        cylinder->Add(Component::Mesh);
        cylinder->mesh.shape = MeshShape::Cylinder;
        cylinder->mesh.size = Vec3(1.6f, 3.0f, 1.6f);
        cylinder->mesh.baseColor = Vec3(0.50f, 0.78f, 0.62f);
        cylinder->transform.position = Vec3(3.6f, 1.5f, 1.0f);
    }
    {
        Entity* cone = scene.Find(scene.CreateEntity("Cone", geometry));
        cone->Add(Component::Mesh);
        cone->mesh.shape = MeshShape::Cone;
        cone->mesh.size = Vec3(1.4f, 2.4f, 1.4f);
        cone->mesh.baseColor = Vec3(0.90f, 0.74f, 0.38f);
        cone->transform.position = Vec3(3.2f, 1.2f, -3.0f);
    }

    const EntityId gameplay = scene.CreateEntity("Gameplay");
    {
        Entity* start = scene.Find(scene.CreateEntity("Player Start", gameplay));
        start->transform.position = Vec3(0.0f, 0.9f, 6.0f);
        start->tag = "gameplay";
    }
    {
        Entity* volume = scene.Find(scene.CreateEntity("Weather Volume", gameplay));
        volume->Add(Component::Weather);
        volume->weather.preset = StormPreset::Supercell;
        volume->transform.position = Vec3(-6.0f, 0.0f, -6.0f);
        volume->transform.scale = Vec3(8.0f, 3.0f, 8.0f);
        volume->tag = "simulation";
    }

    const EntityId cameras = scene.CreateEntity("Cameras");
    {
        Entity* camera = scene.Find(scene.CreateEntity("Camera Rig", cameras));
        camera->Add(Component::Camera);
        camera->camera.fieldOfViewDeg = 55.0f;
        camera->transform.position = Vec3(-6.5f, 4.2f, 9.5f);
        camera->transform.rotation = Vec3(-14.0f, -24.0f, 0.0f);
    }

    scene.dirty_ = false;
    return scene;
}

Scene Scene::CreateStormScene(std::string name) {
    Scene scene;
    scene.SetName(std::move(name));

    const EntityId environment = scene.CreateEntity("Chase Environment");
    scene.Find(environment)->Add(Component::Atmosphere);

    const EntityId cameraId = scene.CreateEntity("Rig / Chase Cam", environment);
    Entity* camera = scene.Find(cameraId);
    camera->transform.position = Vec3(-4.0f, 2.2f, 18.0f);
    camera->transform.rotation = Vec3(-6.0f, 14.0f, 0.0f);
    camera->camera.fieldOfViewDeg = 58.0f;

    const EntityId sun = scene.CreateEntity("Key Light / Sun", environment);
    Entity* sunEntity = scene.Find(sun);
    sunEntity->Add(Component::Light);
    sunEntity->light.color = Vec3(1.0f, 0.83f, 0.65f);
    sunEntity->light.intensity = 2.4f;
    sunEntity->transform.rotation = Vec3(48.0f, -32.0f, 0.0f);

    const EntityId storm = scene.CreateEntity("Supercell 03 / Mesocyclone", environment);
    Entity* stormEntity = scene.Find(storm);
    stormEntity->Add(Component::Weather | Component::Light);
    stormEntity->weather.preset = StormPreset::Supercell;
    stormEntity->transform.position = Vec3(0.0f, 900.0f, 4200.0f);
    stormEntity->weather.windSpeedMps = 31.0f;
    stormEntity->weather.windDirectionDeg = 221.0f;

    const EntityId wall = scene.CreateEntity("Wall Cloud", storm);
    Entity* wallEntity = scene.Find(wall);
    wallEntity->Add(Component::Weather);
    wallEntity->weather.preset = StormPreset::ShelfCloud;
    wallEntity->transform.position = Vec3(120.0f, 640.0f, 3800.0f);

    const EntityId vortex = scene.CreateEntity("Vortex Core", storm);
    Entity* vortexEntity = scene.Find(vortex);
    vortexEntity->Add(Component::Weather | Component::Light);
    vortexEntity->weather.preset = StormPreset::Tornado;
    vortexEntity->weather.stormRelativeHelicity = 340.0f;
    vortexEntity->transform.position = Vec3(60.0f, 60.0f, 3600.0f);
    vortexEntity->light.type = LightType::Point;
    vortexEntity->light.color = Vec3(0.62f, 0.72f, 0.98f);
    vortexEntity->light.intensity = 8.0f;

    const EntityId precip = scene.CreateEntity("Precipitation Curtain", storm);
    Entity* precipEntity = scene.Find(precip);
    precipEntity->Add(Component::Weather);
    precipEntity->weather.preset = StormPreset::HailCore;
    precipEntity->transform.position = Vec3(520.0f, 300.0f, 4400.0f);

    const EntityId chase = scene.CreateEntity("Chase Vehicle");
    Entity* chaseEntity = scene.Find(chase);
    chaseEntity->transform.position = Vec3(-8.0f, 0.0f, 6.0f);
    chaseEntity->transform.rotation = Vec3(0.0f, 198.0f, 0.0f);
    chaseEntity->tag = "gameplay";

    scene.dirty_ = false;
    return scene;
}

}  // namespace astra::scene
