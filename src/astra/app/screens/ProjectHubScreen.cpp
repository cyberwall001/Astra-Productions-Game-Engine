// app/screens/ProjectHubScreen.cpp - choose a project from the GitHub repos you can access.
#include <algorithm>
#include <cmath>

#include "astra/app/Screens.h"
#include "astra/app/screens/Common.h"
#include "astra/core/Strings.h"
#include "astra/core/Time.h"
#include "astra/core/Version.h"
#include "astra/ui/Branding.h"
#include "astra/ui/Widgets.h"
#include "imgui.h"
#include "imgui_internal.h"

namespace astra::app::screens {
namespace {

using ui::Colors;
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

bool MatchesFilter(const github::Repo& repo, const HubState& hub) {
    switch (hub.filter) {
        case 1: if (!repo.hasAstraProject) return false; break;
        case 2: if (!repo.isPrivate) return false; break;
        case 3: if (!repo.isArchived) return false; break;
        default: break;
    }
    if (!hub.search.empty()) {
        const std::string needle = str::ToLower(hub.search);
        const bool hit = str::Contains(str::ToLower(repo.name), needle) ||
                         str::Contains(str::ToLower(repo.description), needle) ||
                         str::Contains(str::ToLower(repo.owner), needle) ||
                         str::Contains(str::ToLower(repo.language), needle);
        if (!hit) return false;
    }
    return true;
}

std::vector<github::Repo> FilteredRepos(const AstraApp& app) {
    std::vector<github::Repo> out;
    for (const auto& repo : app.repos.repos) {
        if (MatchesFilter(repo, app.hub)) out.push_back(repo);
    }
    const int sortMode = app.hub.sortMode;
    std::sort(out.begin(), out.end(), [sortMode](const github::Repo& a, const github::Repo& b) {
        switch (sortMode) {
            case 1: return str::ToLower(a.name) < str::ToLower(b.name);
            case 2: return a.stars > b.stars;
            default: return a.pushedUtc > b.pushedUtc;
        }
    });
    return out;
}

void RepoCard(AstraApp& app, const github::Repo& repo, ImVec2 size) {
    ImGuiWindow* window = ImGui::GetCurrentWindow();
    ImDrawList* dl = window->DrawList;

    ImGui::PushID(repo.fullName.c_str());
    const ImVec2 pos = ImGui::GetCursorScreenPos();
    const ImRect bb(pos, ImVec2(pos.x + size.x, pos.y + size.y));
    ImGui::InvisibleButton("##card", size);
    const bool hovered = ImGui::IsItemHovered();
    const bool clicked = ImGui::IsItemClicked();

    // Overlay widgets (badge pill) would otherwise leave their own item metrics behind and
    // push the next card onto the wrong line, so snapshot the card's layout state and restore it.
    ImGuiWindow* layoutWindow = ImGui::GetCurrentWindow();
    const ImVec2 savedCursor = layoutWindow->DC.CursorPos;
    const ImVec2 savedPrevLine = layoutWindow->DC.CursorPosPrevLine;
    const ImVec2 savedPrevLineSize = layoutWindow->DC.PrevLineSize;
    const ImVec2 savedCursorMax = layoutWindow->DC.CursorMaxPos;

    const float lift = hovered ? U(2.0f) : 0.0f;
    const ImRect drawBb(ImVec2(bb.Min.x, bb.Min.y - lift), ImVec2(bb.Max.x, bb.Max.y - lift));

    if (hovered) {
        dl->AddRect(ImVec2(drawBb.Min.x - U(2.0f), drawBb.Min.y - U(2.0f)),
                    ImVec2(drawBb.Max.x + U(2.0f), drawBb.Max.y + U(2.0f)),
                    ui::Fade(Colors().brand, 0.16f), U(15.0f), 0, U(4.0f));
    }
    dl->AddRectFilled(drawBb.Min, drawBb.Max,
                      hovered ? Colors().surfaceHover : Colors().surface, U(13.0f));
    dl->AddRect(drawBb.Min, drawBb.Max, hovered ? ui::Fade(Colors().brand, 0.55f) : Colors().stroke,
                U(13.0f), 0, U(1.0f));

    const float pad = U(16.0f);
    const ImVec2 iconCenter(drawBb.Min.x + pad + U(11.0f), drawBb.Min.y + pad + U(11.0f));
    dl->AddCircleFilled(iconCenter, U(15.0f),
                        ui::Fade(repo.hasAstraProject ? Colors().brand : Colors().textFaint, 0.14f), 20);
    ui::DrawIcon(dl, repo.hasAstraProject ? ui::Icon::Viewport : ui::Icon::Folder, iconCenter, U(15.0f),
                 repo.hasAstraProject ? Colors().brand : Colors().textDim, U(1.6f));

    // Title row.
    const float titleX = iconCenter.x + U(24.0f);
    dl->AddText(Text().medium, Text().medium->FontSize, ImVec2(titleX, drawBb.Min.y + pad + U(1.0f)),
                Colors().text, str::Ellipsis(repo.name, 22).c_str());

    if (repo.isPrivate) {
        ui::DrawIcon(dl, ui::Icon::Lock,
                     ImVec2(drawBb.Max.x - pad - U(7.0f), drawBb.Min.y + pad + U(11.0f)), U(13.0f),
                     Colors().textFaint, U(1.5f));
    }

    // Description.
    const std::string description =
        repo.description.empty() ? "No description provided." : repo.description;
    const ImVec2 descPos(drawBb.Min.x + pad, drawBb.Min.y + pad + U(34.0f));
    dl->PushClipRect(ImVec2(drawBb.Min.x, descPos.y),
                     ImVec2(drawBb.Max.x - pad, descPos.y + U(38.0f)), true);
    ImGui::PushStyleColor(ImGuiCol_Text, ui::ToVec4(Colors().textDim));
    dl->AddText(Text().small, Text().small->FontSize, descPos, Colors().textDim,
                str::Ellipsis(description, 74).c_str());
    if (description.size() > 74) {
        dl->AddText(Text().small, Text().small->FontSize, ImVec2(descPos.x, descPos.y + U(17.0f)),
                    Colors().textDim, (" - " + str::Ellipsis(description.substr(68), 70)).c_str());
    }
    ImGui::PopStyleColor();
    dl->PopClipRect();

    // Astra project badge.
    const float badgeY = drawBb.Max.y - U(54.0f);
    if (repo.hasAstraProject) {
        ImGui::SetCursorScreenPos(ImVec2(drawBb.Min.x + pad, badgeY));
        ui::Pill("ASTRA PROJECT", ui::Icon::Sparkle, Colors().brand);
    } else if (repo.isArchived) {
        ImGui::SetCursorScreenPos(ImVec2(drawBb.Min.x + pad, badgeY));
        ui::Pill("ARCHIVED", ui::Icon::Clock, Colors().textFaint);
    }

    // Footer: language, stars, updated.
    const ImVec2 langPos(drawBb.Min.x + pad, drawBb.Max.y - U(24.0f));
    if (!repo.language.empty()) {
        dl->AddCircleFilled(ImVec2(langPos.x + U(4.0f), langPos.y + U(7.0f)), U(4.0f),
                            Colors().LanguageColor(repo.language), 10);
        dl->AddText(Text().tiny, Text().tiny->FontSize, ImVec2(langPos.x + U(13.0f), langPos.y),
                    Colors().textFaint, repo.language.c_str());
    }
    const std::string meta = str::Format("* %d   %s", repo.stars,
                                         time::Relative(repo.pushedUtc, time::NowUtc()).c_str());
    const float metaWidth = Text().tiny->CalcTextSizeA(Text().tiny->FontSize, FLT_MAX, 0.0f,
                                                       meta.c_str()).x;
    dl->AddText(Text().tiny, Text().tiny->FontSize,
                ImVec2(drawBb.Max.x - pad - metaWidth, langPos.y), Colors().textFaint, meta.c_str());

    // Hover affordance.
    if (hovered) {
        dl->AddText(Text().small, Text().small->FontSize,
                    ImVec2(drawBb.Max.x - pad - U(74.0f), drawBb.Max.y - U(56.0f)),
                    Colors().brand, "Open  ->");
    }

    layoutWindow->DC.CursorPos = savedCursor;
    layoutWindow->DC.CursorPosPrevLine = savedPrevLine;
    layoutWindow->DC.PrevLineSize = savedPrevLineSize;
    layoutWindow->DC.CursorMaxPos = savedCursorMax;
    if (hovered) ImGui::SetMouseCursor(ImGuiMouseCursor_Hand);
    if (clicked) app.OpenRepo(repo);
    ImGui::PopID();
}

void SkeletonCard(ImVec2 size, float time) {
    const ImVec2 pos = ImGui::GetCursorScreenPos();
    ImDrawList* dl = ImGui::GetWindowDrawList();
    dl->AddRectFilled(pos, ImVec2(pos.x + size.x, pos.y + size.y), Colors().surface, U(13.0f));
    dl->AddRect(pos, ImVec2(pos.x + size.x, pos.y + size.y), Colors().stroke, U(13.0f), 0, U(1.0f));

    ImGui::SetCursorScreenPos(ImVec2(pos.x + U(16.0f), pos.y + U(18.0f)));
    ui::SkeletonBar(ImVec2(U(22.0f), U(22.0f)), time, 11.0f);
    ImGui::SetCursorScreenPos(ImVec2(pos.x + U(48.0f), pos.y + U(22.0f)));
    ui::SkeletonBar(ImVec2(size.x * 0.45f, U(12.0f)), time + 0.2f);
    ImGui::SetCursorScreenPos(ImVec2(pos.x + U(16.0f), pos.y + U(60.0f)));
    ui::SkeletonBar(ImVec2(size.x - U(32.0f), U(9.0f)), time + 0.4f);
    ImGui::SetCursorScreenPos(ImVec2(pos.x + U(16.0f), pos.y + U(76.0f)));
    ui::SkeletonBar(ImVec2(size.x * 0.6f, U(9.0f)), time + 0.6f);
    ImGui::SetCursorScreenPos(ImVec2(pos.x + U(16.0f), pos.y + size.y - U(30.0f)));
    ui::SkeletonBar(ImVec2(U(90.0f), U(16.0f)), time + 0.8f, 8.0f);
    ImGui::SetCursorScreenPos(pos);
    ImGui::Dummy(size);
}

void TopBar(AstraApp& app, ImVec2 min, float width) {
    ImDrawList* dl = ImGui::GetWindowDrawList();
    const float height = U(60.0f);
    dl->AddRectFilled(min, ImVec2(min.x + width, min.y + height), Colors().editorBar);
    dl->AddLine(ImVec2(min.x, min.y + height), ImVec2(min.x + width, min.y + height),
                Colors().stroke, U(1.0f));

    const float logoSize = U(34.0f);
    const ImVec2 logoPos(min.x + U(24.0f), min.y + (height - logoSize) * 0.5f);
    const ui::branding::Image* logo = ui::branding::Logo();
    if (logo && logo->texture != 0) {
        dl->AddImage(logo->texture, logoPos, ImVec2(logoPos.x + logoSize, logoPos.y + logoSize));
    } else {
        ui::BrandMark(dl, ImVec2(logoPos.x + logoSize * 0.5f, logoPos.y + logoSize * 0.5f),
                      logoSize * 0.45f, Colors().brand, Colors().brandDeep, true, 0.3f);
    }

    ImGui::SetCursorScreenPos(ImVec2(logoPos.x + logoSize + U(12.0f), min.y + U(14.0f)));
    ImGui::Dummy(ImVec2(U(1.0f), U(1.0f)));
    const float lockupX = logoPos.x + logoSize + U(14.0f);
    SpacedText(dl, Text().h3, Colors().text, ImVec2(lockupX, min.y + U(11.0f)), "ASTRA", U(3.2f));
    SpacedText(dl, Text().tiny, Colors().textFaint, ImVec2(lockupX, min.y + U(34.0f)),
               "PROJECT HUB", U(1.8f));

    // Right side: sync status, account, actions.
    float rightX = min.x + width - U(24.0f);

    ImGui::SetCursorScreenPos(ImVec2(rightX - U(30.0f), min.y + U(15.0f)));
    if (ui::IconButton("##signout", ui::Icon::Logout, "Sign out", false, 30.0f)) {
        app.SignOut();
    }
    rightX -= U(38.0f);

    ImGui::SetCursorScreenPos(ImVec2(rightX - U(30.0f), min.y + U(15.0f)));
    if (ui::IconButton("##settings", ui::Icon::Gear, "Studio settings", false, 30.0f)) {
        app.showSettings = true;
        app.settingsTab = 0;
    }
    rightX -= U(28.0f);

    const auth::Account* account = app.CurrentUser();
    const std::string name = account ? account->displayName : (app.guestMode ? "Guest" : "Not signed in");
    const std::string role = account ? license::RoleBadge(account->role) : "DEMO";
    const ImU32 accent = account ? AccentFromHex(account->accentHex, Colors().brand) : Colors().info;

    ImGui::SetCursorScreenPos(ImVec2(rightX - U(196.0f), min.y + U(13.0f)));
    ImGui::InvisibleButton("##account", ImVec2(U(196.0f), U(34.0f)));
    const bool accountHovered = ImGui::IsItemHovered();
    if (ImGui::IsItemClicked()) {
        app.showSettings = true;
        app.settingsTab = 2;
    }
    {
        const ImVec2 boxMin(rightX - U(196.0f), min.y + U(12.0f));
        const ImVec2 boxMax(boxMin.x + U(196.0f), boxMin.y + U(36.0f));
        dl->AddRectFilled(boxMin, boxMax,
                          accountHovered ? Colors().surfaceHover : Colors().surfaceRaised, U(10.0f));
        dl->AddRect(boxMin, boxMax, Colors().stroke, U(10.0f), 0, U(1.0f));
        ImGui::SetCursorScreenPos(ImVec2(boxMin.x + U(5.0f), boxMin.y + U(2.0f)));
        ui::Avatar(account ? account->Initials().c_str() : "GU", accent, 28.0f, false);
        dl->AddText(Text().small, Text().small->FontSize, ImVec2(boxMin.x + U(42.0f), boxMin.y + U(5.0f)),
                    Colors().text, str::Ellipsis(name, 18).c_str());
        dl->AddText(Text().tiny, Text().tiny->FontSize,
                    ImVec2(boxMin.x + U(42.0f), boxMin.y + U(21.0f)), ui::Fade(accent, 0.9f),
                    role.c_str());
    }
    rightX -= U(206.0f);

    // Sync state pill.
    ImGui::SetCursorScreenPos(ImVec2(rightX - U(150.0f), min.y + U(19.0f)));
    if (app.repos.state == github::FetchState::Loading) {
        ui::Pill("SYNCING", ui::Icon::Refresh, Colors().warn);
    } else if (app.repos.state == github::FetchState::Failed) {
        ui::Pill("SYNC FAILED", ui::Icon::WifiOff, Colors().danger);
    } else if (app.repos.demo) {
        ui::Pill("DEMO WORKSPACE", ui::Icon::Sparkle, Colors().info);
    } else if (app.repos.state == github::FetchState::Ready) {
        ui::Pill("GITHUB LIVE", ui::Icon::Wifi, Colors().success);
    } else {
        ui::Pill("OFFLINE", ui::Icon::Cloud, Colors().textFaint);
    }
    ImGui::SetCursorScreenPos(ImVec2(min.x, min.y));
}

void Sidebar(AstraApp& app, ImVec2 pos, ImVec2 size) {
    ImGui::SetCursorScreenPos(pos);
    ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0, 0, 0, 0));
    ImGui::BeginChild("##hub_sidebar", size, ImGuiChildFlags_None,
                      ImGuiWindowFlags_NoScrollbar);

