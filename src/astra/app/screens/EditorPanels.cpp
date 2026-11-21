// app/screens/EditorPanels.cpp - implementation of the editor's panels.
#include "astra/app/screens/EditorPanels.h"

#include "astra/app/screens/Common.h"

#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <numeric>
#include <cmath>

#include "astra/core/Log.h"
#include "astra/core/Strings.h"
#include "astra/core/Time.h"
#include "astra/render/ViewportRenderer.h"
#include "astra/scene/Reflection.h"
#include "astra/ui/Widgets.h"
#include "imgui_internal.h"

namespace astra::app::panels {
namespace {

using ui::Colors;
using ui::Text;
using ui::U;
using scene::EntityId;

constexpr float kHeaderHeight = 30.0f;
constexpr float kRowHeight = 26.0f;

ImU32 SeverityColor(log::Level level) {
    switch (level) {
        case log::Level::Warn: return ui::Colors().warn;
        case log::Level::Error:
        case log::Level::Critical: return ui::Colors().danger;
        case log::Level::Debug:
        case log::Level::Trace: return ui::Colors().textFaint;
        default: return ui::Colors().textDim;
    }
}

// ---------------------------------------------------------------------------
// Inspector property rows
// ---------------------------------------------------------------------------

void DrawPropertyRow(scene::PropertyDesc& property, float rowWidth) {
    ImGui::PushID(property.label.c_str());
    ImGuiWindow* window = ImGui::GetCurrentWindow();
    const ImVec2 pos = ImGui::GetCursorScreenPos();

    // The child can gain a scrollbar mid-frame, so never trust the caller's width blindly.
    const float available = ImMax(U(80.0f), ImMin(rowWidth, ImGui::GetContentRegionAvail().x));
    const float labelWidth = ImFloor(available * 0.42f);
    const bool hasUnit = property.unit != nullptr && property.unit[0] != '\0';
    const float unitWidth = hasUnit ? U(24.0f) : 0.0f;
    const float controlWidth = ImMax(U(48.0f), available - labelWidth - unitWidth);

    // Label, clipped so long names never run into the control column.
    window->DrawList->PushClipRect(pos, ImVec2(pos.x + labelWidth - U(6.0f), pos.y + U(kRowHeight)),
                                   true);
    window->DrawList->AddText(Text().small, Text().small->FontSize,
                              ImVec2(pos.x, pos.y + U(4.0f)), Colors().textDim, property.label.c_str());
    window->DrawList->PopClipRect();

    ImGui::SetCursorScreenPos(ImVec2(pos.x + labelWidth, pos.y));
    ImGui::PushItemWidth(controlWidth);

    switch (property.type) {
        case scene::PropertyDesc::Type::Float: {
            float* value = static_cast<float*>(property.data);
            ImGui::PushStyleColor(ImGuiCol_FrameBg, ui::ToVec4(Colors().surfaceSunken));
            ImGui::PushStyleColor(ImGuiCol_FrameBgHovered, ui::ToVec4(Colors().surfaceHover));
            ImGui::PushStyleColor(ImGuiCol_FrameBgActive, ui::ToVec4(Colors().surfaceHover));
            ImGui::PushStyleColor(ImGuiCol_SliderGrab, ui::ToVec4(Colors().brand));
            ImGui::PushStyleColor(ImGuiCol_SliderGrabActive, ui::ToVec4(Colors().brandBright));
            ImGui::PushStyleColor(ImGuiCol_Text, ui::ToVec4(Colors().textDim));
            ImGui::DragFloat("##f", value, property.step > 0.0f ? property.step : 0.01f,
                             property.minValue, property.maxValue, "%.2f");
            ImGui::PopStyleColor(6);
            if (hasUnit) {
                // Unit sits inside the reserved gutter instead of after the widget.
                const float unitX = pos.x + available - unitWidth;
                window->DrawList->AddText(Text().tiny, Text().tiny->FontSize,
                                          ImVec2(unitX, pos.y + U(7.0f)), Colors().textFaint,
                                          property.unit);
            }
            break;
        }
        case scene::PropertyDesc::Type::Int: {
            int* value = static_cast<int*>(property.data);
            ImGui::PushStyleColor(ImGuiCol_FrameBg, ui::ToVec4(Colors().surfaceSunken));
            ImGui::DragInt("##i", value, property.step > 0.0f ? property.step : 1.0f,
                           static_cast<int>(property.minValue), static_cast<int>(property.maxValue),
                           "%d");
            ImGui::PopStyleColor();
            if (hasUnit) {
                window->DrawList->AddText(Text().tiny, Text().tiny->FontSize,
                                          ImVec2(pos.x + available - unitWidth, pos.y + U(7.0f)),
                                          Colors().textFaint, property.unit);
            }
            break;
        }
        case scene::PropertyDesc::Type::Bool: {
            bool* value = static_cast<bool*>(property.data);
            ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(0, 0));
            ImGui::Checkbox("##b", value);
            ImGui::PopStyleVar();
            break;
        }
        case scene::PropertyDesc::Type::String: {
            auto* value = static_cast<std::string*>(property.data);
            ImGui::PushStyleColor(ImGuiCol_FrameBg, ui::ToVec4(Colors().surfaceSunken));
            ImGui::PushStyleColor(ImGuiCol_Text, ui::ToVec4(Colors().text));
            char buffer[128];
            std::snprintf(buffer, sizeof(buffer), "%s", value->c_str());
            if (ImGui::InputText("##s", buffer, sizeof(buffer))) *value = buffer;
            ImGui::PopStyleColor(2);
            break;
        }
        case scene::PropertyDesc::Type::Enum: {
            auto* value = static_cast<int*>(property.data);
            const int current = *value;
            const std::string preview =
                (property.options && current >= 0 && current < property.optionCount)
                    ? property.options[current]
                    : "None";
            if (ImGui::Button(preview.c_str(), ImVec2(controlWidth, 0))) ImGui::OpenPopup("##enum");
            if (ImGui::BeginPopup("##enum")) {
                for (int i = 0; i < property.optionCount; ++i) {
                    if (ImGui::Selectable(property.options[i], i == current)) *value = i;
                }
                ImGui::EndPopup();
            }
            break;
        }
        case scene::PropertyDesc::Type::Vec3: {
            // Vectors get their own line so each axis field stays readable.
            auto* value = static_cast<scene::Vec3*>(property.data);
            const float fieldWidth = ImFloor((available - U(8.0f)) / 3.0f);
            float* components[3] = {&value->x, &value->y, &value->z};
            static const char* kFormats[3] = {"X %.2f", "Y %.2f", "Z %.2f"};
            ImGui::SetCursorScreenPos(ImVec2(pos.x, pos.y + U(20.0f)));
            for (int axis = 0; axis < 3; ++axis) {
                ImGui::PushID(axis);
                ImGui::SetNextItemWidth(fieldWidth);
                ImGui::PushStyleColor(ImGuiCol_FrameBg, ui::ToVec4(Colors().surfaceSunken));
                ImGui::PushStyleColor(ImGuiCol_Text, ui::ToVec4(Colors().textFaint));
                ImGui::DragFloat("##axis", components[axis],
                                 property.step > 0.0f ? property.step : 0.05f, property.minValue,
                                 property.maxValue, kFormats[axis]);
                ImGui::PopStyleColor(2);
                ImGui::PopID();
                if (axis < 2) ImGui::SameLine(0.0f, U(4.0f));
            }
            ImGui::PopItemWidth();
            ImGui::SetCursorScreenPos(ImVec2(pos.x, pos.y + U(52.0f)));
            ImGui::PopID();
            return;
        }
        case scene::PropertyDesc::Type::Color: {
            auto* value = static_cast<scene::Vec3*>(property.data);
            float color[3] = {value->x, value->y, value->z};
            const float swatch = ImMin(controlWidth, U(46.0f));
            ImGui::SetNextItemWidth(swatch);
            ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(U(3.0f), U(3.0f)));
            if (ImGui::ColorEdit3("##c", color, ImGuiColorEditFlags_NoInputs)) {
                value->x = color[0];
                value->y = color[1];
                value->z = color[2];
            }
            ImGui::PopStyleVar();
            const std::string hex = str::Format("#%02X%02X%02X",
                                                static_cast<int>(value->x * 255.0f + 0.5f),
                                                static_cast<int>(value->y * 255.0f + 0.5f),
                                                static_cast<int>(value->z * 255.0f + 0.5f));
            window->DrawList->AddText(Text().monoSmall, Text().monoSmall->FontSize,
                                      ImVec2(pos.x + labelWidth + swatch + U(10.0f), pos.y + U(6.0f)),
                                      Colors().textDim, hex.c_str());
            break;
        }
        case scene::PropertyDesc::Type::ReadOnly: {
            const float textWidth = Text().monoSmall->CalcTextSizeA(
                Text().monoSmall->FontSize, FLT_MAX, 0.0f, property.readOnlyText.c_str()).x;
            ImGui::SetCursorScreenPos(
                ImVec2(pos.x + available - ImMin(textWidth, controlWidth), pos.y + U(3.0f)));
            ImGui::PushStyleColor(ImGuiCol_Text, ui::ToVec4(Colors().brand));
            ui::PushFont(Text().monoSmall);
            ImGui::TextUnformatted(property.readOnlyText.c_str());
            ui::PopFont();
            ImGui::PopStyleColor();
            break;
        }
        default: break;
    }

