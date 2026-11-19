// app/screens/RegisterScreens.cpp - access-code gate and account creation.
#include <cmath>

#include "astra/app/Screens.h"
#include "astra/app/screens/Common.h"
#include "astra/core/Strings.h"
#include "astra/core/Time.h"
#include "astra/core/Version.h"
#include "astra/ui/Widgets.h"
#include "imgui.h"

namespace astra::app::screens {
namespace {

using ui::Colors;
using ui::Text;
using ui::U;

void DrawStrengthMeter(const std::string& password, license::Role role) {
    const auto strength = auth::EvaluatePassword(password, role);
    ImDrawList* dl = ImGui::GetWindowDrawList();
    const ImVec2 pos = ImGui::GetCursorScreenPos();
    const float width = ImGui::GetContentRegionAvail().x;
    const float barsWidth = ImMin(width * 0.34f, U(150.0f));
    const float barWidth = (barsWidth - U(18.0f)) / 4.0f;

    static const ImU32 kColors[] = {IM_COL32(242, 88, 91, 255), IM_COL32(240, 150, 94, 255),
                                    IM_COL32(240, 180, 94, 255), IM_COL32(95, 207, 158, 255),
                                    IM_COL32(95, 207, 158, 255)};

    for (int i = 0; i < 4; ++i) {
        const ImVec2 barMin(pos.x + (barWidth + U(6.0f)) * static_cast<float>(i), pos.y);
        const ImVec2 barMax(barMin.x + barWidth, barMin.y + U(4.0f));
        const bool filled = i < strength.score;
        dl->AddRectFilled(barMin, barMax,
                          filled ? kColors[ImClamp(strength.score, 0, 4)] : Colors().surfaceHover,
                          U(2.0f));
    }
    dl->AddText(Text().tiny, Text().tiny->FontSize, ImVec2(pos.x + barsWidth + U(10.0f), pos.y - U(4.0f)),
                strength.score >= 3 ? Colors().success : Colors().textFaint,
                (strength.label + " - " + strength.hint).c_str());
    ImGui::Dummy(ImVec2(width, U(16.0f)));
}

void BackLink(AstraApp& app, Screen target) {
    if (ui::LinkButton("Back", ui::Icon::ChevronLeft, Colors().textDim)) {
        app.Goto(target);
    }
}

}  // namespace

void RegisterCode(AstraApp& app) {
    ImGuiWindow* window = ImGui::GetCurrentWindow();
    ImDrawList* dl = window->DrawList;
    const ImVec2 min = window->Pos;
    const ImVec2 max = ImVec2(min.x + window->Size.x, min.y + window->Size.y);
    const float width = window->Size.x;
    const float height = window->Size.y;

    ui::Backdrop(dl, min, max, app.time, true);

    const float cardWidth = ImMin(U(520.0f), width - U(64.0f));
    const float cardHeight = ImMin(U(560.0f), height - U(48.0f));
    const float top = min.y + ImMax(U(20.0f), (height - cardHeight) * 0.5f);

    ImGui::SetCursorScreenPos(ImVec2(min.x + (width - cardWidth) * 0.5f, top));
    ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0, 0, 0, 0));
    ImGui::BeginChild("##register_code", ImVec2(cardWidth, cardHeight), ImGuiChildFlags_None,
                      ImGuiWindowFlags_NoScrollbar);

    ui::VSpace(ImMax(0.0f, (cardHeight - U(470.0f)) * 0.35f));

    BackLink(app, Screen::SignIn);
    ui::VSpace(10.0f);
    StepIndicator(1, ImVec2(cardWidth, U(34.0f)));
    ui::VSpace(6.0f);

    ui::Overline("REGISTRATION  ·  STEP 1 OF 3", Colors().brand);
    ui::VSpace(6.0f);
    ui::Text(Text().h1, Colors().text, "Enter your access code");
    ui::VSpace(6.0f);
    ui::TextWrapped(
        "Astra access codes unlock one engine account each. Codes are single-use and "
        "issued by your studio owner.",
        Colors().textDim, cardWidth);
    ui::VSpace(20.0f);

    ui::FieldLabel("ACCESS CODE", "16 characters");
    if (ui::CodeEntry("##code", app.reg.code, !app.reg.error.empty(), cardWidth)) {
        app.reg.error.clear();
        app.reg.hint.clear();
    }

    ui::VSpace(10.0f);
    if (!app.reg.error.empty()) {
        ErrorBanner(app.reg.error, app.reg.hint);
    } else {
        InfoBanner("Codes look like K7QM-3XPD-9RTA-B2VH. Dashes are added for you.",
                       ui::Icon::Key, Colors().textFaint);
    }

    ui::VSpace(18.0f);
    const bool complete = license::IsCompleteCode(app.reg.code);
    if (ui::PrimaryButton("Verify access code", ImVec2(cardWidth, U(46.0f)), complete,
                          ui::Icon::ArrowRight)) {
        app.SubmitCode();
    }
    if (complete && ImGui::IsKeyPressed(ImGuiKey_Enter, false)) {
        app.SubmitCode();
    }
    ui::VSpace(10.0f);
    if (ui::GhostButton("Cancel registration", ImVec2(cardWidth, U(38.0f)))) {
        app.reg.Reset();
        app.Goto(Screen::SignIn);
    }

    ui::VSpace(18.0f);
    ui::Divider(0.8f, 2.0f, 14.0f);

    // Owner hint block.
    ImDrawList* drawList = ImGui::GetWindowDrawList();
    const ImVec2 hintPos = ImGui::GetCursorScreenPos();
    const float hintHeight = U(74.0f);
    drawList->AddRectFilled(hintPos, ImVec2(hintPos.x + cardWidth, hintPos.y + hintHeight),
                            Colors().surfaceSunken, U(12.0f));
    drawList->AddRect(hintPos, ImVec2(hintPos.x + cardWidth, hintPos.y + hintHeight),
                      Colors().stroke, U(12.0f), 0, U(1.0f));
    const ImVec2 iconCenter(hintPos.x + U(28.0f), hintPos.y + U(30.0f));
    drawList->AddCircleFilled(iconCenter, U(15.0f), ui::Fade(Colors().brand, 0.14f), 20);
    ui::DrawIcon(drawList, ui::Icon::Key, iconCenter, U(15.0f), Colors().brand, U(1.6f));
    drawList->AddText(Text().small, Text().small->FontSize, ImVec2(hintPos.x + U(52.0f), hintPos.y + U(16.0f)),
                      Colors().text, "Where do I get a code?");
    drawList->AddText(Text().tiny, Text().tiny->FontSize,
                      ImVec2(hintPos.x + U(52.0f), hintPos.y + U(34.0f)), Colors().textDim,
                      "Your studio owner opens Astra KeyForge and generates one under");
    drawList->AddText(Text().tiny, Text().tiny->FontSize,
                      ImVec2(hintPos.x + U(52.0f), hintPos.y + U(50.0f)),
                      ui::Fade(Colors().brand, 0.9f), "Access codes  >  Generate code.");
    ImGui::Dummy(ImVec2(cardWidth, hintHeight));

    ImGui::EndChild();
    ImGui::PopStyleColor();
}

