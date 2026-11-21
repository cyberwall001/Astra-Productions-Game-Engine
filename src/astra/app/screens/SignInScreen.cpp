// app/screens/SignInScreen.cpp - the saved-account sign-in experience.
#include <cmath>

#include "astra/app/Screens.h"
#include "astra/app/screens/Common.h"
#include "astra/core/Strings.h"
#include "astra/core/Version.h"
#include "astra/ui/Branding.h"
#include "astra/ui/Widgets.h"
#include "imgui.h"

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

void DrawBrandPanel(AstraApp&, ImVec2 min, ImVec2 max, float panelWidth) {
    ImDrawList* dl = ImGui::GetWindowDrawList();
    const ImVec2 pMin(min.x + U(18.0f), min.y + U(18.0f));
    const ImVec2 pMax(pMin.x + panelWidth, max.y - U(18.0f));

    dl->AddRectFilled(pMin, pMax, Colors().surfaceSunken, U(18.0f));
    dl->AddRect(pMin, pMax, Colors().stroke, U(18.0f), 0, U(1.0f));

    // Coral bloom behind the mark.
    ui::RadialGlow(dl, ImVec2(pMin.x + panelWidth * 0.42f, pMin.y + (pMax.y - pMin.y) * 0.30f),
                   panelWidth * 0.85f, Colors().brand, 0.9f);
    ui::Grid(dl, pMin, pMax, U(44.0f), IM_COL32(255, 255, 255, 5));

    // Oversized crescent watermark anchored bottom-right.
    ui::BrandMark(dl, ImVec2(pMax.x - panelWidth * 0.16f, pMax.y - panelWidth * 0.18f),
                  panelWidth * 0.42f, IM_COL32(255, 95, 90, 16), IM_COL32(255, 255, 255, 10), true,
                  0.0f);

    // Lockup.
    const float logoSize = U(84.0f);
    const ui::branding::Image* logo = ui::branding::Logo();
    const ImVec2 logoPos(pMin.x + U(40.0f), pMin.y + U(36.0f));
    if (logo && logo->texture != 0) {
        dl->AddImage(logo->texture, logoPos,
                     ImVec2(logoPos.x + logoSize, logoPos.y + logoSize));
    } else {
        ui::BrandMark(dl, ImVec2(logoPos.x + logoSize * 0.5f, logoPos.y + logoSize * 0.5f),
                      logoSize * 0.5f, Colors().brand, Colors().brandDeep, true, 0.4f);
    }

    ImGui::SetCursorScreenPos(ImVec2(logoPos.x, logoPos.y + logoSize + U(14.0f)));
    Wordmark(Colors().text, Colors().textFaint, false, 0.0f);

    ImGui::SetCursorScreenPos(ImVec2(pMin.x + U(40.0f), pMin.y + (pMax.y - pMin.y) * 0.46f));
    ImGui::PushTextWrapPos(ImGui::GetCursorPosX() + panelWidth - U(96.0f));
    ui::PushFont(Text().h1);
    ImGui::PushStyleColor(ImGuiCol_Text, ui::ToVec4(Colors().text));
    ImGui::TextWrapped("Build the world.\nShip the game.");
    ImGui::PopStyleColor();
    ui::PopFont();
    ImGui::PopTextWrapPos();

    ui::VSpace(12.0f);
    ImGui::SetCursorScreenPos(ImVec2(pMin.x + U(40.0f), ImGui::GetCursorScreenPos().y + U(8.0f)));
    ImGui::PushTextWrapPos(ImGui::GetCursorPosX() + panelWidth - U(110.0f));
    ImGui::PushStyleColor(ImGuiCol_Text, ui::ToVec4(Colors().textDim));
    ui::PushFont(Text().body);
    ImGui::TextWrapped(
        "Astra Engine is a general-purpose game engine: a real level viewport, "
        "actor-based levels, deterministic replays and a studio toolchain for small teams.");
    ui::PopFont();
    ImGui::PopStyleColor();
    ImGui::PopTextWrapPos();

    // Feature bullets.
    struct Bullet {
        ui::Icon icon;
        const char* title;
    };
    static const Bullet kBullets[] = {
        {ui::Icon::Shield, "Local-first accounts - your registry never leaves this machine"},
        {ui::Icon::Github, "Projects synced from the GitHub repositories you can access"},
        {ui::Icon::Key, "One-time access codes issued from Astra KeyForge"},
    };

    float y = pMax.y - U(196.0f);
    for (const auto& bullet : kBullets) {
        const ImVec2 iconPos(pMin.x + U(44.0f), y + U(9.0f));
        dl->AddCircleFilled(iconPos, U(13.0f), ui::Fade(Colors().brand, 0.16f), 20);
        ui::DrawIcon(dl, bullet.icon, iconPos, U(12.0f), Colors().brand, U(1.6f));

        ImGui::PushStyleColor(ImGuiCol_Text, ui::ToVec4(Colors().textDim));
        ui::PushFont(Text().small);
        dl->AddText(Text().small, Text().small->FontSize, ImVec2(iconPos.x + U(22.0f), y),
                    Colors().textDim, bullet.title);
        ui::PopFont();
        ImGui::PopStyleColor();
        y += U(34.0f);
    }

    // Footer.
    dl->AddText(Text().tiny, Text().tiny->FontSize, ImVec2(pMin.x + U(40.0f), pMax.y - U(34.0f)),
                ui::Fade(Colors().textFaint, 0.8f),
                str::Format("%s  ·  ENGINE %s", kCompany, kEngineVersion).c_str());
}