    ImGui::PopItemWidth();
    ImGui::SetCursorScreenPos(ImVec2(pos.x, pos.y + U(kRowHeight)));
    ImGui::PopID();
}

// ---------------------------------------------------------------------------
// Hierarchy
// ---------------------------------------------------------------------------

void DrawEntityRow(AstraApp& app, scene::Scene& scene, EntityId id, int depth, float rowWidth) {
    scene::Entity* entity = scene.Find(id);
    if (!entity) return;

    ImGui::PushID(static_cast<int>(id));
    const ImVec2 pos = ImGui::GetCursorScreenPos();
    const bool selected = app.editor.selected == id;
    const bool hasChildren = !entity->children.empty();

    if (depth == 0 && !app.editor.hierarchySearch.empty()) {
        const std::string needle = str::ToLower(app.editor.hierarchySearch);
        if (!str::Contains(str::ToLower(entity->name), needle)) {
            ImGui::PopID();
            return;
        }
    }

    const float rowHeight = U(kRowHeight);
    ImGui::InvisibleButton("##row", ImVec2(rowWidth, rowHeight));
    const bool hovered = ImGui::IsItemHovered();
    if (ImGui::IsItemClicked()) app.editor.selected = id;
    if (ImGui::IsItemClicked(ImGuiMouseButton_Right)) {
        app.editor.selected = id;
        ImGui::OpenPopup("##entity_menu");
    }

    ImDrawList* dl = ImGui::GetWindowDrawList();
    if (selected) {
        dl->AddRectFilled(pos, ImVec2(pos.x + rowWidth, pos.y + rowHeight),
                          ui::Fade(Colors().brand, 0.16f), U(7.0f));
        dl->AddRectFilled(pos, ImVec2(pos.x + U(2.5f), pos.y + rowHeight), Colors().brand, U(2.0f));
    } else if (hovered) {
        dl->AddRectFilled(pos, ImVec2(pos.x + rowWidth, pos.y + rowHeight), Colors().surfaceHover,
                          U(7.0f));
    }

    const float indent = U(8.0f) + static_cast<float>(depth) * U(14.0f);
    if (hasChildren) {
        ui::DrawIcon(dl, entity->expanded ? ui::Icon::ChevronDown : ui::Icon::ChevronRight,
                     ImVec2(pos.x + indent + U(5.0f), pos.y + rowHeight * 0.5f), U(11.0f),
                     Colors().textFaint, U(1.4f));
        if (ImGui::IsItemClicked() && ImGui::GetIO().KeyAlt) {
            entity->expanded = !entity->expanded;
        }
    }

    const ImVec2 iconCenter(pos.x + indent + U(19.0f), pos.y + rowHeight * 0.5f);
    const bool isWeather = entity->Has(scene::Component::Weather);
    ui::DrawIcon(dl, isWeather ? ui::Icon::Wind : ui::Icon::Cube, iconCenter, U(13.0f),
                 entity->active ? (isWeather ? Colors().info : Colors().textDim)
                                : Colors().textFaint,
                 U(1.5f));

    const ImU32 nameColor = !entity->active ? Colors().textFaint
                                            : (selected ? Colors().text : Colors().textDim);
    dl->AddText(Text().small, Text().small->FontSize,
                ImVec2(iconCenter.x + U(12.0f), pos.y + (rowHeight - Text().small->FontSize) * 0.5f - U(1.0f)),
                nameColor, str::Ellipsis(entity->name, 26).c_str());

    if (isWeather) {
        const float badgeWidth = U(30.0f);
        dl->AddText(Text().tiny, Text().tiny->FontSize,
                    ImVec2(pos.x + rowWidth - badgeWidth, pos.y + rowHeight * 0.5f - U(6.0f)),
                    ui::Fade(Colors().info, 0.8f), "WX");
    }

    if (ImGui::BeginPopup("##entity_menu")) {
        if (ImGui::MenuItem("Add child entity")) {
            const EntityId child = scene.CreateEntity("New Entity", id);
            entity->expanded = true;
            app.editor.selected = child;
        }
        if (ImGui::MenuItem("Add weather field")) {
            scene.AddComponent(id, scene::Component::Weather);
        }
        if (ImGui::MenuItem("Add light")) scene.AddComponent(id, scene::Component::Light);
        ImGui::Separator();
        if (ImGui::MenuItem(entity->active ? "Deactivate" : "Activate")) {
            scene.SetActive(id, !entity->active);
        }
        if (ImGui::MenuItem("Delete entity", "Del")) {
            const EntityId next = entity->parent;
            scene.DestroyEntity(id);
            app.editor.selected = next;
            ImGui::EndPopup();
            ImGui::PopID();
            return;
        }
        ImGui::EndPopup();
    }

    if (entity->expanded) {
        for (EntityId child : entity->children) {
            DrawEntityRow(app, scene, child, depth + 1, rowWidth - U(6.0f));
        }
    }
    ImGui::PopID();
}

// ---------------------------------------------------------------------------
// Console
// ---------------------------------------------------------------------------

void ExecuteCommand(AstraApp& app, const std::string& command) {
    const std::string trimmed = str::Trim(command);
    if (trimmed.empty()) return;
    app.editor.consoleHistory.push_back("> " + trimmed);
    if (app.editor.consoleHistory.size() > 40) app.editor.consoleHistory.erase(
        app.editor.consoleHistory.begin());

    const std::string lowered = str::ToLower(trimmed);
    if (lowered == "help") {
        log::Info("console", "commands: help, clear, entities, save, weather, diag");
    } else if (lowered == "clear") {
        log::Sink::Instance().Clear();
    } else if (lowered == "entities") {
        log::Info("console", str::Format("%zu entities, %zu active", app.editor.scene.Count(),
                                         app.editor.scene.ActiveCount()));
    } else if (lowered == "save") {
        app.SaveScene();
    } else if (lowered == "weather") {
        log::Info("console", "weather fields are simulated on the CPU; rendering lands in 0.2");
    } else if (lowered == "diag") {
        log::Info("console", str::Format("renderer=offline entities=%zu project=%s",
                                         app.editor.scene.Count(),
                                         app.editor.project.isValid()
                                             ? app.editor.project.name.c_str()
                                             : "none"));
    } else {
        log::Warn("console", "unknown command: " + trimmed + " (try 'help')");
    }
}

}  // namespace

