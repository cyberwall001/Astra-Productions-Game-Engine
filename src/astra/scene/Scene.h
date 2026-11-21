// scene/Scene.h - the runtime data the editor manipulates: actors, their components and
// the level's environment. The renderer consumes this; nothing here knows about ImGui.
#pragma once

#include <cstdint>
#include <string>
#include <string_view>
#include <vector>
#include <utility>

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
    Mesh = 1u << 5,
    Material = 1u << 6,
    Particles = 1u << 7,
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

// ---------------------------------------------------------------------------
// Mesh component: what the viewport draws for this actor.
// ---------------------------------------------------------------------------

enum class MeshShape { Cube = 0, Sphere, Cylinder, Cone, Plane, Custom };
const char* MeshShapeName(MeshShape shape);
const char* const* MeshShapeNames(int& count);
constexpr int kMeshShapeCount = 6;

// A shaded vertex. UVs drive the procedural material shaders, so imported meshes keep
// their texture coordinates even though nothing samples a texture yet.
struct MeshVertex {
    Vec3 position;
    Vec3 normal{0.0f, 1.0f, 0.0f};
    float u = 0.0f;
    float v = 0.0f;
};

// Arbitrary triangle geometry: primitives are generated into this shape and imported or
// script-built meshes are stored in it, so the renderer only ever sees one type.
struct MeshAsset {
    std::string name = "Mesh";
    std::vector<MeshVertex> vertices;
    std::vector<u32> indices;
    Vec3 albedo{0.62f, 0.66f, 0.72f};

    bool Empty() const { return vertices.empty() || indices.size() < 3; }
    std::size_t TriangleCount() const { return indices.size() / 3; }
    void RecomputeNormals();
    void Bounds(Vec3& outMin, Vec3& outMax) const;
    void Transform(const Vec3& scale, const Vec3& translation);
};

enum class MaterialShader {
    Lit = 0,       // standard PBR-ish lambert + rim
    Unlit,         // flat albedo, no lighting
    Checker,       // UV checkerboard between base and secondary colour
    Stripes,       // horizontal bands
    Noise,         // value noise fbm
    Gradient,      // vertical gradient between the two colours
    Hologram,      // fresnel rim + scanlines, emissive
    Emissive,      // self-lit surface
};
const char* MaterialShaderName(MaterialShader shader);
const char* const* MaterialShaderNames(int& count);
constexpr int kMaterialShaderCount = 8;

struct MaterialComponent {
    MaterialShader shader = MaterialShader::Lit;
    Vec3 baseColor{0.72f, 0.74f, 0.80f};
    Vec3 accentColor{0.24f, 0.28f, 0.38f};
    float uvScale = 4.0f;
    float roughness = 0.45f;
    float metallic = 0.0f;
    float emissive = 0.0f;
    float opacity = 1.0f;
    float fresnelPower = 3.0f;
};

struct MeshComponent {
    MeshShape shape = MeshShape::Cube;
    u32 meshId = 0;                       // used when shape == Custom (1-based into Scene)
    Vec3 size{1.0f, 1.0f, 1.0f};          // metres along each local axis
    Vec3 baseColor{0.62f, 0.66f, 0.72f};  // linear albedo the viewport shades
    bool visible = true;
    bool castShadows = true;
    bool twoSided = false;
};

// ---------------------------------------------------------------------------
// CPU particles: emitted from an actor, simulated by the editor and drawn as soft
// billboards in the viewport.
// ---------------------------------------------------------------------------

enum class ParticlePreset { Smoke = 0, Sparks, Rain, Dust, Fire, Count };
const char* ParticlePresetName(ParticlePreset preset);
const char* const* ParticlePresetNames(int& count);

struct ParticleComponent {
    ParticlePreset preset = ParticlePreset::Dust;
    float rate = 24.0f;              // particles per second
    float lifetime = 2.4f;           // seconds
    float startSize = 0.35f;         // metres
    float endSize = 0.9f;
    float speed = 1.6f;
    float gravity = -0.6f;
    float spread = 0.6f;
    Vec3 color{0.86f, 0.88f, 0.95f};
    Vec3 colorEnd{0.30f, 0.34f, 0.42f};
    float opacity = 0.55f;
    bool additive = false;
    bool looping = true;
    float emitRadius = 0.4f;
};

void ApplyParticlePreset(ParticleComponent& particles);

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
    MeshComponent mesh;
    MaterialComponent material;
    ParticleComponent particles;
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

    // ---- meshes (custom geometry shared by actors in this level) ------------
    u32 AddMesh(MeshAsset asset);                       // returns a 1-based mesh id
    const MeshAsset* Mesh(u32 id) const;
    u32 FindMesh(std::string_view name) const;
    const std::vector<MeshAsset>& Meshes() const { return meshes_; }

    const std::string& Name() const { return name_; }
    void SetName(std::string name) { name_ = std::move(name); }

    const std::string& SourcePath() const { return sourcePath_; }
    void SetSourcePath(std::string path) { sourcePath_ = std::move(path); }

    bool IsDirty() const { return dirty_; }
    void MarkDirty() { dirty_ = true; }
    void ClearDirty() { dirty_ = false; }

    // Stable 64-bit fingerprint of what the renderer cares about (actors, transforms, meshes,
    // materials, lights, atmosphere). Renderers cache their baked scene against it, so they
    // rebuild only when something actually changed.
    std::uint64_t ContentHash() const;

    std::string ToJson() const;
    bool FromJson(std::string_view text, std::string* error = nullptr);

    // Sample content so the editor is never empty on first open.
    // CreateSampleLevel is the default: a general-purpose level with lighting, a floor,
    // a few primitive actors and one weather volume showing off the simulation module.
    static Scene CreateSampleLevel(std::string name = "Level 01 - Sandbox");
    static Scene CreateStormScene(std::string name = "Storm Sample");
    static Scene CreateEmptyScene(std::string name = "Untitled Scene");

private:
    EntityId PushEntity(Entity entity);

    std::vector<Entity> entities_;
    std::vector<MeshAsset> meshes_;
    EntityId nextId_ = 1;
    std::string name_ = "Untitled Scene";
    std::string sourcePath_;
    bool dirty_ = false;
};

}  // namespace astra::scene
