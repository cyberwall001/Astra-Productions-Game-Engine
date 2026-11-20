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
PanelScope BeginPanel(const char* id, const char* title, ui::Icon icon, ImVec2 pos, ImVec2 size,
                      const char* badge = nullptr, ImGuiWindowFlags flags = 0);
void EndPanel();

void Hierarchy(AstraApp& app, ImVec2 pos, ImVec2 size);
void Inspector(AstraApp& app, ImVec2 pos, ImVec2 size);
void Viewport(AstraApp& app, ImVec2 pos, ImVec2 size);
void Console(AstraApp& app, ImVec2 pos, ImVec2 size);
void Statistics(AstraApp& app, ImVec2 pos, ImVec2 size);

// Shared by the toolbar and menus.
void ToolbarAction(AstraApp& app, const char* label, ui::Icon icon, const char* shortcut);

}  // namespace astra::app::panels