PanelScope BeginPanel(const char* id, const char* title, ui::Icon icon, ImVec2 pos, ImVec2 size,
                      const char* badge, ImGuiWindowFlags flags) {
    ImGuiWindow* window = ImGui::GetCurrentWindow();
    ImDrawList* dl = window->DrawList;

    dl->AddRectFilled(pos, ImVec2(pos.x + size.x, pos.y + size.y), Colors().editorPanel, U(10.0f));
    dl->AddRect(pos, ImVec2(pos.x + size.x, pos.y + size.y), Colors().stroke, U(10.0f), 0, U(1.0f));

    const ImVec2 headerMin(pos.x, pos.y);
    const ImVec2 headerMax(pos.x + size.x, pos.y + U(kHeaderHeight));
    dl->AddRectFilled(headerMin, headerMax, Colors().editorHeader, U(10.0f),
                      ImDrawFlags_RoundCornersTop);
    dl->AddLine(ImVec2(headerMin.x, headerMax.y), ImVec2(headerMax.x, headerMax.y), Colors().stroke,
                U(1.0f));

    ui::DrawIcon(dl, icon, ImVec2(pos.x + U(15.0f), headerMax.y - U(15.0f)), U(13.0f),
                 Colors().textDim, U(1.5f));
    dl->AddText(Text().small, Text().small->FontSize,
                ImVec2(pos.x + U(28.0f), headerMax.y - U(21.0f)), Colors().textDim, title);
    if (badge && *badge) {
        const float badgeWidth = Text().tiny->CalcTextSizeA(Text().tiny->FontSize, FLT_MAX, 0.0f,
                                                            badge).x;
        dl->AddText(Text().tiny, Text().tiny->FontSize,
                    ImVec2(headerMax.x - U(12.0f) - badgeWidth, headerMax.y - U(19.0f)),
                    Colors().textFaint, badge);
    }

    PanelScope scope;
    scope.contentPos = ImVec2(pos.x + U(1.0f), headerMax.y + U(1.0f));
    scope.contentSize = ImVec2(size.x - U(2.0f), size.y - U(kHeaderHeight) - U(2.0f));

    ImGui::SetCursorScreenPos(scope.contentPos);
    ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0, 0, 0, 0));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(U(10.0f), U(8.0f)));
    ImGui::BeginChild(id, scope.contentSize, ImGuiChildFlags_None, flags);
    return scope;
}

void EndPanel() {
    ImGui::EndChild();
    ImGui::PopStyleVar();
    ImGui::PopStyleColor();
}

void Hierarchy(AstraApp& app, ImVec2 pos, ImVec2 size) {
    scene::Scene& scene = app.editor.scene;
    char badge[32];
    std::snprintf(badge, sizeof(badge), "%zu", scene.Count());
    BeginPanel("##hierarchy_panel", "World Outliner", ui::Icon::Layers, pos, size, badge);

    const char* placeholder = "Search actors";
    app.editor.hierarchySearch.reserve(64);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
    ImGui::BeginChild("##hierarchy_list", ImVec2(0, ImGui::GetContentRegionAvail().y - U(38.0f)),
                      ImGuiChildFlags_None, ImGuiWindowFlags_None);
    ImGui::PopStyleVar();

    // Search sits inside its own compact row.
    ImGui::SetCursorScreenPos(ImGui::GetCursorScreenPos());
    ui::SearchField("##hierarchy_search", app.editor.hierarchySearch,
                    ImGui::GetContentRegionAvail().x, placeholder);
    ui::VSpace(6.0f);

    const float rowWidth = ImGui::GetContentRegionAvail().x;
    for (EntityId root : scene.Roots()) {
        DrawEntityRow(app, scene, root, 0, rowWidth);
    }
    if (scene.Roots().empty()) {
        ui::TextFaint("This level has no actors yet.");
    }

    ImGui::EndChild();

    if (ui::GhostButton("Add actor", ImVec2(ImGui::GetContentRegionAvail().x, U(30.0f)), true,
                        ui::Icon::Plus)) {
        const EntityId created = scene.CreateEntity("New Actor");
        app.editor.selected = created;
    }

    EndPanel();
}

