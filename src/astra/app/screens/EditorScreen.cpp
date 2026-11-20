// app/screens/EditorScreen.cpp - the studio editor shell (no game viewport yet).
#include <cmath>

#include "astra/app/Screens.h"
#include "astra/app/screens/Common.h"
#include "astra/app/screens/EditorPanels.h"
#include "astra/core/Strings.h"
#include "astra/core/Time.h"
#include "astra/core/Version.h"
#include "astra/ui/Widgets.h"
#include "imgui.h"

namespace astra::app::screens {
namespace {

using ui::Colors;
using ui::Icon;
using ui::Text;
using ui::U;

constexpr float kMenuHeight = 30.0f;
constexpr float kToolbarHeight = 46.0f;
constexpr float kStatusHeight = 26.0f;
constexpr float kGap = 8.0f;

void MenuBar(AstraApp& app, ImVec2 min, float width) {
    ImDrawList* dl = ImGui::GetWindowDrawList();
    const float height = U(kMenuHeight);
    dl->AddRectFilled(min, ImVec2(min.x + width, min.y + height), Colors().editorBar);
    dl->AddLine(ImVec2(min.x, min.y + height), ImVec2(min.x + width, min.y + height),
                Colors().stroke, U(1.0f));

    struct MenuDef {
        const char* label;
    };
    static const MenuDef kMenus[] = {{"File"}, {"Edit"}, {"Scene"}, {"Assets"},
                                     {"Build"}, {"Window"}, {"Help"}};

    float x = min.x + U(12.0f);
    for (const auto& menu : kMenus) {
        ImGui::SetCursorScreenPos(ImVec2(x, min.y + U(3.0f)));
        ImGui::PushID(menu.label);
        const ImVec2 textSize = ImGui::CalcTextSize(menu.label);
        const float itemWidth = textSize.x + U(18.0f);
        ImGui::InvisibleButton("##menu", ImVec2(itemWidth, U(24.0f)));
        const bool hovered = ImGui::IsItemHovered();
        if (ImGui::IsItemClicked()) ImGui::OpenPopup("##menu_popup");
        if (hovered) {
            dl->AddRectFilled(ImVec2(x, min.y + U(3.0f)),
                              ImVec2(x + itemWidth, min.y + U(27.0f)), Colors().surfaceHover, U(6.0f));
        }
        dl->AddText(Text().small, Text().small->FontSize,
                    ImVec2(x + U(9.0f), min.y + U(7.0f)),
                    hovered ? Colors().text : Colors().textDim, menu.label);

        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(U(6.0f), U(6.0f)));
        if (ImGui::BeginPopup("##menu_popup")) {
            std::string label(menu.label);
            if (label == "File") {
                if (ImGui::MenuItem("New project...")) {
                    app.CreateProject("Astra Project " + str::Format("%02d", static_cast<int>(app.time) % 100));
                }
                if (ImGui::MenuItem("Save scene", "Ctrl+S")) app.SaveScene();
                if (ImGui::MenuItem("Save all")) app.SaveScene();
                ImGui::Separator();
                if (ImGui::MenuItem("Close project")) app.CloseProject();
                if (ImGui::MenuItem("Back to project hub")) app.CloseProject();
                ImGui::Separator();
                if (ImGui::MenuItem("Exit Astra Studio", "Alt+F4")) app.wantsQuit = true;
            } else if (label == "Edit") {
                if (ImGui::MenuItem("Undo", "Ctrl+Z", false, false)) {}
                if (ImGui::MenuItem("Redo", "Ctrl+Y", false, false)) {}
                ImGui::Separator();
                if (ImGui::MenuItem("Command palette", "Ctrl+K")) app.showCommandPalette = true;
                if (ImGui::MenuItem("Preferences...")) {
                    app.showSettings = true;
                    app.settingsTab = 0;
                }
            } else if (label == "Scene") {
                if (ImGui::MenuItem("New scene")) app.NewScene();
                if (ImGui::MenuItem("Rebuild sample scene")) app.RefreshDemoContent();
                ImGui::Separator();
                if (ImGui::MenuItem("Add entity")) {
                    app.editor.selected = app.editor.scene.CreateEntity("New Entity");
                }
                if (ImGui::MenuItem("Delete selected entity", "Del",
                                    false, app.editor.selected != scene::kInvalidEntity)) {
                    app.editor.scene.DestroyEntity(app.editor.selected);
                    app.editor.selected = scene::kInvalidEntity;
                }
            } else if (label == "Assets") {
                if (ImGui::MenuItem("Import asset...")) {
                    app.PushToast(ToastKind::Info, "Asset pipeline", "Import lands with engine 0.2.");
                }
                if (ImGui::MenuItem("Refresh content browser")) {
                    app.SetFooterStatus("Content browser refreshed");
                }
            } else if (label == "Build") {
                if (ImGui::MenuItem("Build for Windows (x64)")) {
                    app.PushToast(ToastKind::Warning, "No renderer yet",
                              "Builds need the 0.2 viewport pipeline.");
                }
                if (ImGui::MenuItem("Build settings...")) {
                    app.PushToast(ToastKind::Info, "Build settings", "Not available in this milestone.");
                }
                if (ImGui::MenuItem("Package to zip")) {
                    app.PushToast(ToastKind::Info, "Packaging",
                              "Use tools/package_windows.ps1 in the repository for now.");
                }
            } else if (label == "Window") {
                ImGui::MenuItem("Hierarchy", nullptr, &app.editor.showHierarchy);
                ImGui::MenuItem("Inspector", nullptr, &app.editor.showInspector);
                ImGui::MenuItem("Output log", nullptr, &app.editor.showConsole);
                ImGui::MenuItem("Statistics", nullptr, &app.editor.showStatistics);
                ImGui::Separator();
                if (ImGui::MenuItem("Reset layout")) {
                    app.editor.showHierarchy = true;
                    app.editor.showInspector = true;
                    app.editor.showConsole = true;
                    app.editor.showStatistics = true;
                    app.SetFooterStatus("Layout reset");
                }
            } else if (label == "Help") {
                if (ImGui::MenuItem("Documentation")) {
                    app.PushToast(ToastKind::Info, "docs/", "See ARCHITECTURE.md, UI_GUIDE.md, BUILD_WINDOWS.md.");
                }
                if (ImGui::MenuItem("Release notes")) {
                    app.PushToast(ToastKind::Info, "0.1.0 - studio shell",
                              "Launcher, registry, access codes and editor panels.");
                }
                ImGui::Separator();
                if (ImGui::MenuItem("About Astra Studio")) {
                    app.showSettings = true;
                    app.settingsTab = 4;
                }
            }
            ImGui::EndPopup();
        }
        ImGui::PopStyleVar();
        ImGui::PopID();
        x += itemWidth;
    }

