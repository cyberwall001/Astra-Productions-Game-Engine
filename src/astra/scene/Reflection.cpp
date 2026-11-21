#include "astra/scene/Reflection.h"

#include <cstdio>

namespace astra::scene {

namespace {

std::string FormatUnit(float value, const char* unit) {
    char buffer[64];
    std::snprintf(buffer, sizeof(buffer), "%.1f %s", static_cast<double>(value), unit);
    return buffer;
}

std::string FormatPercent(float ratio) {
    char buffer[64];
    std::snprintf(buffer, sizeof(buffer), "%.0f%%", static_cast<double>(ratio) * 100.0);
    return buffer;
}

}  // namespace

std::vector<PropertyGroup> BuildInspectorModel(Entity& entity) {
    std::vector<PropertyGroup> groups;

    {
        PropertyGroup group;
        group.title = "Actor";
        group.icon = "tag";

        PropertyDesc name;
        name.label = "Name";
        name.type = PropertyDesc::Type::String;
        name.data = &entity.name;
        group.properties.push_back(std::move(name));

        PropertyDesc tag;
        tag.label = "Tag";
        tag.type = PropertyDesc::Type::String;
        tag.data = &entity.tag;
        tag.tooltip = "Free-form label used by gameplay code to find this entity.";
        group.properties.push_back(std::move(tag));

        PropertyDesc active;
        active.label = "Active";
        active.type = PropertyDesc::Type::Bool;
        active.data = &entity.active;
        group.properties.push_back(std::move(active));

        groups.push_back(std::move(group));
    }

    {
        PropertyGroup group;
        group.title = "Transform";
        group.icon = "move";
        // Every actor has a transform, so it is never offered for removal.
        group.removable = false;

        PropertyDesc position;
        position.label = "Location";
        position.type = PropertyDesc::Type::Vec3;
        position.data = &entity.transform.position;
        position.minValue = -100000.0f;
        position.maxValue = 100000.0f;
        position.step = 0.1f;
        position.unit = "m";
        group.properties.push_back(std::move(position));

        PropertyDesc rotation;
        rotation.label = "Rotation";
        rotation.type = PropertyDesc::Type::Vec3;
        rotation.data = &entity.transform.rotation;
        rotation.minValue = -360.0f;
        rotation.maxValue = 360.0f;
        rotation.step = 0.5f;
        rotation.unit = "deg";
        group.properties.push_back(std::move(rotation));

        PropertyDesc scale;
        scale.label = "Scale";
        scale.type = PropertyDesc::Type::Vec3;
        scale.data = &entity.transform.scale;
        scale.minValue = 0.001f;
        scale.maxValue = 1000.0f;
        scale.step = 0.01f;
        group.properties.push_back(std::move(scale));

        groups.push_back(std::move(group));
    }

    if (entity.Has(Component::Mesh)) {
        PropertyGroup group;
        group.title = "Static Mesh";
        group.icon = "cube";
        group.removable = true;
        group.component = Component::Mesh;

        int shapeCount = 0;
        const char* const* shapes = MeshShapeNames(shapeCount);

        PropertyDesc shape;
        shape.label = "Shape";
        shape.type = PropertyDesc::Type::Enum;
        shape.data = &entity.mesh.shape;
        shape.options = shapes;
        shape.optionCount = shapeCount;
        group.properties.push_back(std::move(shape));

        PropertyDesc size;
        size.label = "Size";
        size.type = PropertyDesc::Type::Vec3;
        size.data = &entity.mesh.size;
        size.minValue = 0.01f;
        size.maxValue = 400.0f;
        size.step = 0.1f;
        size.unit = "m";
        group.properties.push_back(std::move(size));

        PropertyDesc color;
        color.label = "Base color";
        color.type = PropertyDesc::Type::Color;
        color.data = &entity.mesh.baseColor;
        group.properties.push_back(std::move(color));

        PropertyDesc visible;
        visible.label = "Visible";
        visible.type = PropertyDesc::Type::Bool;
        visible.data = &entity.mesh.visible;
        group.properties.push_back(std::move(visible));

        PropertyDesc shadows;
        shadows.label = "Cast shadows";
        shadows.type = PropertyDesc::Type::Bool;
        shadows.data = &entity.mesh.castShadows;
        group.properties.push_back(std::move(shadows));

        PropertyDesc twoSided;
        twoSided.label = "Two sided";
        twoSided.type = PropertyDesc::Type::Bool;
        twoSided.data = &entity.mesh.twoSided;
        twoSided.tooltip = "Draw back faces as well - useful for interior shells.";
        group.properties.push_back(std::move(twoSided));

        groups.push_back(std::move(group));
    }

    if (entity.Has(Component::Weather)) {
        PropertyGroup group;
        group.title = "Weather Field";
        group.icon = "wind";
        group.removable = true;
        group.component = Component::Weather;

        int presetCount = 0;
        const char* const* presets = StormPresetNames(presetCount);

        PropertyDesc preset;
        preset.label = "Storm preset";
        preset.type = PropertyDesc::Type::Enum;
        preset.data = &entity.weather.preset;
        preset.options = presets;
        preset.optionCount = presetCount;
        preset.tooltip = "Applies a tuned baseline to every weather value below.";
        group.properties.push_back(std::move(preset));

        PropertyDesc wind;
        wind.label = "Wind speed";
        wind.type = PropertyDesc::Type::Float;
        wind.data = &entity.weather.windSpeedMps;
        wind.minValue = 0.0f;
        wind.maxValue = 80.0f;
        wind.step = 0.5f;
        wind.unit = "m/s";
        group.properties.push_back(std::move(wind));

        PropertyDesc direction;
        direction.label = "Wind direction";
        direction.type = PropertyDesc::Type::Float;
        direction.data = &entity.weather.windDirectionDeg;
        direction.minValue = 0.0f;
        direction.maxValue = 359.0f;
        direction.step = 1.0f;
        direction.unit = "deg";
        group.properties.push_back(std::move(direction));

        PropertyDesc cape;
        cape.label = "CAPE";
        cape.type = PropertyDesc::Type::Float;
        cape.data = &entity.weather.capeJkg;
        cape.minValue = 0.0f;
        cape.maxValue = 8000.0f;
        cape.step = 10.0f;
        cape.unit = "J/kg";
        cape.tooltip = "Convective available potential energy - the storm's fuel.";
        group.properties.push_back(std::move(cape));

        PropertyDesc srh;
        srh.label = "Storm helicity";
        srh.type = PropertyDesc::Type::Float;
        srh.data = &entity.weather.stormRelativeHelicity;
        srh.minValue = 0.0f;
        srh.maxValue = 800.0f;
        srh.step = 1.0f;
        srh.unit = "m2/s2";
        group.properties.push_back(std::move(srh));

        PropertyDesc precip;
        precip.label = "Precipitation";
        precip.type = PropertyDesc::Type::Float;
        precip.data = &entity.weather.precipMmPerHour;
        precip.minValue = 0.0f;
        precip.maxValue = 120.0f;
        precip.step = 0.5f;
        precip.unit = "mm/h";
        group.properties.push_back(std::move(precip));

        PropertyDesc temp;
        temp.label = "Temperature";
        temp.type = PropertyDesc::Type::Float;
        temp.data = &entity.weather.temperatureC;
        temp.minValue = -40.0f;
        temp.maxValue = 50.0f;
        temp.step = 0.5f;
        temp.unit = "C";
        group.properties.push_back(std::move(temp));

        PropertyDesc dew;
        dew.label = "Dew point";
        dew.type = PropertyDesc::Type::Float;
        dew.data = &entity.weather.dewpointC;
        dew.minValue = -40.0f;
        dew.maxValue = 40.0f;
        dew.step = 0.5f;
        dew.unit = "C";
        group.properties.push_back(std::move(dew));

        PropertyDesc inflow;
        inflow.label = "Inflow depth";
        inflow.type = PropertyDesc::Type::Float;
        inflow.data = &entity.weather.inflowDepthM;
        inflow.minValue = 200.0f;
        inflow.maxValue = 4000.0f;
        inflow.step = 25.0f;
        inflow.unit = "m";
        group.properties.push_back(std::move(inflow));

        PropertyDesc lightning;
        lightning.label = "Lightning";
        lightning.type = PropertyDesc::Type::Float;
        lightning.data = &entity.weather.lightningPerMinute;
        lightning.minValue = 0.0f;
        lightning.maxValue = 30.0f;
        lightning.step = 0.5f;
        lightning.unit = "/min";
        group.properties.push_back(std::move(lightning));

        PropertyDesc rotate;
        rotate.label = "Rotate with wind";
        rotate.type = PropertyDesc::Type::Bool;
        rotate.data = &entity.weather.rotateWithWind;
        group.properties.push_back(std::move(rotate));

        groups.push_back(std::move(group));

        PropertyGroup derived;
        derived.title = "Derived Readouts";
        derived.icon = "gauge";

        PropertyDesc shear;
        shear.label = "0-3km shear";
        shear.type = PropertyDesc::Type::ReadOnly;
        shear.readOnlyText = FormatUnit(entity.weather.InflowLayerShear(), "m/s");
        derived.properties.push_back(std::move(shear));

        PropertyDesc probability;
        probability.label = "Tornado potential";
        probability.type = PropertyDesc::Type::ReadOnly;
        probability.readOnlyText = FormatPercent(entity.weather.TornadoProbability());
        derived.properties.push_back(std::move(probability));

        groups.push_back(std::move(derived));
    }

    if (entity.Has(Component::Camera)) {
        PropertyGroup group;
        group.title = "Camera";
        group.icon = "camera";
        group.removable = true;
        group.component = Component::Camera;

        PropertyDesc fov;
        fov.label = "Field of view";
        fov.type = PropertyDesc::Type::Float;
        fov.data = &entity.camera.fieldOfViewDeg;
        fov.minValue = 10.0f;
        fov.maxValue = 140.0f;
        fov.step = 0.5f;
        fov.unit = "deg";
        group.properties.push_back(std::move(fov));

        PropertyDesc nearPlane;
        nearPlane.label = "Near plane";
        nearPlane.type = PropertyDesc::Type::Float;
        nearPlane.data = &entity.camera.nearPlane;
        nearPlane.minValue = 0.01f;
        nearPlane.maxValue = 10.0f;
        nearPlane.step = 0.01f;
        group.properties.push_back(std::move(nearPlane));

        PropertyDesc farPlane;
        farPlane.label = "Far plane";
        farPlane.type = PropertyDesc::Type::Float;
        farPlane.data = &entity.camera.farPlane;
        farPlane.minValue = 100.0f;
        farPlane.maxValue = 40000.0f;
        farPlane.step = 10.0f;
        group.properties.push_back(std::move(farPlane));

        PropertyDesc primary;
        primary.label = "Primary camera";
        primary.type = PropertyDesc::Type::Bool;
        primary.data = &entity.camera.primary;
        group.properties.push_back(std::move(primary));

        groups.push_back(std::move(group));
    }

    if (entity.Has(Component::Light)) {
        PropertyGroup group;
        group.title = "Light";
        group.icon = "light";
        group.removable = true;
        group.component = Component::Light;

        static const char* kLightTypes[] = {"Directional", "Point", "Spot"};

        PropertyDesc type;
        type.label = "Type";
        type.type = PropertyDesc::Type::Enum;
        type.data = &entity.light.type;
        type.options = kLightTypes;
        type.optionCount = 3;
        group.properties.push_back(std::move(type));

        PropertyDesc color;
        color.label = "Colour";
        color.type = PropertyDesc::Type::Color;
        color.data = &entity.light.color;
        group.properties.push_back(std::move(color));

        PropertyDesc intensity;
        intensity.label = "Intensity";
        intensity.type = PropertyDesc::Type::Float;
        intensity.data = &entity.light.intensity;
        intensity.minValue = 0.0f;
        intensity.maxValue = 40.0f;
        intensity.step = 0.1f;
        group.properties.push_back(std::move(intensity));

        PropertyDesc range;
        range.label = "Range";
        range.type = PropertyDesc::Type::Float;
        range.data = &entity.light.rangeM;
        range.minValue = 1.0f;
        range.maxValue = 2000.0f;
        range.step = 1.0f;
        range.unit = "m";
        group.properties.push_back(std::move(range));

        PropertyDesc shadows;
        shadows.label = "Cast shadows";
        shadows.type = PropertyDesc::Type::Bool;
        shadows.data = &entity.light.castShadows;
        group.properties.push_back(std::move(shadows));

        groups.push_back(std::move(group));
    }

    if (entity.Has(Component::Atmosphere)) {
        PropertyGroup group;
        group.title = "Atmosphere";
        group.icon = "cloud";
        group.removable = true;
        group.component = Component::Atmosphere;

        PropertyDesc fog;
        fog.label = "Fog density";
        fog.type = PropertyDesc::Type::Float;
        fog.data = &entity.atmosphere.fogDensity;
        fog.minValue = 0.0f;
        fog.maxValue = 1.0f;
        fog.step = 0.005f;
        group.properties.push_back(std::move(fog));

        PropertyDesc clouds;
        clouds.label = "Cloud cover";
        clouds.type = PropertyDesc::Type::Float;
        clouds.data = &entity.atmosphere.cloudCover;
        clouds.minValue = 0.0f;
        clouds.maxValue = 1.0f;
        clouds.step = 0.005f;
        group.properties.push_back(std::move(clouds));

        PropertyDesc rain;
        rain.label = "Rain intensity";
        rain.type = PropertyDesc::Type::Float;
        rain.data = &entity.atmosphere.rainIntensity;
        rain.minValue = 0.0f;
        rain.maxValue = 1.0f;
        rain.step = 0.005f;
        group.properties.push_back(std::move(rain));

        PropertyDesc sun;
        sun.label = "Sun direction";
        sun.type = PropertyDesc::Type::Vec3;
        sun.data = &entity.atmosphere.sunDirection;
        sun.minValue = -1.0f;
        sun.maxValue = 1.0f;
        sun.step = 0.01f;
        group.properties.push_back(std::move(sun));

        PropertyDesc exposure;
        exposure.label = "Exposure";
        exposure.type = PropertyDesc::Type::Float;
        exposure.data = &entity.atmosphere.exposure;
        exposure.minValue = 0.1f;
        exposure.maxValue = 4.0f;
        exposure.step = 0.01f;
        group.properties.push_back(std::move(exposure));

        groups.push_back(std::move(group));
    }

    return groups;
}

}  // namespace astra::scene