void Inspector(AstraApp& app, ImVec2 pos, ImVec2 size) {
    scene::Scene& scene = app.editor.scene;
    BeginPanel("##details_panel", "Details", ui::Icon::Sliders, pos, size);

    scene::Entity* entity = scene.Find(app.editor.selected);
    if (!entity) {
        ui::VSpace(12.0f);
        ui::EmptyState(ui::Icon::Cube, "Nothing selected",
                       "Select an actor in the World Outliner to edit its components.");
        EndPanel();
        return;
    }

    // Header: name + id + active.
    ImDrawList* dl = ImGui::GetWindowDrawList();
    const ImVec2 headerPos = ImGui::GetCursorScreenPos();
    dl->AddRectFilled(headerPos, ImVec2(headerPos.x + ImGui::GetContentRegionAvail().x,
                                        headerPos.y + U(44.0f)),
                      Colors().surfaceSunken, U(9.0f));
    ui::DrawIcon(dl, ui::Icon::Cube, ImVec2(headerPos.x + U(18.0f), headerPos.y + U(22.0f)), U(16.0f),
                 Colors().brand, U(1.6f));
    dl->AddText(Text().medium, Text().medium->FontSize, ImVec2(headerPos.x + U(36.0f), headerPos.y + U(7.0f)),
                Colors().text, str::Ellipsis(entity->name, 22).c_str());
    dl->AddText(Text().tiny, Text().tiny->FontSize, ImVec2(headerPos.x + U(36.0f), headerPos.y + U(26.0f)),
                Colors().textFaint, str::Format("actor %u", entity->id).c_str());
    ImGui::SetCursorScreenPos(ImVec2(headerPos.x + ImGui::GetContentRegionAvail().x - U(42.0f),
                                     headerPos.y + U(12.0f)));
    bool active = entity->active;
    if (ui::Toggle("##active", &active)) scene.SetActive(entity->id, active);
    ImGui::SetCursorScreenPos(ImVec2(headerPos.x, headerPos.y + U(52.0f)));

    const float rowWidth = ImGui::GetContentRegionAvail().x;
    auto groups = scene::BuildInspectorModel(*entity);
    for (auto& group : groups) {
        ImGui::PushID(group.title.c_str());
        const bool removable = group.removable;

        ImGui::PushStyleColor(ImGuiCol_Header, ui::ToVec4(Colors().surfaceSunken));
        ImGui::PushStyleColor(ImGuiCol_HeaderHovered, ui::ToVec4(Colors().surfaceHover));
        ImGui::PushStyleColor(ImGuiCol_HeaderActive, ui::ToVec4(Colors().surfaceHover));
        // Unreal keeps the Actor section folded; components open straight away.
        ImGuiTreeNodeFlags headerFlags = 0;
        if (group.title != "Actor") headerFlags |= ImGuiTreeNodeFlags_DefaultOpen;
        const bool open = ImGui::CollapsingHeader(group.title.c_str(), headerFlags);
        ImGui::PopStyleColor(3);
        if (open) {
            ImGui::Indent(U(4.0f));
            for (auto& property : group.properties) {
                DrawPropertyRow(property, rowWidth - U(12.0f));
            }
            if (removable) {
                ui::VSpace(2.0f);
                ImGui::SetCursorPosX(ImGui::GetCursorPosX() + U(2.0f));
                if (ui::LinkButton("Remove component", ui::Icon::Trash, Colors().textFaint)) {
                    scene.RemoveComponent(entity->id, group.component);
                    ImGui::Unindent(U(4.0f));
                    ImGui::PopID();
                    break;
                }
            }
            ImGui::Unindent(U(4.0f));
            ui::VSpace(6.0f);
        }
        ImGui::PopID();
    }

    // ---- presets ----------------------------------------------------------
    // Applying a preset rewrites the whole component, so it is a single undoable command.
    const float presetWidth = ImGui::GetContentRegionAvail().x;
    if (entity->Has(scene::Component::Material)) {
        ui::Divider(0.5f, U(4.0f), U(4.0f));
        ui::Overline("MATERIAL PRESET");
        int count = 0;
        const char* const* names = scene::MaterialShaderNames(count);
        const char* current = scene::MaterialShaderName(entity->material.shader);
        ImGui::SetNextItemWidth(presetWidth - U(6.0f));
        if (ImGui::BeginCombo("##material_preset", current)) {
            for (int i = 0; i < count; ++i) {
                if (ImGui::Selectable(names[i], entity->material.shader ==
                                                    static_cast<scene::MaterialShader>(i))) {
                    app.PushUndo(std::string("material ") + names[i]);
                    entity->material.shader = static_cast<scene::MaterialShader>(i);
                    if (entity->material.shader == scene::MaterialShader::Hologram) {
                        entity->material.emissive = 0.9f;
                        entity->material.opacity = 0.72f;
                    } else if (entity->material.shader == scene::MaterialShader::Emissive) {
                        entity->material.emissive = 1.0f;
                    }
                    scene.MarkDirty();
                }
            }
            ImGui::EndCombo();
        }
        ui::VSpace(U(4.0f));
    }
    if (entity->Has(scene::Component::Particles)) {
        ui::Overline("PARTICLE PRESET");
        int count = 0;
        const char* const* names = scene::ParticlePresetNames(count);
        ImGui::SetNextItemWidth(presetWidth - U(6.0f));
        if (ImGui::BeginCombo("##particle_preset", scene::ParticlePresetName(entity->particles.preset))) {
            for (int i = 0; i < count; ++i) {
                if (ImGui::Selectable(names[i], static_cast<int>(entity->particles.preset) == i)) {
                    app.PushUndo(std::string("particles ") + names[i]);
                    entity->particles.preset = static_cast<scene::ParticlePreset>(i);
                    scene::ApplyParticlePreset(entity->particles);
                    scene.MarkDirty();
                }
            }
            ImGui::EndCombo();
        }
        ui::VSpace(U(4.0f));
    }
    if (entity->Has(scene::Component::Mesh) && !scene.Meshes().empty()) {
        ui::Overline("MESH LIBRARY");
        const auto& meshes = scene.Meshes();
        const std::string current = entity->mesh.shape == scene::MeshShape::Custom &&
                                            entity->mesh.meshId >= 1 &&
                                            entity->mesh.meshId <= meshes.size()
                                        ? meshes[entity->mesh.meshId - 1].name
                                        : "(primitive)";
        ImGui::SetNextItemWidth(presetWidth - U(6.0f));
        if (ImGui::BeginCombo("##mesh_library", current.c_str())) {
            const bool primitiveSelected = entity->mesh.shape != scene::MeshShape::Custom;
            if (ImGui::Selectable("(primitive)", primitiveSelected)) {
                app.PushUndo("mesh primitive");
                entity->mesh.shape = scene::MeshShape::Cube;
                entity->mesh.meshId = 0;
                scene.MarkDirty();
            }
            for (std::size_t i = 0; i < meshes.size(); ++i) {
                const bool selected = entity->mesh.shape == scene::MeshShape::Custom &&
                                      entity->mesh.meshId == i + 1;
                if (ImGui::Selectable(meshes[i].name.c_str(), selected)) {
                    app.PushUndo("mesh " + meshes[i].name);
                    entity->mesh.shape = scene::MeshShape::Custom;
                    entity->mesh.meshId = static_cast<u32>(i + 1);
                    scene.MarkDirty();
                }
            }
            ImGui::EndCombo();
        }
        ui::VSpace(U(4.0f));
    }

    ui::VSpace(6.0f);
    ImGui::PushStyleColor(ImGuiCol_Header, ui::ToVec4(Colors().surfaceSunken));
    if (ImGui::Button("+ Add component", ImVec2(ImGui::GetContentRegionAvail().x, U(30.0f)))) {
        ImGui::OpenPopup("##add_component");
    }
    ImGui::PopStyleColor();
    if (ImGui::BeginPopup("##add_component")) {
        if (ImGui::MenuItem("Static mesh", nullptr, false,
                            !entity->Has(scene::Component::Mesh))) {
            app.PushUndo("add mesh component");
            scene.AddComponent(entity->id, scene::Component::Mesh);
        }
        if (ImGui::MenuItem("Light", nullptr, false, !entity->Has(scene::Component::Light))) {
            scene.AddComponent(entity->id, scene::Component::Light);
        }
        if (ImGui::MenuItem("Weather volume", nullptr, false,
                            !entity->Has(scene::Component::Weather))) {
            scene.AddComponent(entity->id, scene::Component::Weather);
        }
        if (ImGui::MenuItem("Camera", nullptr, false, !entity->Has(scene::Component::Camera))) {
            scene.AddComponent(entity->id, scene::Component::Camera);
        }
        if (ImGui::MenuItem("Atmosphere", nullptr, false,
                            !entity->Has(scene::Component::Atmosphere))) {
            scene.AddComponent(entity->id, scene::Component::Atmosphere);
        }
        ImGui::EndPopup();
    }

    EndPanel();
}

namespace {

// The viewport owns click-vs-drag detection locally: a click selects, a drag orbits.
ImVec2 gViewportPressPos(0.0f, 0.0f);
bool gViewportPressed = false;

const char* kEmptyLevelHint = "Empty level - add actors from the Content Browser";

}  // namespace