    // Right side: engine version + account.
    const std::string version = std::string("ENGINE ") + kEngineVersion;
    const float versionWidth = Text().tiny->CalcTextSizeA(Text().tiny->FontSize, FLT_MAX, 0.0f,
                                                          version.c_str()).x;
    dl->AddText(Text().tiny, Text().tiny->FontSize,
                ImVec2(min.x + width - U(24.0f) - versionWidth, min.y + U(10.0f)),
                Colors().textFaint, version.c_str());

    if (const auth::Account* account = app.CurrentUser()) {
        const float chipX = min.x + width - U(24.0f) - versionWidth - U(150.0f);
        ImGui::SetCursorScreenPos(ImVec2(chipX, min.y + U(5.0f)));
        ImGui::InvisibleButton("##menu_account", ImVec2(U(136.0f), U(20.0f)));
        ui::DrawIcon(dl, Icon::User, ImVec2(chipX + U(8.0f), min.y + U(15.0f)), U(12.0f),
                     Colors().textFaint, U(1.5f));
        dl->AddText(Text().tiny, Text().tiny->FontSize, ImVec2(chipX + U(20.0f), min.y + U(8.0f)),
                    Colors().textDim, str::Ellipsis(account->displayName, 16).c_str());
    }
}

void Toolbar(AstraApp& app, ImVec2 min, float width) {
    ImDrawList* dl = ImGui::GetWindowDrawList();
    dl->AddRectFilled(min, ImVec2(min.x + width, min.y + U(kToolbarHeight)), Colors().editorHeader);
    dl->AddLine(ImVec2(min.x, min.y + U(kToolbarHeight)),
                ImVec2(min.x + width, min.y + U(kToolbarHeight)), Colors().stroke, U(1.0f));

    // Left: project + branch.
    ImGui::SetCursorScreenPos(ImVec2(min.x + U(10.0f), min.y + U(9.0f)));
    if (ui::IconButton("##back_hub", Icon::ChevronLeft, "Back to project hub", false, 28.0f)) {
        app.CloseProject();
    }

    const std::string projectName = app.editor.project.isValid() ? app.editor.project.name
                                                                 : "Unsaved workspace";
    const float projectX = min.x + U(46.0f);
    dl->AddText(Text().medium, Text().medium->FontSize, ImVec2(projectX, min.y + U(7.0f)),
                Colors().text, str::Ellipsis(projectName, 32).c_str());
    std::string meta = app.editor.scene.Name();
    if (app.editor.scene.IsDirty()) meta += "  *";
    dl->AddText(Text().tiny, Text().tiny->FontSize, ImVec2(projectX, min.y + U(26.0f)),
                app.editor.scene.IsDirty() ? Colors().warn : Colors().textFaint, meta.c_str());

    // Center: tools.
    float centerX = min.x + width * 0.5f - U(150.0f);
    ImGui::SetCursorScreenPos(ImVec2(centerX, min.y + U(9.0f)));
    const float toolSize = U(28.0f);
    struct ToolDef {
        Icon icon;
        const char* tooltip;
    };
    static const ToolDef kTools[] = {{Icon::Compass, "Select (Q)"},
                                      {Icon::Move, "Move (W)"},
                                      {Icon::Refresh, "Rotate (E)"},
                                      {Icon::Axis, "Scale (R)"}};
    for (int i = 0; i < 4; ++i) {
        char id[16];
        std::snprintf(id, sizeof(id), "##tool%d", i);
        if (ui::IconButton(id, kTools[i].icon, kTools[i].tooltip, app.editor.tool == i, 28.0f)) {
            app.editor.tool = i;
        }
        ImGui::SameLine(0.0f, U(4.0f));
        centerX += toolSize + U(4.0f);
    }

    ImGui::SameLine(0.0f, U(10.0f));
    const bool autosave = app.editor.autosave;
    if (ui::IconButton("##autosave", Icon::Save,
                       autosave ? "Autosave on (every 90s)" : "Autosave off", autosave, 28.0f)) {
        app.editor.autosave = !app.editor.autosave;
        app.SetFooterStatus(app.editor.autosave ? "Autosave enabled" : "Autosave disabled");
    }

    // Right: transport + save.
    float rightX = min.x + width - U(10.0f);
    ImGui::SetCursorScreenPos(ImVec2(rightX - U(96.0f), min.y + U(8.0f)));
    if (ui::PrimaryButton("Save", ImVec2(U(96.0f), U(30.0f)), true, Icon::Save)) {
        app.SaveScene();
    }
    rightX -= U(104.0f);

    ImGui::SetCursorScreenPos(ImVec2(rightX - U(84.0f), min.y + U(8.0f)));
    if (ui::SecondaryButton("Build", ImVec2(U(84.0f), U(30.0f)))) {
        app.PushToast(ToastKind::Warning, "Renderer offline",
                  "Builds open up once the viewport pipeline exists.");
    }
    rightX -= U(92.0f);

    ImGui::SetCursorScreenPos(ImVec2(rightX - U(96.0f), min.y + U(8.0f)));
    if (ui::IconButton("##stop", Icon::Stop, "Stop (renderer offline)", false, 30.0f)) {
        app.PushToast(ToastKind::Info, "Play mode", "Nothing to stop - no renderer yet.");
    }
    ImGui::SameLine(0.0f, U(2.0f));
    if (ui::IconButton("##pause", Icon::Pause, "Pause (renderer offline)", false, 30.0f)) {
        app.PushToast(ToastKind::Info, "Play mode", "Nothing to pause - no renderer yet.");
    }
    ImGui::SameLine(0.0f, U(2.0f));
    if (ui::IconButton("##play", Icon::Play, "Play (renderer offline)", false, 30.0f)) {
        app.PushToast(ToastKind::Info, "Play mode", "Scenes run headlessly today; the viewport is 0.2.");
    }
}

void StatusBar(AstraApp& app, ImVec2 min, float width, float height) {
    ImDrawList* dl = ImGui::GetWindowDrawList();
    dl->AddRectFilled(min, ImVec2(min.x + width, min.y + height), Colors().editorBar);
    dl->AddLine(min, ImVec2(min.x + width, min.y), Colors().stroke, U(1.0f));

    float x = min.x + U(12.0f);
    ui::DrawIcon(dl, Icon::Branch, ImVec2(x + U(5.0f), min.y + height * 0.5f), U(11.0f),
                 Colors().textFaint, U(1.4f));
    x += U(16.0f);
    dl->AddText(Text().tiny, Text().tiny->FontSize, ImVec2(x, min.y + U(7.0f)), Colors().textDim,
                app.editor.project.isValid() ? "main" : "untracked");

    if (app.editor.project.isValid()) {
        x += U(42.0f);
        dl->AddText(Text().tiny, Text().tiny->FontSize, ImVec2(x, min.y + U(7.0f)),
                    Colors().textFaint, str::Ellipsis(app.editor.project.rootPath, 58).c_str());
    }

    const std::string status = app.editor.statusTimer > 0.0f ? app.editor.statusText
                                                             : app.footerStatus;
    if (!status.empty()) {
        const float statusWidth = Text().tiny->CalcTextSizeA(Text().tiny->FontSize, FLT_MAX, 0.0f,
                                                             status.c_str()).x;
        dl->AddText(Text().tiny, Text().tiny->FontSize,
                    ImVec2(min.x + width * 0.5f - statusWidth * 0.5f, min.y + U(7.0f)),
                    Colors().success, status.c_str());
    }

    std::string right = "Renderer: offline";
    const float rendererWidth = Text().tiny->CalcTextSizeA(Text().tiny->FontSize, FLT_MAX, 0.0f,
                                                           right.c_str()).x;
    dl->AddText(Text().tiny, Text().tiny->FontSize,
                ImVec2(min.x + width - U(16.0f) - rendererWidth, min.y + U(7.0f)), Colors().warn,
                right.c_str());

    right = str::Format("%zu entities  ·  %.0f fps  ·  %s", app.editor.scene.Count(),
                        app.dt > 0.0f ? 1.0f / app.dt : 0.0f, kEngineVersion);
    const float infoWidth = Text().tiny->CalcTextSizeA(Text().tiny->FontSize, FLT_MAX, 0.0f,
                                                       right.c_str()).x;
    dl->AddText(Text().tiny, Text().tiny->FontSize,
                ImVec2(min.x + width - U(150.0f) - infoWidth, min.y + U(7.0f)),
                Colors().textFaint, right.c_str());
}

}  // namespace