    // Connected account card.
    ui::BeginCard("##gh_card", ImVec2(size.x, U(158.0f)), Colors().surface);
    const bool connected = !app.GitHubToken().empty();
    ui::Overline("GITHUB CONNECTION", Colors().textFaint);
    ui::VSpace(6.0f);
    ImGui::PushStyleColor(ImGuiCol_Text, ui::ToVec4(Colors().text));
    ui::PushFont(Text().medium);
    ImGui::TextUnformatted(connected ? (app.repos.user.login.empty() ? "Connected" : app.repos.user.login.c_str())
                                     : "Demo workspace");
    ui::PopFont();
    ImGui::PopStyleColor();
    ui::TextFaint(connected ? "Personal access token stored" : "Sample repositories, no account needed");
    ui::VSpace(10.0f);
    if (ui::SecondaryButton(connected ? "Manage token" : "Connect GitHub",
                            ImVec2(ImGui::GetContentRegionAvail().x, U(34.0f)), true,
                            ui::Icon::Github)) {
        app.hub.showTokenPanel = true;
        app.hub.tokenInput.clear();
        app.hub.tokenError.clear();
    }
    ui::EndCard();

    ui::VSpace(12.0f);

    // New project card.
    ui::BeginCard("##new_project", ImVec2(size.x, U(160.0f)), Colors().surface);
    ui::Overline("NEW LOCAL PROJECT", Colors().textFaint);
    ui::VSpace(8.0f);
    ui::TextField("##new_project_name", app.hub.newProjectName, "Project name", ui::Icon::Folder);
    ui::VSpace(8.0f);
    if (ui::SecondaryButton("Create project", ImVec2(ImGui::GetContentRegionAvail().x, U(34.0f)),
                            !app.hub.newProjectName.empty(), ui::Icon::Plus)) {
        app.CreateProject(app.hub.newProjectName);
        app.hub.newProjectName.clear();
    }
    ui::EndCard();