void Viewport(AstraApp& app, ImVec2 pos, ImVec2 size) {
    ImGuiWindow* window = ImGui::GetCurrentWindow();
    ImDrawList* dl = window->DrawList;

    dl->AddRectFilled(pos, ImVec2(pos.x + size.x, pos.y + size.y), Colors().editorPanel, U(10.0f));
    dl->AddRect(pos, ImVec2(pos.x + size.x, pos.y + size.y), Colors().stroke, U(10.0f), 0, U(1.0f));

    // Header: the viewport reads like a render target, so the chrome stays minimal.
    const ImVec2 headerMax(pos.x + size.x, pos.y + U(kHeaderHeight));
    dl->AddRectFilled(pos, headerMax, Colors().editorHeader, U(10.0f), ImDrawFlags_RoundCornersTop);
    dl->AddLine(ImVec2(pos.x, headerMax.y), ImVec2(headerMax.x, headerMax.y), Colors().stroke,
                U(1.0f));
    ui::DrawIcon(dl, ui::Icon::Viewport, ImVec2(pos.x + U(15.0f), headerMax.y - U(15.0f)), U(13.0f),
                 Colors().textDim, U(1.5f));
    dl->AddText(Text().small, Text().small->FontSize, ImVec2(pos.x + U(28.0f), headerMax.y - U(21.0f)),
                Colors().textDim, "Level Viewport");

    const render::Stats& previous = app.editor.viewportStats;
    const std::string readout =
        str::Format("CPU raster  ·  %d actors  ·  %d tris", previous.actors, previous.triangles);
    const float readoutWidth = Text().tiny->CalcTextSizeA(Text().tiny->FontSize, FLT_MAX, 0.0f,
                                                          readout.c_str()).x;
    dl->AddText(Text().tiny, Text().tiny->FontSize,
                ImVec2(headerMax.x - U(14.0f) - readoutWidth, headerMax.y - U(19.0f)),
                Colors().textFaint, readout.c_str());

    const ImVec2 bodyMin(pos.x + U(1.0f), headerMax.y + U(1.0f));
    const ImVec2 bodyMax(pos.x + size.x - U(1.0f), pos.y + size.y - U(1.0f));
    const render::Rect bodyRect{bodyMin, bodyMax};
    app.editor.viewportMinX = bodyMin.x;
    app.editor.viewportMinY = bodyMin.y;
    app.editor.viewportMaxX = bodyMax.x;
    app.editor.viewportMaxY = bodyMax.y;

    // Play-in-editor: the viewport looks through the possessed pawn, and the editor camera is
    // parked so WASD never fights the mouse-look controls.
    const bool possessed = app.editor.playing && app.editor.playPossessCamera;
    const render::Camera viewCamera =
        possessed ? app.editor.player.ViewCamera() : app.editor.viewportCamera;

    // Play mode hides editor chrome (gizmos, actor icons) exactly like a standalone build.
    render::Options drawOptions = app.editor.viewport;
    if (app.editor.playing) {
        drawOptions.showGizmos = false;
        drawOptions.showActorIcons = false;
    }

    dl->PushClipRect(bodyMin, bodyMax, true);
    app.editor.viewportStats =
        render::Draw(dl, bodyRect, app.editor.scene, viewCamera, drawOptions,
                     app.editor.playing ? scene::kInvalidEntity : app.editor.selected, app.time);
    dl->PopClipRect();

    // Input surface. Everything below draws on top of it, so the overlays win the hover test.
    ImGui::SetCursorScreenPos(bodyMin);
    ImGui::InvisibleButton("##viewport_input", ImVec2(bodyRect.Width(), bodyRect.Height()),
                           ImGuiButtonFlags_MouseButtonLeft | ImGuiButtonFlags_MouseButtonRight |
                               ImGuiButtonFlags_MouseButtonMiddle);
    const bool hovered = ImGui::IsItemHovered();
    const bool active = ImGui::IsItemActive();
    app.editor.viewportFocus = hovered;

    ImGuiIO& io = ImGui::GetIO();
    render::Camera& camera = app.editor.viewportCamera;

    if (possessed) {
        // No editor navigation while playing; the pawn owns the view.
    } else if (hovered && io.MouseWheel != 0.0f) {
        camera.Zoom(io.MouseWheel);
    }
    if (active && !possessed) {
        // Unreal bindings: right drag looks around, middle drag pans, Alt+left does both
        // for trackpads that have no middle button.
        const bool orbit = ImGui::IsMouseDragging(ImGuiMouseButton_Right) ||
                           (io.KeyAlt && ImGui::IsMouseDragging(ImGuiMouseButton_Left));
        if (orbit) {
            camera.Orbit(io.MouseDelta.x, io.MouseDelta.y);
        } else if (ImGui::IsMouseDragging(ImGuiMouseButton_Middle)) {
            camera.Pan(io.MouseDelta.x, io.MouseDelta.y, bodyRect.Height());
        }
    }

    if (hovered && !possessed && ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
        gViewportPressPos = io.MousePos;
        gViewportPressed = true;
    }
    if (gViewportPressed && ImGui::IsMouseReleased(ImGuiMouseButton_Left)) {
        gViewportPressed = false;
        const float dx = io.MousePos.x - gViewportPressPos.x;
        const float dy = io.MousePos.y - gViewportPressPos.y;
        if (dx * dx + dy * dy < 16.0f && bodyRect.Contains(io.MousePos)) {
            const scene::EntityId picked =
                render::Pick(bodyRect, app.editor.scene, camera, io.MousePos);
            app.editor.selected = picked;
            if (picked != scene::kInvalidEntity) {
                if (const scene::Entity* entity = app.editor.scene.Find(picked)) {
                    app.SetFooterStatus("Selected " + entity->name);
                }
            }
        }
    }
    if (hovered && !io.WantTextInput && ImGui::IsKeyPressed(ImGuiKey_F, false)) {
        render::FrameSelection(camera, app.editor.scene, app.editor.selected);
        app.SetFooterStatus("Framed the view");
    }

    // ---- play HUD ----------------------------------------------------------
    if (app.editor.playing) {
        char hud[96];
        std::snprintf(hud, sizeof(hud), "PLAYING  %02d:%02d",
                      static_cast<int>(app.editor.playTime) / 60,
                      static_cast<int>(app.editor.playTime) % 60);
        const char* hint = "Esc to stop";
        const ImVec2 pillCenter((bodyMin.x + bodyMax.x) * 0.5f, bodyMin.y + U(26.0f));
        const float hudWidth =
            Text().small->CalcTextSizeA(Text().small->FontSize, FLT_MAX, 0.0f, hud).x +
            Text().tiny->CalcTextSizeA(Text().tiny->FontSize, FLT_MAX, 0.0f, hint).x + U(54.0f);
        dl->AddRectFilled(ImVec2(pillCenter.x - hudWidth * 0.5f, bodyMin.y + U(10.0f)),
                          ImVec2(pillCenter.x + hudWidth * 0.5f, bodyMin.y + U(42.0f)),
                          ui::Fade(Colors().surfaceSunken, 0.9f), U(16.0f));
        dl->AddRect(ImVec2(pillCenter.x - hudWidth * 0.5f, bodyMin.y + U(10.0f)),
                    ImVec2(pillCenter.x + hudWidth * 0.5f, bodyMin.y + U(42.0f)),
                    ui::Fade(Colors().brand, 0.65f), U(16.0f), 0, U(1.0f));
        dl->AddText(Text().small, Text().small->FontSize,
                    ImVec2(pillCenter.x - hudWidth * 0.5f + U(14.0f), bodyMin.y + U(18.0f)),
                    Colors().brand, hud);
        dl->AddText(Text().tiny, Text().tiny->FontSize,
                    ImVec2(pillCenter.x + hudWidth * 0.5f - U(14.0f) -
                               Text().tiny->CalcTextSizeA(Text().tiny->FontSize, FLT_MAX, 0.0f,
                                                          hint).x,
                           bodyMin.y + U(20.0f)),
                    Colors().textFaint, hint);
    }

    // ---- overlay toolbar ---------------------------------------------------
    const ImVec2 toolbarMin(bodyMin.x + U(10.0f), bodyMin.y + U(10.0f));
    const float pillWidth = U(96.0f) + U(4.0f) + U(30.0f) * 3.0f + U(8.0f) * 3.0f + U(16.0f);
    dl->AddRectFilled(ImVec2(toolbarMin.x - U(8.0f), toolbarMin.y - U(6.0f)),
                      ImVec2(toolbarMin.x + pillWidth, toolbarMin.y + U(34.0f)),
                      ui::Fade(Colors().surfaceSunken, 0.86f), U(9.0f));
    dl->AddRect(ImVec2(toolbarMin.x - U(8.0f), toolbarMin.y - U(6.0f)),
                ImVec2(toolbarMin.x + pillWidth, toolbarMin.y + U(34.0f)), Colors().stroke, U(9.0f),
                0, U(1.0f));

    ImGui::SetCursorScreenPos(toolbarMin);
    const std::string modeLabel = render::ViewModeName(app.editor.viewport.mode);
    if (ui::SecondaryButton(modeLabel.c_str(), ImVec2(U(96.0f), U(28.0f)), true, ui::Icon::Eye)) {
        ImGui::OpenPopup("##view_mode_popup");
    }
    if (ImGui::BeginPopup("##view_mode_popup")) {
        int count = 0;
        const char* const* names = render::ViewModeNames(count);
        for (int i = 0; i < count; ++i) {
            if (ImGui::MenuItem(names[i], nullptr, static_cast<int>(app.editor.viewport.mode) == i)) {
                app.editor.viewport.mode = static_cast<render::ViewMode>(i);
                app.SetFooterStatus(std::string("View mode: ") + names[i]);
            }
        }
        ImGui::EndPopup();
    }
    ImGui::SameLine(0.0f, U(8.0f));
    if (ui::IconButton("##vp_grid", ui::Icon::Grid, "World grid", app.editor.viewport.showGrid,
                       30.0f)) {
        app.editor.viewport.showGrid = !app.editor.viewport.showGrid;
    }
    ImGui::SameLine(0.0f, U(8.0f));
    if (ui::IconButton("##vp_shadows", ui::Icon::Sun, "Ground shadows",
                       app.editor.viewport.showShadows, 30.0f)) {
        app.editor.viewport.showShadows = !app.editor.viewport.showShadows;
    }
    ImGui::SameLine(0.0f, U(8.0f));
    if (ui::IconButton("##vp_gizmos", ui::Icon::Axis, "Selection gizmos",
                       app.editor.viewport.showGizmos, 30.0f)) {
        app.editor.viewport.showGizmos = !app.editor.viewport.showGizmos;
    }

    // ---- corner overlays ---------------------------------------------------
    const std::string footer = str::Format("Perspective  ·  %s  ·  %zu actors",
                                           render::ViewModeName(app.editor.viewport.mode),
                                           app.editor.scene.Count());
    dl->AddText(Text().tiny, Text().tiny->FontSize, ImVec2(bodyMin.x + U(12.0f), bodyMax.y - U(20.0f)),
                ui::Fade(Colors().textDim, 0.9f), footer.c_str());

    if (app.editor.scene.Count() == 0) {
        const float hintWidth = Text().small->CalcTextSizeA(Text().small->FontSize, FLT_MAX, 0.0f,
                                                           kEmptyLevelHint).x;
        dl->AddText(Text().small, Text().small->FontSize,
                    ImVec2((bodyMin.x + bodyMax.x - hintWidth) * 0.5f,
                           (bodyMin.y + bodyMax.y) * 0.5f),
                    Colors().textFaint, kEmptyLevelHint);
    }

    // Axis triad, bottom right.
    const ImVec2 gizmo(bodyMax.x - U(46.0f), bodyMax.y - U(46.0f));
    dl->AddCircleFilled(gizmo, U(24.0f), ui::Fade(Colors().surfaceSunken, 0.8f), 32);
    dl->AddCircle(gizmo, U(24.0f), Colors().stroke, 32, U(1.0f));
    dl->AddLine(gizmo, ImVec2(gizmo.x + U(15.0f), gizmo.y), IM_COL32(242, 100, 100, 255), U(1.8f));
    dl->AddLine(gizmo, ImVec2(gizmo.x, gizmo.y - U(15.0f)), IM_COL32(120, 200, 140, 255), U(1.8f));
    dl->AddLine(gizmo, ImVec2(gizmo.x - U(10.0f), gizmo.y + U(10.0f)), IM_COL32(110, 160, 230, 255),
                U(1.8f));

    const std::string resolution = str::Format("%.0f x %.0f", bodyRect.Width(), bodyRect.Height());
    const float resolutionWidth = Text().tiny->CalcTextSizeA(Text().tiny->FontSize, FLT_MAX, 0.0f,
                                                             resolution.c_str()).x;
    dl->AddText(Text().tiny, Text().tiny->FontSize,
                ImVec2(bodyMax.x - U(84.0f) - resolutionWidth, bodyMax.y - U(20.0f)),
                Colors().textFaint, resolution.c_str());
}

