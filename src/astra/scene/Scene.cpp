#include "astra/scene/Scene.h"

#include <algorithm>
#include <cmath>

#include "astra/core/Strings.h"
#include "astra/core/Version.h"
#include "astra/data/Json.h"

namespace astra::scene {
namespace {

const char* const kStormPresets[] = {"Clear", "Shelf cloud", "Supercell", "Tornado",
                                     "Derecho", "Hail core"};
constexpr int kStormPresetCount = static_cast<int>(sizeof(kStormPresets) / sizeof(kStormPresets[0]));

}  // namespace

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
    entity.Add(Component::Transform | Component::Camera);
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
    scene.Entities().back().Add(Component::Light);
    scene.Entities().back().light.type = LightType::Directional;
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