void DrawSavedAccounts(AstraApp& app, float width) {
    const auto accounts = app.auth.RecentAccounts();
    if (accounts.empty()) return;

    ui::Overline("SAVED ON THIS DEVICE", Colors().textFaint);
    ui::VSpace(8.0f);

    const float chipWidth = (width - U(10.0f) * static_cast<float>(accounts.size() - 1)) /
                            static_cast<float>(accounts.size());
    const float chipHeight = U(58.0f);
    ImDrawList* dl = ImGui::GetWindowDrawList();

    for (std::size_t i = 0; i < accounts.size() && i < 3; ++i) {
        const auth::Account* account = accounts[i];
        const ImVec2 pos = ImGui::GetCursorScreenPos();
        ImGui::PushID(static_cast<int>(i));
        const ImRect bb(pos, ImVec2(pos.x + chipWidth, pos.y + chipHeight));
        ImGui::SetCursorScreenPos(pos);
        ImGui::InvisibleButton("##account", ImVec2(chipWidth, chipHeight));
        const bool hovered = ImGui::IsItemHovered();
        if (ImGui::IsItemClicked()) {
            app.signIn.identity = account->username;
            app.signIn.error.clear();
            app.signIn.hint.clear();
        }

        const ImU32 accent = AccentFromHex(account->accentHex, Colors().brand);
        const bool selected = str::EqualsIgnoreCase(app.signIn.identity, account->username);
        dl->AddRectFilled(bb.Min, bb.Max,
                          selected ? ui::Fade(Colors().brand, 0.12f)
                                   : (hovered ? Colors().surfaceHover : Colors().surfaceRaised),
                          U(11.0f));
        dl->AddRect(bb.Min, bb.Max,
                    selected ? ui::Fade(Colors().brand, 0.55f) : Colors().stroke, U(11.0f), 0,
                    U(1.0f));

        const ImVec2 avatarPos(pos.x + U(12.0f), pos.y + (chipHeight - U(32.0f)) * 0.5f);
        ImGui::SetCursorScreenPos(avatarPos);
        ui::Avatar(account->Initials().c_str(), accent, 32.0f, selected);

        dl->AddText(Text().small, Text().small->FontSize, ImVec2(avatarPos.x + U(42.0f), pos.y + U(12.0f)),
                    Colors().text, str::Ellipsis(account->displayName, 14).c_str());
        dl->AddText(Text().tiny, Text().tiny->FontSize, ImVec2(avatarPos.x + U(42.0f), pos.y + U(31.0f)),
                    Colors().textFaint,
                    license::RoleBadge(account->role));

        ImGui::SetCursorScreenPos(ImVec2(pos.x, pos.y));
        ImGui::Dummy(ImVec2(chipWidth, chipHeight));
        ImGui::PopID();
        if (i + 1 < accounts.size() && i < 2) ImGui::SameLine(0.0f, U(10.0f));
    }
    ui::VSpace(14.0f);
}

}  // namespace