void ConsoleBody(AstraApp& app, ImVec2 pos, ImVec2 size) {
    static const char* kFilters[] = {"All", "Info", "Warnings", "Errors"};
    ImGui::SetCursorPosY(ImGui::GetCursorPosY() - U(2.0f));
    ui::Segmented("##console_filter", &app.editor.consoleFilter, kFilters, 4, U(240.0f));
    ImGui::SameLine(0.0f, U(8.0f));
    if (ui::IconButton("##console_clear", ui::Icon::Trash, "Clear log", false, 26.0f)) {
        log::Sink::Instance().Clear();
    }
    ImGui::SameLine(0.0f, U(2.0f));
    static bool autoScroll = true;
    if (ui::IconButton("##console_scroll", ui::Icon::ChevronDown, "Auto-scroll", autoScroll, 26.0f)) {
        autoScroll = !autoScroll;
    }

    // The command row is anchored to the panel's bottom edge so it can never be clipped.
    const float promptTop = pos.y + size.y - U(36.0f);
    const float logHeight = ImMax(U(30.0f), promptTop - ImGui::GetCursorScreenPos().y - U(6.0f));
    ImGui::BeginChild("##console_lines", ImVec2(0.0f, logHeight), ImGuiChildFlags_None,
                      ImGuiWindowFlags_None);

    const auto entries = log::Sink::Instance().Snapshot();
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(0.0f, U(2.0f)));
    for (const auto& entry : entries) {
        if (app.editor.consoleFilter == 1 && entry.level != log::Level::Info) continue;
        if (app.editor.consoleFilter == 2 && entry.level != log::Level::Warn) continue;
        if (app.editor.consoleFilter == 3 && entry.level != log::Level::Error &&
            entry.level != log::Level::Critical) {
            continue;
        }

        ImGui::PushStyleColor(ImGuiCol_Text, ui::ToVec4(Colors().textFaint));
        ui::PushFont(Text().monoSmall);
        ImGui::TextUnformatted(time::FormatUtc(entry.timeUtc, false).c_str());
        ImGui::PopFont();
        ImGui::PopStyleColor();
        ImGui::SameLine(0.0f, U(8.0f));
        ImGui::PushStyleColor(ImGuiCol_Text, ui::ToVec4(SeverityColor(entry.level)));
        ui::PushFont(Text().monoSmall);
        ImGui::TextUnformatted(str::Format("%-5s", log::LevelName(entry.level)).c_str());
        ImGui::PopFont();
        ImGui::PopStyleColor();
        ImGui::SameLine(0.0f, U(8.0f));
        ImGui::PushStyleColor(ImGuiCol_Text, ui::ToVec4(Colors().textFaint));
        ui::PushFont(Text().monoSmall);
        ImGui::TextUnformatted(entry.category.c_str());
        ImGui::PopFont();
        ImGui::PopStyleColor();
        ImGui::SameLine(0.0f, U(8.0f));
        ImGui::PushStyleColor(ImGuiCol_Text, ui::ToVec4(ui::Fade(Colors().text, 0.9f)));
        ui::PushFont(Text().monoSmall);
        ImGui::TextUnformatted(entry.message.c_str());
        ImGui::PopFont();
        ImGui::PopStyleColor();
    }
    for (const auto& line : app.editor.consoleHistory) {
        ImGui::PushStyleColor(ImGuiCol_Text, ui::ToVec4(Colors().brand));
        ui::PushFont(Text().monoSmall);
        ImGui::TextUnformatted(line.c_str());
        ImGui::PopFont();
        ImGui::PopStyleColor();
    }
    ImGui::PopStyleVar();

    if (autoScroll && ImGui::GetScrollY() >= ImGui::GetScrollMaxY() - U(30.0f)) {
        ImGui::SetScrollHereY(1.0f);
    }
    ImGui::EndChild();

    // Command line, pinned to the bottom of the panel.
    ImGui::SetCursorScreenPos(ImVec2(pos.x + U(10.0f), promptTop + U(4.0f)));
    ImGui::PushStyleColor(ImGuiCol_Text, ui::ToVec4(Colors().brand));
    ui::PushFont(Text().monoSmall);
    const ImVec2 promptPos = ImGui::GetCursorScreenPos();
    ImGui::TextUnformatted(">");
    ui::PopFont();
    ImGui::PopStyleColor();
    ImGui::SameLine(0.0f, U(8.0f));
    ImGui::PushItemWidth(ImGui::GetContentRegionAvail().x);
    ImGui::PushStyleColor(ImGuiCol_FrameBg, ui::ToVec4(Colors().surfaceSunken));
    ImGui::PushStyleColor(ImGuiCol_Text, ui::ToVec4(Colors().text));
    ui::PushFont(Text().monoSmall);
    char command[256];
    std::snprintf(command, sizeof(command), "%s", app.editor.consoleInput.c_str());
    if (ImGui::InputTextWithHint("##console_input", "Enter a command (help)", command,
                                 sizeof(command), ImGuiInputTextFlags_EnterReturnsTrue)) {
        app.editor.consoleInput = command;
        ExecuteCommand(app, app.editor.consoleInput);
        app.editor.consoleInput.clear();
    }
    ui::PopFont();
    ImGui::PopStyleColor(2);
    ImGui::PopItemWidth();
    (void)promptPos;
}

