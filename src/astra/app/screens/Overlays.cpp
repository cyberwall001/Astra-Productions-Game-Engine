// app/screens/Overlays.cpp - root window, toasts, settings and the command palette.
#include <algorithm>
#include <cmath>

#include "astra/app/Screens.h"
#include "astra/app/screens/Common.h"
#include "astra/core/FileIO.h"
#include "astra/core/Paths.h"
#include "astra/core/Strings.h"
#include "astra/core/Time.h"
#include "astra/core/Version.h"
#include "astra/licensing/MasterKey.h"
#include "astra/ui/Branding.h"
#include "astra/ui/Widgets.h"
#include "imgui.h"

namespace astra::app {
namespace {

using ui::Colors;
using ui::Icon;
using ui::Text;
using ui::U;

ImU32 AccentFromHex(const std::string& hex, ImU32 fallback) {
    if (hex.size() != 6) return fallback;
    auto parse = [](const std::string& s, int offset) {
        return static_cast<int>(std::strtol(s.substr(static_cast<std::size_t>(offset), 2).c_str(),
                                            nullptr, 16));
    };
    return IM_COL32(parse(hex, 0), parse(hex, 2), parse(hex, 4), 255);
}

Icon ToastIcon(ToastKind kind) {
    switch (kind) {
        case ToastKind::Success: return Icon::Check;
        case ToastKind::Warning: return Icon::Warning;
        case ToastKind::Error: return Icon::Warning;
        default: return Icon::Info;
    }
}

ImU32 ToastColor(ToastKind kind) {
    switch (kind) {
        case ToastKind::Success: return Colors().success;
        case ToastKind::Warning: return Colors().warn;
        case ToastKind::Error: return Colors().danger;
        default: return Colors().info;
    }
}

// ---------------------------------------------------------------------------
// Settings
// ---------------------------------------------------------------------------

void DrawAboutTab(AstraApp& app) {
    ui::VSpace(4.0f);
    ui::LogoImage(ImVec2(U(72.0f), U(72.0f)), 0.5f);
    ui::VSpace(4.0f);
    screens::Wordmark(Colors().text, Colors().textFaint, false, 0.0f);
    ui::VSpace(8.0f);
    ui::TextFaint("Astra Engine %s  ·  %s  ·  %s channel", kEngineVersion, kEngineCodename,
                  kBuildChannel);
    ui::VSpace(14.0f);
    ui::Divider(0.8f, 2.0f, 10.0f);
    ui::KeyValue("Build", "milestone 1 - studio shell");
    ui::KeyValue("Renderer", "offline (engine 0.2)",
                 Colors().warn);
    ui::KeyValue("Access key", license::MasterKeyFingerprint().c_str());
    ui::KeyValue("Data root", paths::DataRoot().c_str());
    ui::VSpace(12.0f);
    ui::TextWrapped(
        "Built on Dear ImGui (MIT), GLFW (zlib), stb_image (MIT) and the Roboto/Cousine typefaces. "
        "Astra Studio, Astra Engine and Astra KeyForge are products of Astra Productions.",
        Colors().textFaint, U(460.0f));
    ui::VSpace(10.0f);
    if (ui::LinkButton("Read the security model", Icon::Shield)) {
        app.PushToast(ToastKind::Info, "docs/SECURITY.md",
                  "Access codes, threat model and the v0.2 licensing service plan.");
    }
}

void DrawSettingsBody(AstraApp& app) {
    AppSettings& settings = app.settings.Get();

    switch (app.settingsTab) {
        case 0: {  // Appearance
            ui::TextDim("INTERFACE SCALE");
            ui::VSpace(6.0f);
            float scale = settings.uiScale;
            ImGui::PushItemWidth(U(300.0f));
            if (ImGui::SliderFloat("##uiscale", &scale, 0.85f, 1.35f, "%.2fx")) {
                settings.uiScale = scale;
            }
            ImGui::PopItemWidth();
            ui::TextFaint("Reload the studio to fully apply a new scale.");
            ui::VSpace(16.0f);

            ui::TextDim("DENSITY");
            ui::VSpace(6.0f);
            static const char* kDensities[] = {"Compact", "Comfortable", "Spacious"};
            int density = static_cast<int>(settings.density);
            if (ui::Segmented("##density", &density, kDensities, 3, U(300.0f))) {
                settings.density = static_cast<UiDensity>(density);
            }
            ui::VSpace(18.0f);

            ui::Toggle("Reduced motion", &settings.reducedMotion,
                       "Shortens transitions and stops the boot pulse.");
            ui::VSpace(10.0f);
            ui::Toggle("Show tooltips", &settings.showTooltips, nullptr);
            ui::VSpace(10.0f);
            ui::Toggle("Verbose logging", &settings.verboseLogging,
                       "Adds debug-level entries to the output log.");
            break;
        }
        case 1: {  // GitHub
            const bool connected = !app.GitHubToken().empty();
            ui::BeginCard("##github_settings", ImVec2(0.0f, U(96.0f)), Colors().surfaceSunken);
            ui::DrawIcon(ImGui::GetWindowDrawList(), Icon::Github,
                         ImVec2(ImGui::GetCursorScreenPos().x + U(12.0f), ImGui::GetCursorScreenPos().y + U(12.0f)),
                         U(18.0f), connected ? Colors().success : Colors().textFaint, U(1.6f));
            ImGui::SetCursorPosX(ImGui::GetCursorPosX() + U(34.0f));
            ui::Text(Text().medium, Colors().text, "%s",
                     connected ? (settings.githubLogin.empty() ? "Token connected" : settings.githubLogin.c_str())
                               : "No GitHub token");
            ui::TextFaint(connected ? "Repository list syncs on open" : "Demo workspace is active");
            ui::EndCard();
            ui::VSpace(14.0f);

            ui::FieldLabel("PERSONAL ACCESS TOKEN");
            ui::TextField("##github_token_settings", app.hub.tokenInput, "ghp_...", Icon::Key);
            ui::VSpace(10.0f);
            if (ui::PrimaryButton("Save token", ImVec2(U(160.0f), U(36.0f)),
                                  app.hub.tokenInput.size() >= 8, Icon::Check)) {
                app.SetGitHubToken(app.hub.tokenInput, "");
                app.hub.tokenInput.clear();
                app.PushToast(ToastKind::Success, "GitHub token saved");
            }
            ImGui::SameLine(0.0f, U(8.0f));
            if (ui::SecondaryButton("Sync now", ImVec2(U(130.0f), U(36.0f)), true, Icon::Refresh)) {
                app.StartRepoSync();
            }
            ImGui::SameLine(0.0f, U(8.0f));
            if (ui::GhostButton("Clear", ImVec2(U(100.0f), U(36.0f)), connected, Icon::Trash)) {
                app.SetGitHubToken("", "");
                app.PushToast(ToastKind::Info, "GitHub token cleared");
            }
            ui::VSpace(16.0f);
            ui::Toggle("Use the demo workspace without a token", &settings.allowDemoWorkspace,
                       "Shows sample repositories so the hub is usable offline.");
            break;
        }
        case 2: {  // Account
            const auth::Account* account = app.CurrentUser();
            if (!account) {
                ui::EmptyState(Icon::User, "Not signed in",
                               "Sign in from the launcher to manage your engine account.");
                break;
            }
            const ImU32 accent = AccentFromHex(account->accentHex, Colors().brand);
            ImGui::SetCursorPosY(ImGui::GetCursorPosY() + U(4.0f));
            ui::Avatar(account->Initials().c_str(), accent, 56.0f, true);
            ImGui::SameLine(0.0f, U(14.0f));
            ImGui::BeginGroup();
            ui::Text(Text().h3, Colors().text, "%s", account->displayName.c_str());
            ui::TextFaint("@%s  ·  %s", account->username.c_str(),
                          license::RoleName(account->role));
            ImGui::EndGroup();
            ui::VSpace(16.0f);
            ui::KeyValue("Role", license::RoleBadge(account->role), accent);
            ui::KeyValue("Email", account->email.empty() ? "not set" : account->email.c_str());
            ui::KeyValue("Registered", time::FormatUtc(account->createdUtc).c_str());
            ui::KeyValue("Last sign-in", time::Relative(account->lastLoginUtc, time::NowUtc()).c_str());
            ui::KeyValue("Sign-ins", str::Format("%d", account->signInCount).c_str());
            ui::KeyValue("Access code", account->accessCodeMasked.c_str());
            ui::VSpace(18.0f);
            if (ui::SecondaryButton("Sign out", ImVec2(U(160.0f), U(38.0f)), true, Icon::Logout)) {
                app.showSettings = false;
                app.SignOut();
            }
            ImGui::SameLine(0.0f, U(8.0f));
            if (ui::GhostButton("Switch account", ImVec2(U(180.0f), U(38.0f)), true, Icon::Users)) {
                app.showSettings = false;
                app.SignOut();
            }
            break;
        }
        case 3: {  // Storage
            ui::TextDim("WHERE ASTRA KEEPS STATE");
            ui::VSpace(8.0f);
            ui::BeginCard("##storage_paths", ImVec2(0.0f, U(150.0f)), Colors().surfaceSunken);
            ui::TextFaint("Data root");
            ui::Text(Text().monoSmall, Colors().textDim, "%s", paths::DataRoot().c_str());
            ui::VSpace(8.0f);
            ui::TextFaint("Asset root");
            ui::Text(Text().monoSmall, Colors().textDim, "%s", paths::AssetRoot().c_str());
            ui::VSpace(8.0f);
            ui::TextFaint("Workspace");
            ui::Text(Text().monoSmall, Colors().textDim, "%s",
                     paths::Join(paths::EngineDataDir(), "workspace").c_str());
            ui::EndCard();
            ui::VSpace(14.0f);
            ui::KeyValue("Registered accounts", str::Format("%zu", app.auth.AccountCount()).c_str());
            ui::KeyValue("Redeemed codes", str::Format("%zu", app.auth.Accounts().Accounts().size()).c_str());
            ui::VSpace(14.0f);
            if (ui::SecondaryButton("Open data folder", ImVec2(U(190.0f), U(36.0f)), true, Icon::Folder)) {
                app.PushToast(ToastKind::Info, "Data folder", paths::DataRoot());
            }
            ImGui::SameLine(0.0f, U(8.0f));
            if (ui::GhostButton("Copy path", ImVec2(U(140.0f), U(36.0f)), true, Icon::Copy)) {
                ImGui::SetClipboardText(paths::DataRoot().c_str());
                app.PushToast(ToastKind::Info, "Path copied");
            }
            break;
        }
        default: DrawAboutTab(app); break;
    }
}

// ---------------------------------------------------------------------------
// Command palette
// ---------------------------------------------------------------------------

struct Command {
    const char* name;
    const char* hint;
    Icon icon;
};

void RunCommand(AstraApp& app, const std::string& name) {
    if (name == "Go to project hub") {
        app.CloseProject();
    } else if (name == "Save scene") {
        app.SaveScene();
    } else if (name == "New scene") {
        app.NewScene();
    } else if (name == "Rebuild sample scene") {
        app.RefreshDemoContent();
    } else if (name == "Toggle output log") {
        app.editor.showConsole = !app.editor.showConsole;
    } else if (name == "Toggle inspector") {
        app.editor.showInspector = !app.editor.showInspector;
    } else if (name == "Toggle statistics") {
        app.editor.showStatistics = !app.editor.showStatistics;
    } else if (name == "Open settings") {
        app.showSettings = true;
    } else if (name == "Sign out") {
        app.SignOut();
    } else if (name == "Sync repositories") {
        app.StartRepoSync();
    }
}

}  // namespace

void DrawToastHost(AstraApp& app) {
    if (app.toasts.empty()) return;
    const ImGuiViewport* viewport = ImGui::GetMainViewport();
    float y = viewport->Pos.y + viewport->Size.y - U(24.0f);
    const float width = U(330.0f);
    const float x = viewport->Pos.x + viewport->Size.x - width - U(20.0f);

    for (auto it = app.toasts.rbegin(); it != app.toasts.rend(); ++it) {
        Toast& toast = *it;
        const float fadeIn = ImMin(1.0f, toast.age / 0.18f);
        const float fadeOut = ImMin(1.0f, (toast.ttl - toast.age) / 0.4f);
        const float alpha = ImClamp(ImMin(fadeIn, fadeOut), 0.0f, 1.0f);
        const float slide = (1.0f - ui::EaseOutCubic(fadeIn)) * U(16.0f);

        const float height = toast.body.empty() ? U(48.0f) : U(66.0f);
        y -= height;

        ImGui::SetNextWindowPos(ImVec2(x + slide, y));
        ImGui::SetNextWindowSize(ImVec2(width, height));
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
        ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, U(12.0f));
        ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
        ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0, 0, 0, 0));
        ImGui::Begin(str::Format("##toast%p", static_cast<void*>(&toast)).c_str(), nullptr,
                     ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoInputs |
                         ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoFocusOnAppearing |
                         ImGuiWindowFlags_NoBringToFrontOnFocus);

        ImDrawList* dl = ImGui::GetWindowDrawList();
        const ImVec2 min = ImGui::GetWindowPos();
        const ImVec2 max(min.x + width, min.y + height);
        const ImU32 accent = ui::Fade(ToastColor(toast.kind), alpha);

        dl->AddRectFilled(min, max, ui::Fade(Colors().surfaceRaised, alpha * 0.98f), U(12.0f));
        dl->AddRect(min, max, ui::Fade(Colors().strokeStrong, alpha), U(12.0f), 0, U(1.0f));
        dl->AddRectFilled(min, ImVec2(min.x + U(3.0f), max.y), accent, U(2.0f));

        const ImVec2 iconCenter(min.x + U(24.0f), min.y + U(24.0f));
        dl->AddCircleFilled(iconCenter, U(13.0f), ui::Fade(ToastColor(toast.kind), alpha * 0.16f), 20);
        ui::DrawIcon(dl, ToastIcon(toast.kind), iconCenter, U(13.0f), accent, U(1.7f));

        dl->AddText(Text().small, Text().small->FontSize, ImVec2(min.x + U(46.0f), min.y + U(12.0f)),
                    ui::Fade(Colors().text, alpha), toast.title.c_str());
        if (!toast.body.empty()) {
            dl->AddText(Text().tiny, Text().tiny->FontSize, ImVec2(min.x + U(46.0f), min.y + U(32.0f)),
                        ui::Fade(Colors().textDim, alpha), str::Ellipsis(toast.body, 54).c_str());
        }
        ImGui::End();
        ImGui::PopStyleColor();
        ImGui::PopStyleVar(3);

        y -= U(8.0f);
    }
}