    ui::VSpace(12.0f);

    // Recent projects. Height is whatever is left in the sidebar so the card never clips.
    const float recentHeight =
        ImMax(U(150.0f), (pos.y + size.y) - ImGui::GetCursorScreenPos().y - U(1.0f));
    ui::BeginCard("##recent", ImVec2(size.x, recentHeight), Colors().surface);
    ui::Overline("RECENT PROJECTS", Colors().textFaint);
    ui::VSpace(8.0f);
    const auto entries = app.recents.Entries();
    if (entries.empty()) {
        ui::VSpace(6.0f);
        ui::TextFaint("No projects yet.");
        ui::VSpace(4.0f);
        ui::TextWrapped("Open an Astra repository or create a local project to get started.",
                        Colors().textFaint, size.x - U(30.0f));
    } else {
        for (std::size_t i = 0; i < entries.size() && i < 6; ++i) {
            const auto& info = entries[i];
            ImGui::PushID(static_cast<int>(i));
            const ImVec2 rowPos = ImGui::GetCursorScreenPos();
            const float rowHeight = U(42.0f);
            ImGui::InvisibleButton("##recent_row", ImVec2(size.x - U(28.0f), rowHeight));
            const bool hovered = ImGui::IsItemHovered();
            if (ImGui::IsItemClicked()) app.OpenProject(info);
            ImDrawList* dl = ImGui::GetWindowDrawList();
            if (hovered) {
                dl->AddRectFilled(rowPos, ImVec2(rowPos.x + size.x - U(28.0f), rowPos.y + rowHeight),
                                  Colors().surfaceHover, U(8.0f));
            }
            ui::DrawIcon(dl, ui::Icon::Viewport,
                         ImVec2(rowPos.x + U(14.0f), rowPos.y + rowHeight * 0.5f), U(15.0f),
                         Colors().brand, U(1.6f));
            dl->AddText(Text().small, Text().small->FontSize, ImVec2(rowPos.x + U(32.0f), rowPos.y + U(5.0f)),
                        Colors().text, str::Ellipsis(info.name, 20).c_str());
            dl->AddText(Text().tiny, Text().tiny->FontSize,
                        ImVec2(rowPos.x + U(32.0f), rowPos.y + U(22.0f)), Colors().textFaint,
                        time::Relative(info.lastOpenedUtc, time::NowUtc()).c_str());
            ImGui::PopID();
        }
    }
    ui::EndCard();