void RegisterAccount(AstraApp& app) {
    ImGuiWindow* window = ImGui::GetCurrentWindow();
    ImDrawList* dl = window->DrawList;
    const ImVec2 min = window->Pos;
    const ImVec2 max = ImVec2(min.x + window->Size.x, min.y + window->Size.y);
    const float width = window->Size.x;
    const float height = window->Size.y;

    ui::Backdrop(dl, min, max, app.time, true);

    const float cardWidth = ImMin(U(560.0f), width - U(64.0f));
    const float cardHeight = height - U(20.0f);
    const float top = min.y + U(10.0f);

    ImGui::SetCursorScreenPos(ImVec2(min.x + (width - cardWidth) * 0.5f, top));
    ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0, 0, 0, 0));
    ImGui::BeginChild("##register_account", ImVec2(cardWidth, cardHeight), ImGuiChildFlags_None,
                      ImGuiWindowFlags_None);

    const license::AccessCode& code = app.reg.codeResult.code;
    const license::Role role = code.payload.role;

    ui::VSpace(2.0f);
    BackLink(app, Screen::RegisterCode);
    ui::VSpace(6.0f);
    StepIndicator(2, ImVec2(cardWidth, U(32.0f)));
    ui::VSpace(4.0f);

    ui::Overline("STEP 2 OF 3  ·  CREATE ACCOUNT", Colors().brand);
    ui::VSpace(4.0f);
    ui::Text(Text().h2, Colors().text, "Create your account");
    ui::VSpace(4.0f);
    ui::TextWrapped(
        role == license::Role::Staff
            ? "Staff accounts unlock build tools, multi-scene editing and the weather debugger."
            : "Your account is stored in the local Astra registry on this device.",
        Colors().textDim, cardWidth);
    ui::VSpace(12.0f);

    // Code summary.
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(U(8.0f), U(8.0f)));
    SummaryChip("ACCESS CODE", license::MaskCode(code.text).c_str(), Colors().textDim);
    ImGui::SameLine(0.0f, U(8.0f));
    SummaryChip("ROLE", license::RoleName(role), role == license::Role::Staff ? Colors().brand
                                                                             : Colors().info);
    ImGui::SameLine(0.0f, U(8.0f));
    const std::string expiry = code.neverExpires
                                   ? "Never"
                                   : time::FormatUtc(code.expiryUtc).substr(0, 10);
    SummaryChip("VALID UNTIL", expiry.c_str(), Colors().success);
    ImGui::PopStyleVar();
    ui::VSpace(12.0f);

    if (!app.reg.error.empty()) {
        ErrorBanner(app.reg.error, app.reg.hint);
        ui::VSpace(12.0f);
    }

    ui::FieldLabel("Display name");
    ui::TextField("##display", app.reg.displayName, "Ada Lovelace", ui::Icon::User);
    ui::VSpace(9.0f);

    ui::FieldLabel("Username", "3-24 characters");
    ui::TextField("##username", app.reg.username, "ada.lovelace", ui::Icon::Tag);
    ui::VSpace(9.0f);

    ui::FieldLabel("Email", "optional");
    ui::TextField("##email", app.reg.email, "ada@astrapictures.com", ui::Icon::Mail);
    ui::VSpace(9.0f);

    ui::FieldLabel("Password", role == license::Role::Staff ? "12+ characters" : "10+ characters");
    ui::PasswordField("##password", app.reg.password, app.reg.revealPassword, "Choose a strong password");
    ui::VSpace(6.0f);
    DrawStrengthMeter(app.reg.password, role);
    ui::VSpace(2.0f);

    ui::FieldLabel("Confirm password");
    ui::PasswordField("##confirm", app.reg.confirmPassword, app.reg.revealPassword,
                      "Repeat your password");

    ui::VSpace(10.0f);
    bool terms = app.reg.acceptedTerms;
    if (ui::Checkbox("I accept the Astra Productions studio terms",
                     &terms, "Access is tied to this device's registry. Codes cannot be reused.")) {
        app.reg.acceptedTerms = terms;
    }

    ui::VSpace(16.0f);
    const bool ready = !app.reg.displayName.empty() && !app.reg.username.empty() &&
                       !app.reg.password.empty() && !app.reg.confirmPassword.empty() &&
                       app.reg.acceptedTerms;
    if (ui::PrimaryButton("Create account", ImVec2(cardWidth, U(46.0f)), ready, ui::Icon::Check)) {
        app.SubmitRegistration();
    }
    ui::VSpace(8.0f);
    if (ui::GhostButton("Cancel", ImVec2(cardWidth, U(36.0f)))) {
        app.reg.Reset();
        app.Goto(Screen::SignIn);
    }
    ui::VSpace(14.0f);

    ImGui::EndChild();
    ImGui::PopStyleColor();
}

}  // namespace astra::app::screens