void DrawSettingsModal(AstraApp& app) {
    if (!app.showSettings) return;

    const ImGuiViewport* viewport = ImGui::GetMainViewport();

    const ImVec2 size(ImMin(U(760.0f), viewport->Size.x - U(60.0f)),
                      ImMin(U(520.0f), viewport->Size.y - U(60.0f)));
    const ImVec2 cardMin(viewport->Pos.x + (viewport->Size.x - size.x) * 0.5f,
                         viewport->Pos.y + (viewport->Size.y - size.y) * 0.5f);

    // Full-screen overlay window: its translucent background dims every panel behind it,
    // and the settings card is drawn inside it at the centred rect.
    ImGui::SetNextWindowPos(viewport->Pos);
    ImGui::SetNextWindowSize(viewport->Size);
    ImGui::SetNextWindowFocus();  // keeps the overlay above the editor panels
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
    ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0.024f, 0.024f, 0.035f, 0.62f));
    ImGui::PushStyleColor(ImGuiCol_Border, ImVec4(0, 0, 0, 0));

    ImGui::Begin("##settings_modal", nullptr,
                 ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove |
                     ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoScrollbar);

    static const char* kTabs[] = {"Appearance", "GitHub", "Account", "Storage", "About"};
    static const Icon kTabIcons[] = {Icon::Sun, Icon::Github, Icon::User, Icon::Database,
                                     Icon::Info};

    ImDrawList* dl = ImGui::GetWindowDrawList();
    const ImVec2 min = cardMin;
    dl->AddRectFilled(min, ImVec2(min.x + size.x, min.y + size.y), Colors().surface, U(16.0f));
    dl->AddRect(min, ImVec2(min.x + size.x, min.y + size.y), Colors().strokeStrong, U(16.0f), 0,
                U(1.0f));
    const float sidebarWidth = U(210.0f);

    dl->AddRectFilled(min, ImVec2(min.x + sidebarWidth, min.y + size.y), Colors().surfaceSunken,
                      U(16.0f), ImDrawFlags_RoundCornersLeft);
    dl->AddLine(ImVec2(min.x + sidebarWidth, min.y), ImVec2(min.x + sidebarWidth, min.y + size.y),
                Colors().stroke, U(1.0f));

    ImGui::SetCursorScreenPos(ImVec2(min.x + U(20.0f), min.y + U(22.0f)));
    ui::Text(Text().h3, Colors().text, "Studio settings");
    ui::VSpace(2.0f);
    ui::TextFaint("v%s  ·  %s", kEngineVersion, kBuildChannel);

    float tabY = min.y + U(84.0f);
    for (int i = 0; i < 5; ++i) {
        const ImRect tabBb(ImVec2(min.x + U(10.0f), tabY),
                           ImVec2(min.x + sidebarWidth - U(10.0f), tabY + U(34.0f)));
        ImGui::SetCursorScreenPos(tabBb.Min);
        ImGui::PushID(i);
        ImGui::InvisibleButton("##tab", tabBb.GetSize());
        const bool hovered = ImGui::IsItemHovered();
        if (ImGui::IsItemClicked()) app.settingsTab = i;
        ImGui::PopID();

        const bool active = app.settingsTab == i;
        if (active) {
            dl->AddRectFilled(tabBb.Min, tabBb.Max, ui::Fade(Colors().brand, 0.14f), U(8.0f));
            dl->AddRectFilled(tabBb.Min, ImVec2(tabBb.Min.x + U(2.5f), tabBb.Max.y), Colors().brand,
                              U(2.0f));
        } else if (hovered) {
            dl->AddRectFilled(tabBb.Min, tabBb.Max, Colors().surfaceHover, U(8.0f));
        }
        ui::DrawIcon(dl, kTabIcons[i], ImVec2(tabBb.Min.x + U(20.0f), tabBb.Min.y + U(17.0f)), U(14.0f),
                     active ? Colors().brand : Colors().textFaint, U(1.6f));
        dl->AddText(Text().small, Text().small->FontSize, ImVec2(tabBb.Min.x + U(36.0f), tabBb.Min.y + U(9.0f)),
                    active ? Colors().text : Colors().textDim, kTabs[i]);
        tabY += U(40.0f);
    }

    // Close button.
    ImGui::SetCursorScreenPos(ImVec2(min.x + sidebarWidth - U(36.0f), min.y + U(16.0f)));
    ImGui::SetCursorScreenPos(ImVec2(min.x + size.x - U(40.0f), min.y + U(14.0f)));
    if (ui::IconButton("##settings_close", Icon::Close, "Close", false, 28.0f)) {
        app.showSettings = false;
    }

    // Content.
    ImGui::SetCursorScreenPos(ImVec2(min.x + sidebarWidth + U(24.0f), min.y + U(24.0f)));
    ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0, 0, 0, 0));
    ImGui::BeginChild("##settings_body", ImVec2(size.x - sidebarWidth - U(40.0f), size.y - U(48.0f)),
                      ImGuiChildFlags_None, ImGuiWindowFlags_None);
    DrawSettingsBody(app);
    ImGui::EndChild();
    ImGui::PopStyleColor();

    ImGui::End();
    ImGui::PopStyleColor(2);
    ImGui::PopStyleVar(3);

    if (ImGui::IsKeyPressed(ImGuiKey_Escape, false)) app.showSettings = false;
}

