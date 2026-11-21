// app/screens/EditorScreen.cpp - the studio editor shell (no game viewport yet).
#include <cmath>

#include "astra/app/Screens.h"
#include "astra/asset/Bridge.h"
#include "astra/app/screens/Common.h"
#include "astra/app/screens/EditorPanels.h"
#include "astra/core/Strings.h"
#include "astra/core/Time.h"
#include "astra/core/Log.h"
#include "astra/core/Version.h"
#include "astra/render/ViewportRenderer.h"
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
    static const MenuDef kMenus[] = {{"File"}, {"Edit"}, {"Window"}, {"Tools"},
                                     {"Build"}, {"Help"}};

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
                if (ImGui::MenuItem("Undo", "Ctrl+Z", false, app.CanUndo())) app.Undo();
                if (ImGui::MenuItem("Redo", "Ctrl+Shift+Z", false, app.CanRedo())) app.Redo();
                ImGui::Separator();
                if (ImGui::MenuItem("Duplicate", "Ctrl+D",
                                    false, app.editor.selected != scene::kInvalidEntity)) {
                    app.DuplicateSelected();
                }
                if (ImGui::MenuItem("Delete", "Del",
                                    false, app.editor.selected != scene::kInvalidEntity)) {
                    app.DeleteSelected();
                }
                ImGui::Separator();
                if (ImGui::MenuItem("Command palette", "Ctrl+K")) app.showCommandPalette = true;
                if (ImGui::MenuItem("Preferences...")) {
                    app.showSettings = true;
                    app.settingsTab = 0;
                }
            } else if (label == "Tools") {
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
                ImGui::Separator();
                if (ImGui::MenuItem("Import from Blender")) app.ImportFromBlender();
                if (ImGui::MenuItem("Blender bridge folder...")) {
                    app.PushToast(ToastKind::Info, "Blender bridge",
                                  asset::InboxDirectory() + "  -  " + asset::BlenderStatus());
                }
                if (ImGui::MenuItem("Open script workspace", "Ctrl+Enter")) {
                    app.OpenScriptEditor(app.script.directory.empty()
                                             ? app.editor.project.rootPath + "/Scripts"
                                             : app.script.directory);
                }
                if (ImGui::MenuItem("Render frame (path tracer)")) {
                    app.editor.showRenderPanel = true;
                }
                if (ImGui::MenuItem("World settings")) {
                    app.editor.showWorldSettings = !app.editor.showWorldSettings;
                }
                if (ImGui::MenuItem("Import asset (OBJ)...")) {
                    app.PushToast(ToastKind::Info, "Asset import",
                                  "Drop OBJ files into the bridge inbox, or use Tools > Import from Blender.");
                }
                if (ImGui::MenuItem("Refresh content browser")) {
                    app.SetFooterStatus("Content browser refreshed");
                }
            } else if (label == "Build") {
                if (ImGui::MenuItem("Build for Windows (x64)")) {
                    app.PushToast(ToastKind::Info, "Build queued",
                                  "Packaging runs offline: tools/package_windows.ps1.");
                }
                if (ImGui::MenuItem("Build settings...")) {
                    app.PushToast(ToastKind::Info, "Build settings",
                                  "Configuration cook is on the roadmap for 0.3.");
                }
                if (ImGui::MenuItem("Package to zip")) {
                    app.PushToast(ToastKind::Info, "Packaging",
                              "Use tools/package_windows.ps1 in the repository for now.");
                }
            } else if (label == "Window") {
                ImGui::MenuItem("World Outliner", nullptr, &app.editor.showHierarchy);
                ImGui::MenuItem("Details", nullptr, &app.editor.showInspector);
                ImGui::MenuItem("Content Browser / Output Log", nullptr, &app.editor.showConsole);
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
                    app.PushToast(ToastKind::Info, std::string(kEngineVersion) + " - viewport milestone",
                              "CPU level viewport, content browser, actor meshes.");
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

    // Left: back to the hub + project/level identity.
    ImGui::SetCursorScreenPos(ImVec2(min.x + U(10.0f), min.y + U(9.0f)));
    if (ui::IconButton("##back_hub", Icon::ChevronLeft, "Back to project hub", false, 28.0f)) {
        app.CloseProject();
    }

    const std::string projectName = app.editor.project.isValid() ? app.editor.project.name
                                                                 : "Unsaved workspace";
    const float projectX = min.x + U(46.0f);
    dl->AddText(Text().medium, Text().medium->FontSize, ImVec2(projectX, min.y + U(7.0f)),
                Colors().text, str::Ellipsis(projectName, 30).c_str());
    std::string meta = app.editor.scene.Name();
    if (app.editor.scene.IsDirty()) meta += "  *";
    dl->AddText(Text().tiny, Text().tiny->FontSize, ImVec2(projectX, min.y + U(26.0f)),
                app.editor.scene.IsDirty() ? Colors().warn : Colors().textFaint, meta.c_str());

    // Center: transform tools, space and snapping - the Unreal muscle memory row.
    float centerX = min.x + width * 0.5f - U(232.0f);
    ImGui::SetCursorScreenPos(ImVec2(centerX, min.y + U(9.0f)));
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
        centerX += U(32.0f);
    }

    ImGui::SameLine(0.0f, U(8.0f));
    if (ui::SecondaryButton(app.editor.transformSpace == 0 ? "World" : "Local",
                            ImVec2(U(74.0f), U(30.0f)), true, Icon::Grid)) {
        app.editor.transformSpace = app.editor.transformSpace == 0 ? 1 : 0;
        app.SetFooterStatus(app.editor.transformSpace == 0 ? "World space" : "Local space");
    }
    ImGui::SameLine(0.0f, U(4.0f));
    if (ui::IconButton("##snap", Icon::Grid, "Snap to 0.5 m grid", app.editor.snapToGrid, 30.0f)) {
        app.editor.snapToGrid = !app.editor.snapToGrid;
        app.SetFooterStatus(app.editor.snapToGrid ? "Snapping on" : "Snapping off");
    }

    // Right: save, build, transport, then the editor tools. Each block is measured from the
    // right edge so adding buttons can never overlap the ones already placed.
    float rightX = min.x + width - U(10.0f);
    const auto place = [&](float blockWidth, float gap) {
        ImGui::SetCursorScreenPos(ImVec2(rightX - U(blockWidth), min.y + U(8.0f)));
        rightX -= U(blockWidth) + U(gap);
    };

    place(96.0f, 10.0f);
    if (ui::PrimaryButton("Save", ImVec2(U(96.0f), U(30.0f)), true, Icon::Save)) {
        app.SaveScene();
    }

    place(84.0f, 14.0f);
    if (ui::SecondaryButton("Build", ImVec2(U(84.0f), U(30.0f)), true, Icon::Bolt)) {
        app.PushToast(ToastKind::Info, "Packaging",
                      "Cook + package runs from tools/package_windows.ps1.");
    }

    // Play transport: stop, simulate, play.
    place(96.0f, 14.0f);
    if (ui::IconButton("##stop", Icon::Stop, "Stop (Esc)",
                       app.editor.playing || app.editor.simulating, 30.0f)) {
        app.ExitPlay();
        app.editor.simulating = false;
        app.SetFooterStatus("Stopped");
    }
    ImGui::SameLine(0.0f, U(3.0f));
    if (ui::IconButton("##simulate", Icon::Pause, "Simulate (no input)", app.editor.simulating,
                       30.0f)) {
        app.editor.simulating = !app.editor.simulating;
        app.ExitPlay();
        app.SetFooterStatus(app.editor.simulating ? "Simulating" : "Simulation stopped");
    }
    ImGui::SameLine(0.0f, U(3.0f));
    if (ui::IconButton("##play", Icon::Play, "Play in editor as the player (F5)", app.editor.playing,
                       30.0f)) {
        if (app.editor.playing) {
            app.ExitPlay();
        } else {
            app.EnterPlay();
        }
    }

    // Editor tools: history, scripts, offline render, world settings.
    place(176.0f, 12.0f);
    if (ui::IconButton("##undo", Icon::Undo, "Undo (Ctrl+Z)", false, 30.0f, app.CanUndo())) {
        app.Undo();
    }
    ImGui::SameLine(0.0f, U(3.0f));
    if (ui::IconButton("##redo", Icon::Redo, "Redo (Ctrl+Shift+Z)", false, 30.0f,
                       app.CanRedo())) {
        app.Redo();
    }
    ImGui::SameLine(0.0f, U(10.0f));
    if (ui::IconButton("##script", Icon::Code, "AstraScript workspace (Ctrl+Enter)",
                       app.screen == Screen::ScriptEditor, 30.0f)) {
        app.OpenScriptEditor(app.script.directory.empty()
                                 ? (app.editor.project.isValid()
                                        ? app.editor.project.rootPath + "/Scripts"
                                        : std::string())
                                 : app.script.directory);
    }
    ImGui::SameLine(0.0f, U(3.0f));
    if (ui::IconButton("##render", Icon::Camera, "Path tracer render panel",
                       app.editor.showRenderPanel, 30.0f)) {
        app.editor.showRenderPanel = !app.editor.showRenderPanel;
    }
    ImGui::SameLine(0.0f, U(3.0f));
    if (ui::IconButton("##world", Icon::Sliders, "World settings", app.editor.showWorldSettings,
                       30.0f)) {
        app.editor.showWorldSettings = !app.editor.showWorldSettings;
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

    std::string right = "Renderer: CPU raster";
    const float rendererWidth = Text().tiny->CalcTextSizeA(Text().tiny->FontSize, FLT_MAX, 0.0f,
                                                           right.c_str()).x;
    dl->AddText(Text().tiny, Text().tiny->FontSize,
                ImVec2(min.x + width - U(16.0f) - rendererWidth, min.y + U(7.0f)),
                Colors().success, right.c_str());

    right = str::Format("%zu actors  ·  %d tris  ·  %.0f fps  ·  %s", app.editor.scene.Count(),
                        app.editor.viewportStats.triangles,
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
    const float drawerHeight =
        app.editor.showConsole ? ImClamp(bodyHeight * 0.28f, U(150.0f), U(220.0f)) : 0.0f;
    const float viewportHeight = bodyHeight - (app.editor.showConsole ? drawerHeight + U(kGap) : 0.0f);

    panels::Viewport(app, ImVec2(cursorX, bodyTop), ImVec2(centerWidth, viewportHeight));
    if (app.editor.showConsole) {
        panels::BottomPanel(app, ImVec2(cursorX, bodyTop + viewportHeight + U(kGap)),
                            ImVec2(centerWidth, drawerHeight));
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

    panels::RenderPanel(app);
    panels::WorldSettings(app);

    StatusBar(app, ImVec2(min.x, max.y - U(kStatusHeight)), width, U(kStatusHeight));

    // Play-in-editor: the viewport renders through the pawn camera and WASD drives it.
    if (app.editor.playing) {
        app.editor.playTime += app.dt;
        game::PlayerInput input;
        ImGuiIO& playIo = ImGui::GetIO();
        input.forward = ImGui::IsKeyDown(ImGuiKey_W);
        input.back = ImGui::IsKeyDown(ImGuiKey_S);
        input.right = ImGui::IsKeyDown(ImGuiKey_D);
        input.left = ImGui::IsKeyDown(ImGuiKey_A);
        input.jump = ImGui::IsKeyPressed(ImGuiKey_Space, false);
        input.sprint = playIo.KeyShift;
        if (ImGui::IsMouseDown(ImGuiMouseButton_Right)) {
            input.lookDeltaX = playIo.MouseDelta.x;
            input.lookDeltaY = playIo.MouseDelta.y;
        }
        app.editor.player.Update(app.dt, input);
    }

    // Keyboard shortcuts.
    ImGuiIO& io = ImGui::GetIO();
    if (!io.WantTextInput) {
        if (ImGui::IsKeyPressed(ImGuiKey_Q, false)) app.editor.tool = 0;
        if (ImGui::IsKeyPressed(ImGuiKey_W, false)) app.editor.tool = 1;
        if (ImGui::IsKeyPressed(ImGuiKey_E, false)) app.editor.tool = 2;
        if (ImGui::IsKeyPressed(ImGuiKey_R, false)) app.editor.tool = 3;
        if (ImGui::IsKeyPressed(ImGuiKey_F, false)) {
            render::FrameSelection(app.editor.viewportCamera, app.editor.scene, app.editor.selected);
        }
    }
    if (io.KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_S, false)) app.SaveScene();
    if (io.KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_Z, false)) {
        if (io.KeyShift) app.Redo(); else app.Undo();
    }
    if (io.KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_Y, false)) app.Redo();
    if (io.KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_D, false) && !app.editor.playing) {
        app.DuplicateSelected();
    }
    if (io.KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_Enter, false)) {
        app.OpenScriptEditor(app.script.directory.empty() ? app.editor.project.rootPath + "/Scripts"
                                                          : app.script.directory);
    }
    if (ImGui::IsKeyPressed(ImGuiKey_F5, false)) {
        if (app.editor.playing) app.ExitPlay(); else app.EnterPlay();
    }
    if (ImGui::IsKeyPressed(ImGuiKey_Escape, false) && app.editor.playing) app.ExitPlay();
    if (io.KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_K, false)) app.showCommandPalette = true;
    if (!io.WantTextInput && ImGui::IsKeyPressed(ImGuiKey_Delete, false) &&
        app.editor.selected != scene::kInvalidEntity) {
        app.DeleteSelected();
    }

    ImGui::SetCursorScreenPos(min);
}

}  // namespace astra::app::screens