void Console(AstraApp& app, ImVec2 pos, ImVec2 size) {
    const PanelScope scope = BeginPanel("##console_panel", "Output Log", ui::Icon::Terminal, pos, size);
    ConsoleBody(app, scope.contentPos, scope.contentSize);
    EndPanel();
}

void Statistics(AstraApp& app, ImVec2 pos, ImVec2 size) {
    BeginPanel("##stats_panel", "Statistics", ui::Icon::Gauge, pos, size);

    const auto& history = app.editor.frameHistory;
    const float average = history.empty()
                              ? 0.0f
                              : std::accumulate(history.begin(), history.end(), 0.0f) /
                                    static_cast<float>(history.size());

    const ImVec2 rowPos = ImGui::GetCursorScreenPos();
    ui::Text(Text().h2, Colors().text, "%.1f ms", average);
    const float fpsSlotWidth = ImGui::GetContentRegionAvail().x;  // measured before the row advances
    const std::string fps = str::Format("%.0f fps", average > 0.0f ? 1000.0f / average : 0.0f);
    const float fpsWidth = Text().tiny->CalcTextSizeA(Text().tiny->FontSize, FLT_MAX, 0.0f,
                                                      fps.c_str()).x;
    ImGui::GetWindowDrawList()->AddText(Text().tiny, Text().tiny->FontSize,
                                        ImVec2(rowPos.x + fpsSlotWidth - fpsWidth - U(2.0f),
                                               rowPos.y + U(11.0f)),
                                        Colors().textFaint, fps.c_str());
    ImGui::SetCursorScreenPos(ImVec2(rowPos.x, rowPos.y + U(32.0f)));

    if (!history.empty()) {
        // Green while a frame fits the 60 Hz budget, amber past it - the same signal the
        // performance overlay in Unreal gives you at a glance.
        const ImU32 sparkColor = average <= 17.0f ? Colors().success : Colors().warn;
        ui::Sparkline(history.data(), static_cast<int>(history.size()),
                      ImVec2(ImGui::GetContentRegionAvail().x, U(46.0f)), sparkColor);
        ui::VSpace(6.0f);
    }

    const render::Stats& viewport = app.editor.viewportStats;
    ui::KeyValue("Actors", str::Format("%zu", app.editor.scene.Count()).c_str());
    ui::KeyValue("Active", str::Format("%zu", app.editor.scene.ActiveCount()).c_str());
    ui::KeyValue("Drawn", str::Format("%d", viewport.drawnActors).c_str());
    ui::KeyValue("Triangles", str::Format("%d", viewport.triangles).c_str());
    ui::KeyValue("Viewport", str::Format("%.1f ms", viewport.milliseconds).c_str(),
                 Colors().success);

    EndPanel();
}

namespace {

struct PlaceableAsset {
    const char* name;
    const char* kind;
    ui::Icon icon;
};

const PlaceableAsset kAssets[] = {
    {"Cube", "Static mesh", ui::Icon::Cube},
    {"Sphere", "Static mesh", ui::Icon::Cube},
    {"Cylinder", "Static mesh", ui::Icon::Cube},
    {"Cone", "Static mesh", ui::Icon::Cube},
    {"Floor", "Static mesh", ui::Icon::Grid},
    {"Directional Light", "Light", ui::Icon::Sun},
    {"Point Light", "Light", ui::Icon::Light},
    {"Camera", "Camera", ui::Icon::Camera},
    {"Player Start", "Gameplay", ui::Icon::Flag},
    {"Weather Volume", "Simulation", ui::Icon::Cloud},
};
constexpr int kAssetCount = static_cast<int>(sizeof(kAssets) / sizeof(kAssets[0]));

scene::Vec3 SnapLocation(const AstraApp& app, scene::Vec3 position) {
    if (!app.editor.snapToGrid) return position;
    const float step = 0.5f;
    position.x = std::round(position.x / step) * step;
    position.y = std::round(position.y / step) * step;
    position.z = std::round(position.z / step) * step;
    return position;
}

scene::EntityId SpawnPlaceable(AstraApp& app, int index) {
    scene::Scene& scene = app.editor.scene;
    if (index < 0 || index >= kAssetCount) return scene::kInvalidEntity;

    const scene::Vec3 at =
        SnapLocation(app, scene::Vec3(app.editor.viewportCamera.target.x, 0.0f,
                                      app.editor.viewportCamera.target.z));
    const std::string name = kAssets[index].name;
    const scene::EntityId id = scene.CreateEntity(name);
    scene::Entity* entity = scene.Find(id);
    if (!entity) return id;

    switch (index) {
        case 0:  // Cube
            entity->Add(scene::Component::Mesh);
            entity->mesh.shape = scene::MeshShape::Cube;
            entity->mesh.size = scene::Vec3(2.0f, 2.0f, 2.0f);
            entity->mesh.baseColor = scene::Vec3(0.86f, 0.36f, 0.34f);
            entity->transform.position = scene::Vec3(at.x, 1.0f, at.z);
            break;
        case 1:  // Sphere
            entity->Add(scene::Component::Mesh);
            entity->mesh.shape = scene::MeshShape::Sphere;
            entity->mesh.size = scene::Vec3(2.0f, 2.0f, 2.0f);
            entity->mesh.baseColor = scene::Vec3(0.55f, 0.66f, 0.92f);
            entity->transform.position = scene::Vec3(at.x, 1.0f, at.z);
            break;
        case 2:  // Cylinder
            entity->Add(scene::Component::Mesh);
            entity->mesh.shape = scene::MeshShape::Cylinder;
            entity->mesh.size = scene::Vec3(1.6f, 3.0f, 1.6f);
            entity->mesh.baseColor = scene::Vec3(0.50f, 0.78f, 0.62f);
            entity->transform.position = scene::Vec3(at.x, 1.5f, at.z);
            break;
        case 3:  // Cone
            entity->Add(scene::Component::Mesh);
            entity->mesh.shape = scene::MeshShape::Cone;
            entity->mesh.size = scene::Vec3(1.6f, 2.6f, 1.6f);
            entity->mesh.baseColor = scene::Vec3(0.90f, 0.74f, 0.38f);
            entity->transform.position = scene::Vec3(at.x, 1.3f, at.z);
            break;
        case 4:  // Floor
            entity->Add(scene::Component::Mesh);
            entity->mesh.shape = scene::MeshShape::Plane;
            entity->mesh.size = scene::Vec3(24.0f, 1.0f, 24.0f);
            entity->mesh.baseColor = scene::Vec3(0.24f, 0.25f, 0.28f);
            entity->transform.position = scene::Vec3(at.x, 0.0f, at.z);
            entity->tag = "world";
            break;
        case 5:  // Directional Light
            entity->Add(scene::Component::Light);
            entity->light.type = scene::LightType::Directional;
            entity->light.intensity = 2.6f;
            entity->transform.position = scene::Vec3(at.x, 6.0f, at.z);
            entity->transform.rotation = scene::Vec3(-48.0f, -34.0f, 0.0f);
            break;
        case 6:  // Point Light
            entity->Add(scene::Component::Light);
            entity->light.type = scene::LightType::Point;
            entity->light.color = scene::Vec3(1.0f, 0.72f, 0.46f);
            entity->light.intensity = 6.0f;
            entity->transform.position = scene::Vec3(at.x, 2.4f, at.z);
            break;
        case 7:  // Camera
            entity->Add(scene::Component::Camera);
            entity->transform.position = scene::Vec3(at.x, 2.0f, at.z + 6.0f);
            entity->transform.rotation = scene::Vec3(-10.0f, 180.0f, 0.0f);
            break;
        case 8:  // Player Start
            entity->transform.position = scene::Vec3(at.x, 0.9f, at.z);
            entity->tag = "gameplay";
            break;
        case 9:  // Weather Volume
            entity->Add(scene::Component::Weather);
            entity->transform.position = scene::Vec3(at.x, 0.0f, at.z);
            entity->transform.scale = scene::Vec3(8.0f, 3.0f, 8.0f);
            entity->tag = "simulation";
            break;
        default:
            break;
    }
    return id;
}

}  // namespace