namespace screens {

void SettingsModal(AstraApp& app) { DrawSettingsModal(app); }

void Overlays(AstraApp& app) {
    DrawSettingsModal(app);

    if (app.showCommandPalette) {
        const ImGuiViewport* viewport = ImGui::GetMainViewport();
        const ImVec2 cardSize(U(520.0f), U(392.0f));
        const ImVec2 cardMin(viewport->Pos.x + (viewport->Size.x - cardSize.x) * 0.5f,
                             viewport->Pos.y + viewport->Size.y * 0.22f);

        // Full-screen overlay window: dims the studio and owns the z-order, so the palette
        // can never be clipped by an editor panel.
        ImGui::SetNextWindowPos(viewport->Pos);
        ImGui::SetNextWindowSize(viewport->Size);
        ImGui::SetNextWindowFocus();
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
        ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
        ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0.024f, 0.024f, 0.035f, 0.55f));
        ImGui::Begin("##command_palette", nullptr,
                     ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize |
                         ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoScrollbar |
                         ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoScrollWithMouse);

        ImGui::SetCursorScreenPos(cardMin);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(U(16.0f), U(14.0f)));
        ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, U(14.0f));
        ImGui::PushStyleColor(ImGuiCol_ChildBg, ui::ToVec4(Colors().surface));
        ImGui::PushStyleColor(ImGuiCol_Border, ui::ToVec4(Colors().strokeStrong));
        ImGui::BeginChild("##palette_card", cardSize, ImGuiChildFlags_Borders,
                          ImGuiWindowFlags_NoScrollbar);

        ui::SearchField("##command_search", app.commandText, cardSize.x - U(32.0f),
                        "Type a command...");
        ui::VSpace(10.0f);

        static const Command kCommands[] = {
            {"Go to project hub", "Launcher", Icon::ChevronLeft},
            {"Save scene", "Ctrl+S", Icon::Save},
            {"New scene", "Scene", Icon::Plus},
            {"Rebuild sample scene", "Scene", Icon::Refresh},
            {"Toggle output log", "Window", Icon::Terminal},
            {"Toggle inspector", "Window", Icon::Sliders},
            {"Toggle statistics", "Window", Icon::Gauge},
            {"Sync repositories", "GitHub", Icon::Wifi},
            {"Open settings", "Studio", Icon::Gear},
            {"Sign out", "Account", Icon::Logout},
        };

        const std::string needle = str::ToLower(app.commandText);
        ImGui::BeginChild("##command_list", ImVec2(0, cardSize.y - U(124.0f)), ImGuiChildFlags_None,
                          ImGuiWindowFlags_None);
        int shown = 0;
        for (const auto& command : kCommands) {
            if (!needle.empty() && !str::Contains(str::ToLower(command.name), needle)) continue;
            ++shown;
            ImGui::PushID(command.name);
            const ImVec2 pos = ImGui::GetCursorScreenPos();
            const float rowWidth = ImGui::GetContentRegionAvail().x;
            ImGui::InvisibleButton("##cmd", ImVec2(rowWidth, U(34.0f)));
            const bool hovered = ImGui::IsItemHovered();
            if (ImGui::IsItemClicked()) {
                RunCommand(app, command.name);
                app.showCommandPalette = false;
            }
            ImDrawList* dl = ImGui::GetWindowDrawList();
            if (hovered) {
                dl->AddRectFilled(pos, ImVec2(pos.x + rowWidth, pos.y + U(34.0f)),
                                  Colors().surfaceHover, U(8.0f));
            }
            ui::DrawIcon(dl, command.icon, ImVec2(pos.x + U(18.0f), pos.y + U(17.0f)), U(14.0f),
                         hovered ? Colors().brand : Colors().textFaint, U(1.6f));
            dl->AddText(Text().small, Text().small->FontSize, ImVec2(pos.x + U(36.0f), pos.y + U(9.0f)),
                        Colors().text, command.name);
            const float hintWidth = Text().tiny->CalcTextSizeA(Text().tiny->FontSize, FLT_MAX, 0.0f,
                                                               command.hint).x;
            dl->AddText(Text().tiny, Text().tiny->FontSize,
                        ImVec2(pos.x + rowWidth - hintWidth - U(8.0f), pos.y + U(11.0f)),
                        Colors().textFaint, command.hint);
            ImGui::PopID();
        }
        if (shown == 0) {
            ui::TextFaint("No commands match \"%s\".", app.commandText.c_str());
        }
        ImGui::EndChild();

        ui::Divider(0.6f, 2.0f, 8.0f);
        ui::TextFaint("Enter to run  ·  Esc to close");

        ImGui::EndChild();
        ImGui::PopStyleColor(2);
        ImGui::PopStyleVar(2);

        if (ImGui::IsKeyPressed(ImGuiKey_Escape, false)) app.showCommandPalette = false;
        ImGui::End();
        ImGui::PopStyleColor();
        ImGui::PopStyleVar(3);
    }

    DrawToastHost(app);
}

}  // namespace screens

