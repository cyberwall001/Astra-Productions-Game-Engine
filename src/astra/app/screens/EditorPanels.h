// app/screens/EditorPanels.h - dockable-style panels for the studio editor shell.
#pragma once

#include "astra/app/AstraApp.h"
#include "astra/ui/Icons.h"
#include "imgui.h"

namespace astra::app::panels {

struct PanelScope {
    ImVec2 contentPos;
    ImVec2 contentSize;
};

// Draws the panel chrome (surface, border, header) and opens a padded child for content.
// `overlay` gives the panel its own backdrop child, which is what keeps a panel drawn late
// (and any panel the user dragged) painting on top of the viewport instead of behind it.
PanelScope BeginPanel(const char* id, const char* title, ui::Icon icon, ImVec2 pos, ImVec2 size,
                      const char* badge = nullptr, ImGuiWindowFlags flags = 0,
                      bool overlay = false);
void EndPanel();

// Unreal-style panel set: World Outliner, Details, Level Viewport, and a tabbed
// bottom drawer holding the Content Browser and the Output Log.
void Hierarchy(AstraApp& app, ImVec2 pos, ImVec2 size);
void Inspector(AstraApp& app, ImVec2 pos, ImVec2 size);
void Viewport(AstraApp& app, ImVec2 pos, ImVec2 size);
void ContentBrowserBody(AstraApp& app);
void BottomPanel(AstraApp& app, ImVec2 pos, ImVec2 size);
void Console(AstraApp& app, ImVec2 pos, ImVec2 size);
void ConsoleBody(AstraApp& app, ImVec2 pos, ImVec2 size);
void Statistics(AstraApp& app, ImVec2 pos, ImVec2 size);

// Floating windows (movable, resizable): offline path tracer and the world/atmosphere settings.
void RenderPanel(AstraApp& app);
void WorldSettings(AstraApp& app);
void MaterialEditor(AstraApp& app);

// Panels can be dragged by their header; this snaps every panel back to its dock slot.
void ResetPanelLayout();
bool AnyPanelDragging();
ImVec2 PaneOffset(const char* id);

// Panels are drawn in two passes so a dragged (floating) panel paints over its neighbours.
void SetFloatingPass(bool floating);
bool PassAllows(const char* id);
bool AnyPanelFloating();

// Shared by the toolbar and menus.
void ToolbarAction(AstraApp& app, const char* label, ui::Icon icon, const char* shortcut);

}  // namespace astra::app::panels