void ContentBrowserBody(AstraApp& app) {
    ImGui::PushStyleColor(ImGuiCol_Text, ui::ToVec4(Colors().textFaint));
    ui::PushFont(Text().tiny);
    ImGui::TextUnformatted("Content  /  Level");
    ui::PopFont();
    ImGui::PopStyleColor();
    ui::VSpace(6.0f);

    const float tileWidth = U(104.0f);
    const float tileHeight = U(88.0f);
    const float gap = U(8.0f);
    const float available = ImGui::GetContentRegionAvail().x;
    const int columns =
        ImMax(1, static_cast<int>((available + gap) / (tileWidth + gap)));
    ImDrawList* dl = ImGui::GetWindowDrawList();

    for (int i = 0; i < kAssetCount; ++i) {
        if (i > 0) {
            if (i % columns != 0) {
                ImGui::SameLine(0.0f, gap);
            } else {
                ui::VSpace(gap);
            }
        }
        ImGui::PushID(i);
        const ImVec2 tilePos = ImGui::GetCursorScreenPos();
        ImGui::InvisibleButton("##asset", ImVec2(tileWidth, tileHeight));
        const bool hovered = ImGui::IsItemHovered();
        if (ImGui::IsItemClicked()) {
            const scene::EntityId created = SpawnPlaceable(app, i);
            if (created != scene::kInvalidEntity) {
                app.editor.selected = created;
                app.SetFooterStatus(std::string("Added ") + kAssets[i].name + " to the level");
            }
        }

        const ImVec2 tileMax(tilePos.x + tileWidth, tilePos.y + tileHeight);
        dl->AddRectFilled(tilePos, tileMax,
                          hovered ? Colors().surfaceHover : Colors().surfaceSunken, U(9.0f));
        dl->AddRect(tilePos, tileMax, hovered ? ui::Fade(Colors().brand, 0.55f) : Colors().stroke,
                    U(9.0f), 0, U(1.0f));

        const ImVec2 iconCenter(tilePos.x + tileWidth * 0.5f, tilePos.y + U(30.0f));
        dl->AddCircleFilled(iconCenter, U(17.0f), ui::Fade(Colors().brand, 0.12f), 24);
        ui::DrawIcon(dl, kAssets[i].icon, iconCenter, U(15.0f),
                     hovered ? Colors().brand : Colors().textDim, U(1.5f));

        const ImVec2 nameSize = Text().small->CalcTextSizeA(Text().small->FontSize, FLT_MAX, 0.0f,
                                                            kAssets[i].name);
        dl->AddText(Text().small, Text().small->FontSize,
                    ImVec2(tilePos.x + (tileWidth - nameSize.x) * 0.5f, tilePos.y + U(52.0f)),
                    Colors().text, kAssets[i].name);
        const ImVec2 kindSize = Text().tiny->CalcTextSizeA(Text().tiny->FontSize, FLT_MAX, 0.0f,
                                                           kAssets[i].kind);
        dl->AddText(Text().tiny, Text().tiny->FontSize,
                    ImVec2(tilePos.x + (tileWidth - kindSize.x) * 0.5f, tilePos.y + U(70.0f)),
                    Colors().textFaint, kAssets[i].kind);
        ImGui::PopID();
    }
}

void BottomPanel(AstraApp& app, ImVec2 pos, ImVec2 size) {
    ImGuiWindow* window = ImGui::GetCurrentWindow();
    ImDrawList* dl = window->DrawList;

    const ImVec2 max(pos.x + size.x, pos.y + size.y);
    dl->AddRectFilled(pos, max, Colors().editorPanel, U(10.0f));
    dl->AddRect(pos, max, Colors().stroke, U(10.0f), 0, U(1.0f));

    const ImVec2 headerMax(max.x, pos.y + U(kHeaderHeight));
    dl->AddRectFilled(pos, headerMax, Colors().editorHeader, U(10.0f), ImDrawFlags_RoundCornersTop);
    dl->AddLine(ImVec2(pos.x, headerMax.y), ImVec2(headerMax.x, headerMax.y), Colors().stroke,
                U(1.0f));

    struct TabDef {
        const char* label;
        ui::Icon icon;
    };
    static const TabDef kTabs[] = {{"Content Browser", ui::Icon::Folder},
                                   {"Output Log", ui::Icon::Terminal}};

    float x = pos.x + U(8.0f);
    for (int i = 0; i < 2; ++i) {
        ImGui::PushID(i);
        const bool active = app.editor.bottomTab == i;
        const float labelWidth = Text().small->CalcTextSizeA(Text().small->FontSize, FLT_MAX, 0.0f,
                                                             kTabs[i].label)
                                     .x;
        const float tabWidth = labelWidth + U(42.0f);
        ImGui::SetCursorScreenPos(ImVec2(x, pos.y + U(4.0f)));
        ImGui::InvisibleButton("##tab", ImVec2(tabWidth, U(kHeaderHeight) - U(6.0f)));
        const bool hovered = ImGui::IsItemHovered();
        if (ImGui::IsItemClicked()) app.editor.bottomTab = i;

        ui::DrawIcon(dl, kTabs[i].icon, ImVec2(x + U(13.0f), pos.y + U(kHeaderHeight) * 0.5f),
                     U(12.0f), active ? Colors().brand : Colors().textFaint, U(1.4f));
        dl->AddText(Text().small, Text().small->FontSize,
                    ImVec2(x + U(26.0f), pos.y + U(kHeaderHeight) * 0.5f - U(7.0f)),
                    active ? Colors().text : (hovered ? Colors().textDim : Colors().textFaint),
                    kTabs[i].label);
        if (active) {
            dl->AddRectFilled(ImVec2(x, headerMax.y - U(2.0f)),
                              ImVec2(x + tabWidth - U(8.0f), headerMax.y), Colors().brand,
                              U(1.0f));
        }
        ImGui::PopID();
        x += tabWidth;
    }

    const std::string hint = app.editor.bottomTab == 0
                                 ? "Click an asset to place it at the camera target"
                                 : "Commands: help, clear, entities, save, weather, diag";
    const float hintWidth = Text().tiny->CalcTextSizeA(Text().tiny->FontSize, FLT_MAX, 0.0f,
                                                       hint.c_str()).x;
    dl->AddText(Text().tiny, Text().tiny->FontSize,
                ImVec2(max.x - U(14.0f) - hintWidth, pos.y + U(11.0f)), Colors().textFaint,
                hint.c_str());

    const ImVec2 contentPos(pos.x + U(1.0f), headerMax.y + U(1.0f));
    const ImVec2 contentSize(size.x - U(2.0f), size.y - U(kHeaderHeight) - U(2.0f));
    ImGui::SetCursorScreenPos(contentPos);
    ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0, 0, 0, 0));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(U(10.0f), U(8.0f)));
    ImGui::BeginChild("##bottom_content", contentSize, ImGuiChildFlags_None, ImGuiWindowFlags_None);
    if (app.editor.bottomTab == 0) {
        ContentBrowserBody(app);
    } else {
        ConsoleBody(app, contentPos, contentSize);
    }
    ImGui::EndChild();
    ImGui::PopStyleVar();
    ImGui::PopStyleColor();
}

void ToolbarAction(AstraApp& app, const char* label, ui::Icon icon, const char* shortcut) {
    (void)shortcut;
    app.PushToast(ToastKind::Info, label, "Not wired up in this milestone.");
    (void)icon;
}

}  // namespace astra::app::panels
