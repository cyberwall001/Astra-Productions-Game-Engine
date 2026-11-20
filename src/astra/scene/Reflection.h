// scene/Reflection.h - a tiny, hand-rolled inspector model. Editors can render and
// mutate entity components without templating or RTTI magic.
#pragma once

#include <string>
#include <vector>

#include "astra/scene/Scene.h"

namespace astra::scene {

struct PropertyDesc {
    enum class Type { Float, Int, Bool, String, Enum, Vec3, Color, ReadOnly };

    std::string label;
    Type type = Type::Float;
    void* data = nullptr;
    float minValue = 0.0f;
    float maxValue = 1.0f;
    float step = 0.01f;
    const char* const* options = nullptr;
    int optionCount = 0;
    std::string tooltip;
    std::string readOnlyText;
    const char* unit = "";
};

struct PropertyGroup {
    std::string title;
    std::string icon;      // icon id understood by astra::ui::Icon
    bool removable = false;
    Component component = Component::None;
    std::vector<PropertyDesc> properties;
};

// Builds the inspector model for an entity. Pointers reference the entity's own
// storage, so the model must not outlive the entity.
std::vector<PropertyGroup> BuildInspectorModel(Entity& entity);

// Recomputes preset-driven defaults (used when the preset dropdown changes).
void ApplyStormPreset(WeatherComponent& weather);

}  // namespace astra::scene