    ImGui::EndChild();
    ImGui::PopStyleColor();
}

void TokenPanel(AstraApp& app) {
    if (!app.hub.showTokenPanel) return;

    const ImGuiViewport* viewport = ImGui::GetMainViewport();
    const ImVec2 size(U(520.0f), U(344.0f));
    ImGui::SetNextWindowPos(ImVec2(viewport->Pos.x + (viewport->Size.x - size.x) * 0.5f,
                                   viewport->Pos.y + (viewport->Size.y - size.y) * 0.5f));
    ImGui::SetNextWindowSize(size);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(U(24.0f), U(20.0f)));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, U(16.0f));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, U(1.0f));
    ImGui::PushStyleColor(ImGuiCol_WindowBg, ui::ToVec4(Colors().surface));
    ImGui::PushStyleColor(ImGuiCol_Border, ui::ToVec4(Colors().strokeStrong));

    ImGui::Begin("Connect GitHub", nullptr,
                 ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoCollapse |
                     ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoTitleBar);

    ui::CardHeader("Connect GitHub", "Personal access token with repo scope", ui::Icon::Github);
    ui::TextWrapped(
        "Astra reads the repositories you already have access to. The token is protected with "
        "Windows DPAPI and stored per user.",
        Colors().textDim, U(468.0f));
    ui::VSpace(14.0f);
    ui::FieldLabel("PERSONAL ACCESS TOKEN", "ghp_...");
    ui::TextField("##token", app.hub.tokenInput, "ghp_xxxxxxxxxxxxxxxxxxxx", ui::Icon::Key, nullptr,
                  !app.hub.tokenError.empty());
    if (!app.hub.tokenError.empty()) {
        ui::VSpace(8.0f);
        ErrorBanner(app.hub.tokenError);
    }
    ui::VSpace(16.0f);
    const bool hasToken = app.hub.tokenInput.size() >= 8;
    if (ui::PrimaryButton("Save and sync", ImVec2(U(230.0f), U(40.0f)), hasToken, ui::Icon::Check)) {
        app.SetGitHubToken(app.hub.tokenInput, "");
        app.hub.showTokenPanel = false;
        app.hub.tokenInput.clear();
        app.PushToast(ToastKind::Success, "GitHub connected", "Syncing your repositories now.");
    }
    ImGui::SameLine(0.0f, U(10.0f));
    if (ui::SecondaryButton("Use demo workspace", ImVec2(U(200.0f), U(40.0f)), true,
                            ui::Icon::Sparkle)) {
        app.SetGitHubToken("", "");
        app.hub.showTokenPanel = false;
        app.StartRepoSync(true);
        app.PushToast(ToastKind::Info, "Demo workspace", "Showing sample repositories.");
    }
    ui::VSpace(6.0f);
    if (ui::LinkButton("Clear stored token", ui::Icon::Trash, Colors().textFaint)) {
        app.SetGitHubToken("", "");
        app.PushToast(ToastKind::Info, "Token cleared");
    }

    ImGui::End();
    ImGui::PopStyleColor(2);
    ImGui::PopStyleVar(3);
}

}  // namespace