void SignIn(AstraApp& app) {
    ImGuiWindow* window = ImGui::GetCurrentWindow();
    ImDrawList* dl = window->DrawList;
    const ImVec2 min = window->Pos;
    const ImVec2 max = ImVec2(min.x + window->Size.x, min.y + window->Size.y);
    const float width = window->Size.x;
    const float height = window->Size.y;

    ui::Backdrop(dl, min, max, app.time, true);

    const bool wideEnough = width > 900.0f;
    float formCenterX = (min.x + max.x) * 0.5f;
    if (wideEnough) {
        const float panelWidth = ImClamp(width * 0.42f, U(360.0f), U(560.0f));
        DrawBrandPanel(app, min, max, panelWidth);
        formCenterX = min.x + U(18.0f) * 2.0f + panelWidth + (width - panelWidth - U(54.0f)) * 0.5f;
    }

    const float formWidth = ImMin(U(392.0f), width - U(64.0f));
    const float formHeight = ImMin(height - U(64.0f), U(640.0f));
    const float formTop = min.y + ImMax(U(28.0f), (height - formHeight) * 0.5f);

    ImGui::SetCursorScreenPos(ImVec2(formCenterX - formWidth * 0.5f, formTop));
    ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0, 0, 0, 0));
    ImGui::BeginChild("##signin_form", ImVec2(formWidth, formHeight), ImGuiChildFlags_None,
                      ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);

    // Anchor the form's contents near its vertical middle.
    const float contentHeight = U(468.0f) + (app.auth.AccountCount() > 0 ? U(90.0f) : 0.0f);
    ui::VSpace(ImMax(0.0f, (formHeight - contentHeight) * 0.45f));

    ui::Overline("ASTRA STUDIO", Colors().brand);
    ui::VSpace(8.0f);
    ui::Text(Text().h1, Colors().text, "Welcome back");
    ui::VSpace(4.0f);
    ui::Text(Text().small, Colors().textDim, "Sign in to open your studio and projects.");
    ui::VSpace(20.0f);

    DrawSavedAccounts(app, formWidth);

    bool submit = false;

    ui::FieldLabel("Username or email");
    if (ui::TextField("##identity", app.signIn.identity, "you@studio.com", ui::Icon::User, &submit,
                      app.signIn.error.find("username") != std::string::npos)) {
        app.signIn.error.clear();
    }
    ui::VSpace(12.0f);

    ui::FieldLabel("Password");
    if (ui::PasswordField("##password", app.signIn.password, app.signIn.reveal, "Enter your password",
                          &submit)) {
        app.signIn.error.clear();
    }

    ui::VSpace(10.0f);
    bool remember = app.signIn.remember;
    if (ui::Checkbox("Keep me signed in on this device", &remember)) {
        app.signIn.remember = remember;
    }

    if (!app.signIn.error.empty()) {
        ui::VSpace(10.0f);
        ErrorBanner(app.signIn.error, app.signIn.hint);
        if (app.signIn.attempts >= 3) {
            ui::VSpace(6.0f);
            ui::TextFaint("Trouble signing in? Your access code can register a new account.");
        }
    }

    ui::VSpace(16.0f);
    const bool canSubmit = !app.signIn.identity.empty() && !app.signIn.password.empty();
    if (ui::PrimaryButton("Sign in", ImVec2(formWidth, U(46.0f)), canSubmit, ui::Icon::ArrowRight) ||
        (submit && canSubmit)) {
        app.SubmitSignIn();
    }

    ui::VSpace(14.0f);
    ui::Divider(0.8f, 2.0f, 12.0f);
    ui::Text(Text().tiny, Colors().textFaint, "NEW TO ASTRA?");
    ui::VSpace(8.0f);
    if (ui::SecondaryButton("Register with an access code", ImVec2(formWidth, U(42.0f)), true,
                            ui::Icon::Key)) {
        app.reg.Reset();
        app.Goto(Screen::RegisterCode);
    }

    ui::VSpace(10.0f);
    float linkWidth = ImGui::CalcTextSize("Explore the demo workspace").x;
    ImGui::SetCursorPosX(ImGui::GetCursorPosX() + (formWidth - linkWidth) * 0.5f - U(14.0f));
    if (ui::LinkButton("Explore the demo workspace", ui::Icon::Sparkle, Colors().textFaint)) {
        app.guestMode = true;
        app.Goto(Screen::ProjectHub);
        app.StartRepoSync(true);
    }

    ui::VSpace(16.0f);
    ui::Divider(0.6f, 2.0f, 10.0f);
    ImGui::PushStyleColor(ImGuiCol_Text, ui::ToVec4(ui::Fade(Colors().textFaint, 0.85f)));
    ui::PushFont(Text().tiny);
    ImGui::Text("%s %s  ·  %s", kEngineName, kEngineVersion, kBuildChannel);
    ui::PopFont();
    ImGui::PopStyleColor();

    ImGui::EndChild();
    ImGui::PopStyleColor();
}

}  // namespace astra::app::screens