void Editor(AstraApp& app) {
    ImGuiWindow* window = ImGui::GetCurrentWindow();
    ImDrawList* dl = window->DrawList;
    const ImVec2 min = window->Pos;
    const ImVec2 max = ImVec2(min.x + window->Size.x, min.y + window->Size.y);
    const float width = window->Size.x;

    dl->AddRectFilled(min, max, Colors().editorBar);

    MenuBar(app, min, width);
    Toolbar(app, ImVec2(min.x, min.y + U(kMenuHeight)), width);

    const float bodyTop = min.y + U(kMenuHeight) + U(kToolbarHeight) + U(kGap);
    const float bodyBottom = max.y - U(kStatusHeight) - U(kGap);
    const float bodyHeight = ImMax(U(200.0f), bodyBottom - bodyTop);

    const float leftWidth = app.editor.showHierarchy ? ImClamp(width * 0.19f, U(210.0f), U(280.0f)) : 0.0f;
    const float rightWidth = app.editor.showInspector ? ImClamp(width * 0.23f, U(268.0f), U(340.0f)) : 0.0f;

    float cursorX = min.x + U(kGap);
    if (app.editor.showHierarchy) {
        panels::Hierarchy(app, ImVec2(cursorX, bodyTop), ImVec2(leftWidth, bodyHeight));
        cursorX += leftWidth + U(kGap);
    }

    const float rightEdge = max.x - U(kGap) - (app.editor.showInspector ? rightWidth + U(kGap) : 0.0f);
    const float centerWidth = ImMax(U(320.0f), rightEdge - cursorX);
    const float consoleHeight = app.editor.showConsole ? ImClamp(bodyHeight * 0.30f, U(140.0f), U(240.0f)) : 0.0f;
    const float viewportHeight = bodyHeight - (app.editor.showConsole ? consoleHeight + U(kGap) : 0.0f);

    panels::Viewport(app, ImVec2(cursorX, bodyTop), ImVec2(centerWidth, viewportHeight));
    if (app.editor.showConsole) {
        panels::Console(app, ImVec2(cursorX, bodyTop + viewportHeight + U(kGap)),
                        ImVec2(centerWidth, consoleHeight));
    }

    if (app.editor.showInspector) {
        const float statsHeight = app.editor.showStatistics ? ImClamp(bodyHeight * 0.32f, U(160.0f), U(230.0f)) : 0.0f;
        panels::Inspector(app, ImVec2(rightEdge + U(kGap), bodyTop),
                          ImVec2(rightWidth, bodyHeight - (statsHeight > 0.0f ? statsHeight + U(kGap) : 0.0f)));
        if (app.editor.showStatistics) {
            panels::Statistics(app,
                               ImVec2(rightEdge + U(kGap), bodyTop + bodyHeight - statsHeight),
                               ImVec2(rightWidth, statsHeight));
        }
    }

    StatusBar(app, ImVec2(min.x, max.y - U(kStatusHeight)), width, U(kStatusHeight));

    // Keyboard shortcuts.
    ImGuiIO& io = ImGui::GetIO();
    if (io.KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_S, false)) app.SaveScene();
    if (io.KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_K, false)) app.showCommandPalette = true;
    if (!io.WantTextInput && ImGui::IsKeyPressed(ImGuiKey_Delete, false) &&
        app.editor.selected != scene::kInvalidEntity) {
        app.editor.scene.DestroyEntity(app.editor.selected);
        app.editor.selected = scene::kInvalidEntity;
        app.SetFooterStatus("Entity deleted");
    }

    ImGui::SetCursorScreenPos(min);
}

}  // namespace astra::app::screens