void ProjectHub(AstraApp& app) {
    ImGuiWindow* window = ImGui::GetCurrentWindow();
    ImDrawList* dl = window->DrawList;
    const ImVec2 min = window->Pos;
    const ImVec2 max = ImVec2(min.x + window->Size.x, min.y + window->Size.y);
    const float width = window->Size.x;

    dl->AddRectFilled(min, max, Colors().bg);
    ui::RadialGlow(dl, ImVec2(min.x + width * 0.15f, min.y), width * 0.8f, Colors().brand, 0.28f);

    TopBar(app, min, width);

    // Header block.
    const float headerTop = min.y + U(60.0f);
    const float pad = U(28.0f);
    dl->AddText(Text().h1, Text().h1->FontSize, ImVec2(min.x + pad, headerTop + U(22.0f)),
                Colors().text, "Projects");
    const std::string subtitle =
        app.repos.demo ? "Demo repositories - connect GitHub to list your own"
                       : (app.repos.user.login.empty()
                              ? "Repositories from your GitHub account"
                              : ("Repositories for " + app.repos.user.login));
    dl->AddText(Text().small, Text().small->FontSize,
                ImVec2(min.x + pad, headerTop + U(64.0f)), Colors().textDim, subtitle.c_str());

    // Search + actions (right aligned).
    const float searchWidth = U(240.0f);
    float actionRight = min.x + width - pad;
    ImGui::SetCursorScreenPos(ImVec2(actionRight - U(140.0f), headerTop + U(28.0f)));
    if (ui::PrimaryButton("New project", ImVec2(U(140.0f), U(38.0f)), true, ui::Icon::Plus)) {
        const std::string name = "Astra Project";
        app.CreateProject(name + " " + str::Format("%02d", static_cast<int>(app.time) % 100));
    }
    actionRight -= U(150.0f);
    ImGui::SetCursorScreenPos(ImVec2(actionRight - U(34.0f), headerTop + U(28.0f)));
    if (ui::IconButton("##refresh", ui::Icon::Refresh, "Sync repositories", false, 34.0f)) {
        app.StartRepoSync(app.repos.demo);
    }
    actionRight -= U(44.0f);
    ImGui::SetCursorScreenPos(ImVec2(actionRight - searchWidth, headerTop + U(29.0f)));
    ui::SearchField("##hub_search", app.hub.search, searchWidth, "Search repositories...");
    actionRight -= searchWidth + U(12.0f);

    // Filter row.
    const float filterTop = headerTop + U(98.0f);
    static const char* kFilters[] = {"All", "Astra projects", "Private", "Archived"};
    ImGui::SetCursorScreenPos(ImVec2(min.x + pad, filterTop));
    ui::Segmented("##filter", &app.hub.filter, kFilters, 4, U(348.0f));

    static const char* kSorts[] = {"Recently updated", "Name (A-Z)", "Most starred"};
    ImGui::SetCursorScreenPos(ImVec2(min.x + pad + U(362.0f), filterTop));
    ui::Select("##sort", &app.hub.sortMode, kSorts, 3, U(170.0f));

    // View toggle on the right.
    ImGui::SetCursorScreenPos(ImVec2(min.x + width - pad - U(66.0f), filterTop + U(3.0f)));
    if (ui::IconButton("##grid", ui::Icon::Grid, "Grid view", app.hub.gridView, 30.0f)) {
        app.hub.gridView = true;
    }
    ImGui::SetCursorScreenPos(ImVec2(min.x + width - pad - U(32.0f), filterTop + U(3.0f)));
    if (ui::IconButton("##list", ui::Icon::List, "List view", !app.hub.gridView, 30.0f)) {
        app.hub.gridView = false;
    }

    // Body.
    const float bodyTop = filterTop + U(52.0f);
    const float bodyHeight = max.y - bodyTop - U(30.0f);
    const float sidebarWidth = ImMin(U(292.0f), width * 0.30f);
    const bool showSidebar = width > 1040.0f;

    if (showSidebar) {
        Sidebar(app, ImVec2(min.x + pad, bodyTop), ImVec2(sidebarWidth, bodyHeight));
    }

    const float gridX = min.x + pad + (showSidebar ? sidebarWidth + U(20.0f) : 0.0f);
    const float gridWidth = max.x - pad - gridX;

    ImGui::SetCursorScreenPos(ImVec2(gridX, bodyTop));
    ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0, 0, 0, 0));
    ImGui::BeginChild("##repo_grid", ImVec2(gridWidth, bodyHeight), ImGuiChildFlags_None,
                      ImGuiWindowFlags_None);

    const auto filtered = FilteredRepos(app);
    const bool loading = app.repos.state == github::FetchState::Loading ||
                         app.repos.state == github::FetchState::Idle;

    if (app.repos.demo && !loading) {
        InfoBanner("Demo workspace: sample repositories, no token required.", ui::Icon::Sparkle,
                       Colors().info);
        ui::VSpace(12.0f);
    }

    if (loading) {
        const int columns = ImMax(1, static_cast<int>(gridWidth / U(320.0f)));
        const float cardWidth = (gridWidth - U(16.0f) * static_cast<float>(columns - 1)) /
                                static_cast<float>(columns);
        for (int i = 0; i < 6; ++i) {
            SkeletonCard(ImVec2(cardWidth, U(168.0f)), app.time + static_cast<float>(i) * 0.12f);
            if ((i + 1) % columns != 0) ImGui::SameLine(0.0f, U(16.0f));
            else ui::VSpace(16.0f);
        }
    } else if (app.repos.state == github::FetchState::Failed) {
        bool retry = false;
        ui::EmptyState(ui::Icon::WifiOff, "GitHub sync failed", app.repos.error.c_str(), "Retry sync",
                       &retry);
        if (retry) app.StartRepoSync(app.repos.demo);
    } else if (filtered.empty()) {
        bool reset = false;
        ui::EmptyState(ui::Icon::Search, "No repositories match",
                       app.hub.search.empty()
                           ? "Try clearing the filters, or connect a GitHub account with more access."
                           : "Nothing matched your search. Try a different name, language or owner.",
                       "Clear filters", &reset);
        if (reset) {
            app.hub.search.clear();
            app.hub.filter = 0;
        }
    } else if (app.hub.gridView) {
        const int columns = ImMax(1, static_cast<int>(gridWidth / U(320.0f)));
        const float cardWidth = (gridWidth - U(16.0f) * static_cast<float>(columns - 1)) /
                                static_cast<float>(columns);
        int column = 0;
        for (const auto& repo : filtered) {
            RepoCard(app, repo, ImVec2(cardWidth, U(168.0f)));
            ++column;
            if (column % columns != 0) ImGui::SameLine(0.0f, U(16.0f));
            else ui::VSpace(16.0f);
        }
    } else {
        for (const auto& repo : filtered) {
            ImGui::PushID(repo.fullName.c_str());
            const ImVec2 rowPos = ImGui::GetCursorScreenPos();
            const float rowHeight = U(58.0f);
            ImGui::InvisibleButton("##row", ImVec2(gridWidth, rowHeight));
            const bool hovered = ImGui::IsItemHovered();
            if (ImGui::IsItemClicked()) app.OpenRepo(repo);
            ImDrawList* rowDl = ImGui::GetWindowDrawList();
            rowDl->AddRectFilled(rowPos, ImVec2(rowPos.x + gridWidth, rowPos.y + rowHeight),
                                 hovered ? Colors().surfaceHover : Colors().surface, U(10.0f));
            rowDl->AddRect(rowPos, ImVec2(rowPos.x + gridWidth, rowPos.y + rowHeight),
                           Colors().stroke, U(10.0f), 0, U(1.0f));
            ui::DrawIcon(rowDl, repo.hasAstraProject ? ui::Icon::Viewport : ui::Icon::Folder,
                         ImVec2(rowPos.x + U(22.0f), rowPos.y + rowHeight * 0.5f), U(16.0f),
                         repo.hasAstraProject ? Colors().brand : Colors().textDim, U(1.6f));
            rowDl->AddText(Text().medium, Text().medium->FontSize, ImVec2(rowPos.x + U(44.0f), rowPos.y + U(10.0f)),
                           Colors().text, str::Ellipsis(repo.name, 30).c_str());
            rowDl->AddText(Text().tiny, Text().tiny->FontSize, ImVec2(rowPos.x + U(44.0f), rowPos.y + U(31.0f)),
                           Colors().textFaint,
                           (repo.language.empty() ? "no language" : repo.language).c_str());
            const std::string right = str::Format("%s  ·  %s",
                                                  time::Relative(repo.pushedUtc, time::NowUtc()).c_str(),
                                                  repo.isPrivate ? "private" : "public");
            const float rightWidth = Text().tiny->CalcTextSizeA(Text().tiny->FontSize, FLT_MAX, 0.0f,
                                                                right.c_str()).x;
            rowDl->AddText(Text().tiny, Text().tiny->FontSize,
                           ImVec2(rowPos.x + gridWidth - U(20.0f) - rightWidth, rowPos.y + U(22.0f)),
                           Colors().textFaint, right.c_str());
            if (repo.hasAstraProject) {
                ImGui::SetCursorScreenPos(ImVec2(rowPos.x + U(280.0f), rowPos.y + U(18.0f)));
                ui::Pill("ASTRA", ui::Icon::Sparkle, Colors().brand);
                ImGui::SetCursorScreenPos(ImVec2(rowPos.x, rowPos.y + rowHeight));
            }
            ImGui::PopID();
        }
    }

    ImGui::EndChild();
    ImGui::PopStyleColor();

    // Status bar.
    const float statusTop = max.y - U(26.0f);
    dl->AddRectFilled(ImVec2(min.x, statusTop), ImVec2(max.x, max.y), Colors().editorBar);
    dl->AddLine(ImVec2(min.x, statusTop), ImVec2(max.x, statusTop), Colors().stroke, U(1.0f));
    const std::string left =
        app.repos.demo ? "api.github.com  ·  demo fixture"
                       : "api.github.com  ·  " + std::string(app.GitHubToken().empty() ? "no token" : "token active");
    dl->AddText(Text().tiny, Text().tiny->FontSize, ImVec2(min.x + pad, statusTop + U(7.0f)),
                Colors().textFaint, left.c_str());

    std::string right = str::Format("%zu repositories", app.repos.repos.size());
    if (app.repos.rate.remaining >= 0 && !app.repos.demo) {
        right += str::Format("  ·  %d requests left", app.repos.rate.remaining);
    }
    if (!app.repos.repos.empty() && app.repos.finishedAtMs > 0.0) {
        right += "  ·  synced just now";
    }
    const float rightWidth = Text().tiny->CalcTextSizeA(Text().tiny->FontSize, FLT_MAX, 0.0f, right.c_str()).x;
    dl->AddText(Text().tiny, Text().tiny->FontSize, ImVec2(max.x - pad - rightWidth, statusTop + U(7.0f)),
                Colors().textFaint, right.c_str());

    TokenPanel(app);

    ImGui::SetCursorScreenPos(min);
}

}  // namespace astra::app::screens
