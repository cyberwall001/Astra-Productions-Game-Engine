// scene/Scene.h - the runtime data the editor manipulates: entities, components and
// the project's storm-chasing weather fields. No rendering, by design (milestone 1).
#pragma once

#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

#include "astra/core/Types.h"

namespace astra::scene {

using EntityId = u32;
inline constexpr EntityId kInvalidEntity = 0;

struct Vec3 {
    float x = 0.0f;
    float y = 0.0f;
    float z = 0.0f;

    Vec3() = default;
    Vec3(float x_, float y_, float z_) : x(x_), y(y_), z(z_) {}
};

enum class Component : u32 {
    None = 0,
    Transform = 1u << 0,
    Weather = 1u << 1,
    Camera = 1u << 2,
    Light = 1u << 3,
    Atmosphere = 1u << 4,
};
inline Component operator|(Component a, Component b) {
    return static_cast<Component>(static_cast<u32>(a) | static_cast<u32>(b));
}
inline bool HasFlag(u32 mask, Component flag) {
    return (mask & static_cast<u32>(flag)) != 0;
}

struct TransformComponent {
    Vec3 position{0.0f, 0.0f, 0.0f};
    Vec3 rotation{0.0f, 0.0f, 0.0f};
    Vec3 scale{1.0f, 1.0f, 1.0f};
};

enum class StormPreset { Clear = 0, ShelfCloud, Supercell, Tornado, Derecho, HailCore };
const char* StormPresetName(StormPreset preset);
const char* const* StormPresetNames(int& count);

struct WeatherComponent {
    StormPreset preset = StormPreset::Supercell;
    float windSpeedMps = 28.0f;
    float windDirectionDeg = 205.0f;
    float capeJkg = 2400.0f;
    float stormRelativeHelicity = 180.0f;
    float precipMmPerHour = 12.0f;
    float temperatureC = 24.0f;
    float dewpointC = 19.0f;
    float inflowDepthM = 1200.0f;
    float lightningPerMinute = 3.0f;
    bool rotateWithWind = true;

    // Derived, read-only readouts used by the editor UI.
    float InflowLayerShear() const;   // m/s over the 0-3km layer
    float TornadoProbability() const; // 0..1 heuristic from CAPE + SRH
};

// Applies the tuned baseline values for the current storm preset.
void ApplyStormPreset(WeatherComponent& weather);

struct CameraComponent {
    float fieldOfViewDeg = 65.0f;
    float nearPlane = 0.1f;
    float farPlane = 2400.0f;
    bool primary = true;
    bool affectedByFog = true;
};

enum class LightType { Directional = 0, Point, Spot };
const char* LightTypeName(LightType type);

struct LightComponent {
    LightType type = LightType::Directional;
    Vec3 color{1.0f, 0.96f, 0.90f};
    float intensity = 3.2f;
    float rangeM = 60.0f;
    bool castShadows = true;
};

struct AtmosphereComponent {
    float fogDensity = 0.35f;
    float cloudCover = 0.82f;
    float rainIntensity = 0.4f;
    Vec3 sunDirection{-0.35f, -0.72f, 0.60f};
    float exposure = 1.05f;
};

struct Entity {
    EntityId id = kInvalidEntity;
    std::string name = "Entity";
    std::string tag;
    bool active = true;
    bool expanded = true;
    EntityId parent = kInvalidEntity;
    std::vector<EntityId> children;
    u32 components = 0;

    TransformComponent transform;
    WeatherComponent weather;
    CameraComponent camera;
    LightComponent light;
    AtmosphereComponent atmosphere;

    bool Has(Component component) const { return HasFlag(components, component); }
    void Add(Component component) { components |= static_cast<u32>(component); }
    void Remove(Component component);
};

class Scene {
public:
    Scene();

    void Clear();
    EntityId CreateEntity(std::string name, EntityId parent = kInvalidEntity);
    bool DestroyEntity(EntityId id, bool includeChildren = true);
    bool RenameEntity(EntityId id, std::string name);
    bool Reparent(EntityId id, EntityId newParent);
    bool SetActive(EntityId id, bool active);
    bool AddComponent(EntityId id, Component component);
    bool RemoveComponent(EntityId id, Component component);

    Entity* Find(EntityId id);
    const Entity* Find(EntityId id) const;
    Entity* FindByName(std::string_view name);

    const std::vector<Entity>& Entities() const { return entities_; }
    std::vector<Entity>& Entities() { return entities_; }
    std::vector<EntityId> Roots() const;
    std::size_t Count() const { return entities_.size(); }
    std::size_t ActiveCount() const;

    const std::string& Name() const { return name_; }
    void SetName(std::string name) { name_ = std::move(name); }

    const std::string& SourcePath() const { return sourcePath_; }
    void SetSourcePath(std::string path) { sourcePath_ = std::move(path); }

    bool IsDirty() const { return dirty_; }
    void MarkDirty() { dirty_ = true; }
    void ClearDirty() { dirty_ = false; }

    std::string ToJson() const;
    bool FromJson(std::string_view text, std::string* error = nullptr);

    // Sample content so the editor is never empty on first open.
    static Scene CreateStormScene(std::string name = "Supercell Intercept");
    static Scene CreateEmptyScene(std::string name = "Untitled Scene");

private:
    EntityId PushEntity(Entity entity);

    std::vector<Entity> entities_;
    EntityId nextId_ = 1;
    std::string name_ = "Untitled Scene";
    std::string sourcePath_;
    bool dirty_ = false;
};

}  // namespace astra::scene