void DrawAstraApp(AstraApp& app) {
    ImGuiIO& io = ImGui::GetIO();
    if (!app.initialized) app.Init(app.headless);
    app.Tick(io.DeltaTime);

    const ImGuiViewport* viewport = ImGui::GetMainViewport();
    ImGui::SetNextWindowPos(viewport->Pos);
    ImGui::SetNextWindowSize(viewport->Size);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
    ImGui::PushStyleColor(ImGuiCol_WindowBg, ui::ToVec4(Colors().bg));
    ImGui::Begin("##astra_root", nullptr,
                 ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove |
                     ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse |
                     ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoBringToFrontOnFocus |
                     ImGuiWindowFlags_NoNavFocus | ImGuiWindowFlags_NoCollapse);

    switch (app.screen) {
        case Screen::Boot: screens::Boot(app); break;
        case Screen::SignIn: screens::SignIn(app); break;
        case Screen::RegisterCode: screens::RegisterCode(app); break;
        case Screen::RegisterAccount: screens::RegisterAccount(app); break;
        case Screen::ProjectHub: screens::ProjectHub(app); break;
        case Screen::Editor: screens::Editor(app); break;
    }

    // Curtain used for screen transitions (draw-list primitives cannot be alpha-multiplied,
    // so a short cover/reveal reads better than a broken cross-fade).
    if (app.transition < 1.0f) {
        const float t = ui::EaseOutCubic(ImClamp(app.transition, 0.0f, 1.0f));
        ImGui::GetWindowDrawList()->AddRectFilled(
            viewport->Pos, ImVec2(viewport->Pos.x + viewport->Size.x,
                                  viewport->Pos.y + viewport->Size.y),
            ui::Fade(Colors().bg, 1.0f - t));
    }

    ImGui::End();
    ImGui::PopStyleColor();
    ImGui::PopStyleVar(2);

    screens::Overlays(app);
}

}  // namespace astra::app
