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
    BeginPanel("##hierarchy_panel", "Hierarchy", ui::Icon::Layers, pos, size, badge);

    const char* placeholder = "Filter entities";
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
        ui::TextFaint("This scene is empty.");
    }

    ImGui::EndChild();

    if (ui::GhostButton("Add entity", ImVec2(ImGui::GetContentRegionAvail().x, U(30.0f)), true,
                        ui::Icon::Plus)) {
        const EntityId created = scene.CreateEntity("New Entity");
        app.editor.selected = created;
    }

    EndPanel();
}

void Inspector(AstraApp& app, ImVec2 pos, ImVec2 size) {
    scene::Scene& scene = app.editor.scene;
    BeginPanel("##inspector_panel", "Inspector", ui::Icon::Sliders, pos, size);

    scene::Entity* entity = scene.Find(app.editor.selected);
    if (!entity) {
        ui::VSpace(12.0f);
        ui::EmptyState(ui::Icon::Cube, "Nothing selected",
                       "Pick an entity in the hierarchy to edit its components.");
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
                Colors().textFaint, str::Format("entity %u", entity->id).c_str());
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
        const bool open = ImGui::CollapsingHeader(group.title.c_str(),
                                                  ImGuiTreeNodeFlags_DefaultOpen);
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

    ui::VSpace(6.0f);
    ImGui::PushStyleColor(ImGuiCol_Header, ui::ToVec4(Colors().surfaceSunken));
    if (ImGui::Button("+ Add component", ImVec2(ImGui::GetContentRegionAvail().x, U(30.0f)))) {
        ImGui::OpenPopup("##add_component");
    }
    ImGui::PopStyleColor();
    if (ImGui::BeginPopup("##add_component")) {
        if (ImGui::MenuItem("Weather field", nullptr, false,
                            !entity->Has(scene::Component::Weather))) {
            scene.AddComponent(entity->id, scene::Component::Weather);
        }
        if (ImGui::MenuItem("Light", nullptr, false, !entity->Has(scene::Component::Light))) {
            scene.AddComponent(entity->id, scene::Component::Light);
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

void Viewport(AstraApp& app, ImVec2 pos, ImVec2 size) {
    ImGuiWindow* window = ImGui::GetCurrentWindow();
    ImDrawList* dl = window->DrawList;

    dl->AddRectFilled(pos, ImVec2(pos.x + size.x, pos.y + size.y), Colors().viewport, U(10.0f));
    dl->AddRect(pos, ImVec2(pos.x + size.x, pos.y + size.y), Colors().stroke, U(10.0f), 0, U(1.0f));

    // Header.
    const ImVec2 headerMax(pos.x + size.x, pos.y + U(kHeaderHeight));
    dl->AddRectFilled(pos, headerMax, Colors().editorHeader, U(10.0f), ImDrawFlags_RoundCornersTop);
    dl->AddLine(ImVec2(pos.x, headerMax.y), ImVec2(headerMax.x, headerMax.y), Colors().stroke, U(1.0f));
    ui::DrawIcon(dl, ui::Icon::Viewport, ImVec2(pos.x + U(15.0f), headerMax.y - U(15.0f)), U(13.0f),
                 Colors().textDim, U(1.5f));
    dl->AddText(Text().small, Text().small->FontSize, ImVec2(pos.x + U(28.0f), headerMax.y - U(21.0f)),
                Colors().textDim, "Viewport");
    dl->AddText(Text().tiny, Text().tiny->FontSize, ImVec2(headerMax.x - U(150.0f), headerMax.y - U(19.0f)),
                Colors().textFaint, "Perspective  ·  Shaded  ·  0 draws");

    // Body.
    const ImVec2 bodyMin(pos.x + U(1.0f), headerMax.y + U(1.0f));
    const ImVec2 bodyMax(pos.x + size.x - U(1.0f), pos.y + size.y - U(1.0f));
    dl->AddRectFilled(bodyMin, bodyMax, Colors().viewport, U(0.0f), ImDrawFlags_RoundCornersBottom);
    ui::Grid(dl, bodyMin, bodyMax, U(38.0f), IM_COL32(255, 255, 255, 6));
    // Horizon line.
    dl->AddLine(ImVec2(bodyMin.x, (bodyMin.y + bodyMax.y) * 0.5f),
                ImVec2(bodyMax.x, (bodyMin.y + bodyMax.y) * 0.5f), IM_COL32(255, 255, 255, 10), U(1.0f));

    const ImVec2 center((bodyMin.x + bodyMax.x) * 0.5f, (bodyMin.y + bodyMax.y) * 0.5f);
    ui::RadialGlow(dl, center, (bodyMax.x - bodyMin.x) * 0.35f, Colors().brand, 0.22f);

    const float cardWidth = ImMin(U(430.0f), (bodyMax.x - bodyMin.x) - U(40.0f));
    const float cardHeight = U(196.0f);
    const ImVec2 cardMin(center.x - cardWidth * 0.5f, center.y - cardHeight * 0.5f);
    const ImVec2 cardMax(cardMin.x + cardWidth, cardMin.y + cardHeight);
    dl->AddRectFilled(cardMin, cardMax, ui::Fade(Colors().surface, 0.92f), U(14.0f));
    dl->AddRect(cardMin, cardMax, ui::Fade(Colors().brand, 0.28f), U(14.0f), 0, U(1.0f));

    ui::BrandMark(dl, ImVec2(center.x, cardMin.y + U(46.0f)), U(24.0f), ui::Fade(Colors().brand, 0.9f),
                  Colors().brandDeep, true, 0.0f);

    const char* title = "Renderer not connected";
    const float titleWidth = Text().h3->CalcTextSizeA(Text().h3->FontSize, FLT_MAX, 0.0f, title).x;
    dl->AddText(Text().h3, Text().h3->FontSize, ImVec2(center.x - titleWidth * 0.5f, cardMin.y + U(76.0f)),
                Colors().text, title);

    ImGui::SetCursorScreenPos(ImVec2(cardMin.x + U(24.0f), cardMin.y + U(110.0f)));
    ImGui::PushTextWrapPos(ImGui::GetCursorScreenPos().x + cardWidth - U(48.0f));
    ImGui::PushStyleColor(ImGuiCol_Text, ui::ToVec4(Colors().textDim));
    ui::PushFont(Text().small);
    ImGui::TextWrapped(
        "This milestone ships the studio shell, scene data and weather tooling. The Vulkan/D3D12 "
        "viewport pipeline arrives in engine 0.2 - scenes stay editable in the meantime.");
    ui::PopFont();
    ImGui::PopStyleColor();
    ImGui::PopTextWrapPos();

    ImGui::SetCursorScreenPos(ImVec2(cardMin.x + U(24.0f), cardMax.y - U(44.0f)));
    if (ui::SecondaryButton("Open roadmap", ImVec2(U(132.0f), U(30.0f)), true, ui::Icon::Flag)) {
        app.PushToast(ToastKind::Info, "Roadmap: engine 0.2", "Vulkan viewport, asset pipeline, replay.");
    }
    ImGui::SameLine(0.0f, U(8.0f));
    if (ui::GhostButton("Rebuild sample scene", ImVec2(U(172.0f), U(30.0f)), true, ui::Icon::Refresh)) {
        app.RefreshDemoContent();
    }

    // Corner overlays.
    dl->AddText(Text().tiny, Text().tiny->FontSize, ImVec2(bodyMin.x + U(12.0f), bodyMin.y + U(10.0f)),
                Colors().textFaint, "GIZMO: MOVE");
    std::string sceneLabel = app.editor.scene.Name() + "  ·  " +
                             std::to_string(app.editor.scene.Count()) + " entities";
    dl->AddText(Text().tiny, Text().tiny->FontSize, ImVec2(bodyMin.x + U(12.0f), bodyMax.y - U(20.0f)),
                Colors().textFaint, sceneLabel.c_str());

    // Axis gizmo, bottom right.
    const ImVec2 gizmo(bodyMax.x - U(52.0f), bodyMax.y - U(50.0f));
    dl->AddCircleFilled(gizmo, U(26.0f), ui::Fade(Colors().surfaceSunken, 0.85f), 32);
    dl->AddCircle(gizmo, U(26.0f), Colors().stroke, 32, U(1.0f));
    dl->AddLine(gizmo, ImVec2(gizmo.x + U(16.0f), gizmo.y), IM_COL32(242, 100, 100, 255), U(1.8f));
    dl->AddLine(gizmo, ImVec2(gizmo.x, gizmo.y - U(16.0f)), IM_COL32(120, 200, 140, 255), U(1.8f));
    dl->AddLine(gizmo, ImVec2(gizmo.x - U(11.0f), gizmo.y + U(11.0f)), IM_COL32(110, 160, 230, 255),
                U(1.8f));

    dl->AddText(Text().tiny, Text().tiny->FontSize, ImVec2(bodyMax.x - U(96.0f), bodyMax.y - U(86.0f)),
                Colors().textFaint, "1920 x 1080");
}

void Console(AstraApp& app, ImVec2 pos, ImVec2 size) {
    BeginPanel("##console_panel", "Output Log", ui::Icon::Terminal, pos, size);

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
    const float promptTop = pos.y + size.y - U(40.0f);
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
    ImGui::SetCursorScreenPos(ImVec2(pos.x + U(10.0f), promptTop + U(2.0f)));
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
        ui::Sparkline(history.data(), static_cast<int>(history.size()),
                      ImVec2(ImGui::GetContentRegionAvail().x, U(46.0f)), Colors().brand);
        ui::VSpace(6.0f);
    }

    ui::KeyValue("Entities", str::Format("%zu", app.editor.scene.Count()).c_str());
    ui::KeyValue("Active", str::Format("%zu", app.editor.scene.ActiveCount()).c_str());
    ui::KeyValue("Renderer", "offline (milestone 1)", Colors().warn);

    EndPanel();
}

void ToolbarAction(AstraApp& app, const char* label, ui::Icon icon, const char* shortcut) {
    (void)shortcut;
    app.PushToast(ToastKind::Info, label, "Not wired up in this milestone.");
    (void)icon;
}

}  // namespace astra::app::panels
