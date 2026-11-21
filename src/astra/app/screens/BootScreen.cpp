// app/screens/BootScreen.cpp - the Astra loading screen and boot sequence.
#include <cmath>

#include "astra/app/Screens.h"
#include "astra/app/screens/Common.h"
#include "astra/core/Strings.h"
#include "astra/core/Version.h"
#include "astra/ui/Branding.h"
#include "astra/ui/Widgets.h"
#include "imgui.h"

namespace astra::app {
namespace {

using screens::SpacedText;
using screens::SpacedWidth;
using screens::Wordmark;

const std::vector<boot::Stage> kStages = {
    {"Verifying engine integrity", "astra-runtime.sig", 0.00f},
    {"Loading cryptographic modules", "sha256, hmac, pbkdf2", 0.42f},
    {"Opening the account registry", "accounts.json", 0.95f},
    {"Preparing the studio workspace", "scenes, assets", 1.50f},
    {"Checking GitHub access", "api.github.com", 2.00f},
};

const std::vector<std::string> kTerminalLines = {
    "astra-boot 0.1.0 (cumulonimbus) starting",
    "[ ok ] secure storage ............ dpapi",
    "[ ok ] crypto self-test .......... hmac-sha256",
    "[ ok ] account registry .......... 0 corrupt records",
    "[ ok ] scene runtime ............. world loaded, 13 actors",
    "[ ok ] level viewport ............ raster pipeline ready",
    "[ ok ] github transport .......... winhttp",
    "[ ok ] renderer .................. offline (milestone 1)",
    "[ ok ] studio shell ready in 412 ms",
};

constexpr float kLoadingDuration = 2.35f;
constexpr float kTerminalDuration = 1.25f;

void CenteredText(ImDrawList* dl, ImFont* font, ImU32 color, float centerX, float top,
                  const char* text) {
    if (!font) return;
    const float width = font->CalcTextSizeA(font->FontSize, FLT_MAX, 0.0f, text).x;
    dl->AddText(font, font->FontSize, ImVec2(centerX - width * 0.5f, top), color, text);
}

void DrawLoading(AstraApp& app, ImDrawList* dl, ImVec2 min, ImVec2 max) {
    const float t = app.bootTimer / kLoadingDuration;
    const float centerX = (min.x + max.x) * 0.5f;
    const float centerY = (min.y + max.y) * 0.5f;

    const float logoSize = ui::U(168.0f);
    const float pulse = 0.55f + 0.45f * std::sin(app.time * 1.6f);

    // Logo with a slow breathing glow.
    ui::RadialGlow(dl, ImVec2(centerX, centerY - ui::U(58.0f)), ui::U(420.0f), ui::Colors().brand,
                   0.55f + pulse * 0.35f);
    const ui::branding::Image* logo = ui::branding::Logo();
    const ImVec2 logoMin(centerX - logoSize * 0.5f, centerY - ui::U(180.0f));
    if (logo && logo->texture != 0) {
        dl->AddImage(logo->texture, logoMin,
                     ImVec2(logoMin.x + logoSize, logoMin.y + logoSize));
    } else {
        ui::BrandMark(dl, ImVec2(centerX, logoMin.y + logoSize * 0.5f), logoSize * 0.42f,
                      ui::Colors().brand, ui::Colors().brandDeep, true, 0.8f);
    }

    // Wordmark.
    const float markY = logoMin.y + logoSize + ui::U(6.0f);
    ImGui::SetCursorScreenPos(ImVec2(centerX - ui::U(120.0f), markY));
    Wordmark(ui::Colors().text, ui::Colors().textFaint, true, ui::U(240.0f));

    const float versionY = markY + ui::U(40.0f);
    CenteredText(dl, ui::Text().tiny, ui::Colors().textFaint, centerX, versionY,
                 "ASTRA ENGINE  •  0.1.0");
    CenteredText(dl, ui::Text().tiny, ui::Fade(ui::Colors().textFaint, 0.75f), centerX,
                 versionY + ui::U(18.0f), "CUMULONIMBUS  •  PREVIEW BUILD");

    // Progress.
    const float barWidth = ui::U(340.0f);
    const float barY = centerY + ui::U(150.0f);
    const ImVec2 barMin(centerX - barWidth * 0.5f, barY);
    const float barHeight = ui::U(4.0f);
    dl->AddRectFilled(barMin, ImVec2(barMin.x + barWidth, barMin.y + barHeight),
                      ui::Colors().surfaceRaised, barHeight * 0.5f);
    const float clamped = ImClamp(t, 0.0f, 1.0f);
    if (clamped > 0.0f) {
        dl->AddRectFilled(barMin, ImVec2(barMin.x + barWidth * clamped, barMin.y + barHeight),
                          ui::Colors().brand, barHeight * 0.5f);
        dl->AddCircleFilled(ImVec2(barMin.x + barWidth * clamped, barMin.y + barHeight * 0.5f),
                            ui::U(3.5f), ui::Colors().brandBright, 12);
    }

    const int stageIndex = ImClamp(app.bootStage, 0, static_cast<int>(kStages.size()) - 1);
    const boot::Stage& stage = kStages[static_cast<std::size_t>(stageIndex)];
    const float labelY = barY + ui::U(14.0f);
    dl->AddText(ui::Text().tiny, ui::Text().tiny->FontSize, ImVec2(barMin.x, labelY),
                ui::Colors().textDim, stage.label);
    const std::string percent = str::Format("%d%%", static_cast<int>(clamped * 100.0f));
    const float percentWidth = ui::Text().monoSmall->CalcTextSizeA(
                                   ui::Text().monoSmall->FontSize, FLT_MAX, 0.0f, percent.c_str()).x;
    dl->AddText(ui::Text().monoSmall, ui::Text().monoSmall->FontSize,
                ImVec2(barMin.x + barWidth - percentWidth, labelY - ui::U(1.0f)),
                ui::Colors().brand, percent.c_str());

    // Footer.
    CenteredText(dl, ui::Text().tiny, ui::Fade(ui::Colors().textFaint, 0.6f), centerX,
                 max.y - ui::U(34.0f), "ASTRA PRODUCTIONS  ·  FRISCO, TEXAS");
}

void DrawTerminal(AstraApp& app, ImDrawList* dl, ImVec2 min, ImVec2 max) {
    const float t = (app.bootTimer - kLoadingDuration) / kTerminalDuration;
    const float centerX = (min.x + max.x) * 0.5f;

    const float logoSize = ui::U(46.0f);
    const float top = min.y + ui::U(64.0f);
    const ui::branding::Image* logo = ui::branding::Logo();
    if (logo && logo->texture != 0) {
        dl->AddImage(logo->texture, ImVec2(centerX - logoSize * 0.5f, top),
                     ImVec2(centerX + logoSize * 0.5f, top + logoSize));
    } else {
        ui::BrandMark(dl, ImVec2(centerX, top + logoSize * 0.5f), logoSize * 0.45f,
                      ui::Colors().brand, ui::Colors().brandDeep, true, 0.4f);
    }

    SpacedText(dl, ui::Text().tiny, ui::Colors().textDim,
                        ImVec2(centerX - SpacedWidth(ui::Text().tiny, "BOOT SEQUENCE", ui::U(4.0f)) * 0.5f,
                               top + logoSize + ui::U(12.0f)),
                        "BOOT SEQUENCE", ui::U(4.0f));

    // Terminal card.
    const float cardWidth = ui::U(620.0f);
    const float cardHeight = ui::U(268.0f);
    const ImVec2 cardMin(centerX - cardWidth * 0.5f, top + logoSize + ui::U(40.0f));
    const ImVec2 cardMax(cardMin.x + cardWidth, cardMin.y + cardHeight);

    dl->AddRectFilled(cardMin, cardMax, ui::Colors().surfaceSunken, ui::U(14.0f));
    dl->AddRect(cardMin, cardMax, ui::Colors().stroke, ui::U(14.0f), 0, ui::U(1.0f));

    // Title strip with traffic dots.
    dl->AddRectFilled(cardMin, ImVec2(cardMax.x, cardMin.y + ui::U(30.0f)),
                      ui::Colors().editorHeader, ui::U(14.0f), ImDrawFlags_RoundCornersTop);
    for (int i = 0; i < 3; ++i) {
        dl->AddCircleFilled(ImVec2(cardMin.x + ui::U(16.0f) + static_cast<float>(i) * ui::U(12.0f),
                                   cardMin.y + ui::U(15.0f)),
                            ui::U(3.0f),
                            ui::Fade(i == 0 ? ui::Colors().brand : ui::Colors().textFaint, 0.55f), 10);
    }
    dl->AddText(ui::Text().monoSmall, ui::Text().monoSmall->FontSize,
                ImVec2(cardMin.x + ui::U(60.0f), cardMin.y + ui::U(8.0f)), ui::Colors().textFaint,
                "astra-studio --boot");

    const int visible = ImClamp(static_cast<int>(t * (static_cast<float>(kTerminalLines.size()) + 1.0f)),
                                0, static_cast<int>(kTerminalLines.size()));
    float lineY = cardMin.y + ui::U(44.0f);
    for (int i = 0; i < visible; ++i) {
        const std::string& line = kTerminalLines[static_cast<std::size_t>(i)];
        const bool isOk = str::Contains(line, "[ ok ]");
        const bool isHeader = i == 0;
        ImU32 color = ui::Colors().textDim;
        if (isHeader) color = ui::Colors().text;
        else if (isOk) color = ui::Fade(ui::Colors().success, 0.9f);
        dl->AddText(ui::Text().monoSmall, ui::Text().monoSmall->FontSize,
                    ImVec2(cardMin.x + ui::U(20.0f), lineY), color, line.c_str());
        lineY += ImGui::GetTextLineHeightWithSpacing() * 0.92f;
    }

    // Blinking caret.
    if (std::fmod(app.time, 1.0f) < 0.55f) {
        dl->AddRectFilled(ImVec2(cardMin.x + ui::U(20.0f), lineY),
                          ImVec2(cardMin.x + ui::U(26.0f), lineY + ui::U(11.0f)),
                          ui::Colors().brand);
    }

    CenteredText(dl, ui::Text().tiny, ui::Fade(ui::Colors().textFaint, 0.75f), centerX,
                          cardMax.y + ui::U(20.0f), "Press any key to skip");
}

}  // namespace

namespace boot {

const std::vector<Stage>& Stages() { return kStages; }
const std::vector<std::string>& TerminalLines() { return kTerminalLines; }
float LoadingDuration() { return kLoadingDuration; }
float TotalDuration() { return kLoadingDuration + kTerminalDuration; }

}  // namespace boot

namespace screens {

void Boot(AstraApp& app) {
    ImGuiWindow* window = ImGui::GetCurrentWindow();
    ImDrawList* dl = window->DrawList;
    const ImVec2 min = window->Pos;
    const ImVec2 max = ImVec2(window->Pos.x + window->Size.x, window->Pos.y + window->Size.y);

    const bool wasLoading = app.bootTimer <= boot::LoadingDuration();
    ui::Backdrop(dl, min, max, app.time, !wasLoading);

    if (wasLoading) {
        DrawLoading(app, dl, min, max);
    } else {
        DrawTerminal(app, dl, min, max);
    }

    // Skipping is only wired for a real keyboard press, not a stray click.
    if (!ImGui::GetIO().AppAcceptingEvents) return;
    for (int key = ImGuiKey_NamedKey_BEGIN; key < ImGuiKey_NamedKey_END; ++key) {
        if (ImGui::IsKeyPressed(static_cast<ImGuiKey>(key), false)) {
            app.Goto(Screen::SignIn, false);
            break;
        }
    }
}

}  // namespace screens

}  // namespace astra::app
