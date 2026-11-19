#include "astra/app/screens/Common.h"

#include "astra/core/Strings.h"
#include "astra/ui/Widgets.h"
#include "imgui.h"

namespace astra::app::screens {

using ui::Colors;
using ui::Text;
using ui::U;

void SpacedText(ImDrawList* dl, ImFont* font, ImU32 color, ImVec2 pos, const char* text,
                float spacing) {
    if (!font) {
        dl->AddText(pos, color, text);
        return;
    }
    float x = pos.x;
    for (const char* p = text; *p; ++p) {
        const char glyph[2] = {*p, '\0'};
        dl->AddText(font, font->FontSize, ImVec2(x, pos.y), color, glyph);
        x += font->CalcTextSizeA(font->FontSize, FLT_MAX, 0.0f, glyph, nullptr).x + spacing;
    }
}

float SpacedWidth(ImFont* font, const char* text, float spacing) {
    if (!font) return ImGui::CalcTextSize(text).x;
    float width = 0.0f;
    for (const char* p = text; *p; ++p) {
        const char glyph[2] = {*p, '\0'};
        width += font->CalcTextSizeA(font->FontSize, FLT_MAX, 0.0f, glyph, nullptr).x + spacing;
    }
    return width > 0.0f ? width - spacing : 0.0f;
}

void Wordmark(ImU32 mainColor, ImU32 subColor, bool centered, float width) {
    ImDrawList* dl = ImGui::GetWindowDrawList();
    const ImVec2 pos = ImGui::GetCursorScreenPos();
    const char* main = "ASTRA";
    const char* sub = "PRODUCTIONS";
    const float spacingMain = U(7.0f);
    const float spacingSub = U(5.4f);

    const float mainWidth = SpacedWidth(Text().h2, main, spacingMain);
    const float subWidth = SpacedWidth(Text().tiny, sub, spacingSub);
    const float total = mainWidth + U(10.0f) + subWidth;

    float x = centered ? pos.x + (width - total) * 0.5f : pos.x;
    const float baseline = pos.y + ImGui::GetTextLineHeight() * 0.1f;
    SpacedText(dl, Text().h2, mainColor, ImVec2(x, baseline), main, spacingMain);
    x += mainWidth + U(10.0f);
    SpacedText(dl, Text().tiny, subColor,
               ImVec2(x, baseline + (Text().h2 ? (Text().h2->FontSize - Text().tiny->FontSize) * 0.72f : 0.0f)),
               sub, spacingSub);

    ImGui::Dummy(ImVec2(centered ? width : total, Text().h2 ? Text().h2->FontSize : 24.0f));
}

void StepIndicator(int current, ImVec2 size) {
    static const char* kLabels[] = {"Access code", "Account", "Ready"};
    const int count = 3;
    ImDrawList* dl = ImGui::GetWindowDrawList();
    const ImVec2 pos = ImGui::GetCursorScreenPos();

    const float stepGap = U(10.0f);
    const float lineWidth = (size.x - stepGap * static_cast<float>(count - 1)) /
                            static_cast<float>(count);

    for (int i = 0; i < count; ++i) {
        const float x0 = pos.x + (lineWidth + stepGap) * static_cast<float>(i);
        const float x1 = x0 + lineWidth;
        const bool done = (i + 1) < current;
        const bool active = (i + 1) == current;
        const ImU32 color = done ? Colors().success
                                 : (active ? Colors().brand : Colors().stroke);

        dl->AddLine(ImVec2(x0, pos.y + U(6.0f)), ImVec2(x1, pos.y + U(6.0f)), color, U(2.0f));

        const ImU32 labelColor = active ? Colors().text : (done ? Colors().textDim : Colors().textFaint);
        dl->AddText(Text().tiny, Text().tiny->FontSize, ImVec2(x0, pos.y + U(14.0f)), labelColor,
                    kLabels[i]);
    }
    ImGui::Dummy(ImVec2(size.x, U(34.0f)));
}

void ErrorBanner(const std::string& message, const std::string& hint) {
    if (message.empty()) return;
    ImDrawList* dl = ImGui::GetWindowDrawList();
    const float width = ImGui::GetContentRegionAvail().x;
    const float height = hint.empty() ? U(42.0f) : U(58.0f);
    const ImVec2 pos = ImGui::GetCursorScreenPos();

    dl->AddRectFilled(pos, ImVec2(pos.x + width, pos.y + height),
                      ui::Fade(Colors().danger, 0.12f), U(10.0f));
    dl->AddRect(pos, ImVec2(pos.x + width, pos.y + height), ui::Fade(Colors().danger, 0.34f),
                U(10.0f), 0, U(1.0f));
    ui::DrawIcon(dl, ui::Icon::Warning, ImVec2(pos.x + U(20.0f), pos.y + U(21.0f)), U(15.0f),
                 Colors().danger);

    dl->AddText(Text().small, Text().small->FontSize, ImVec2(pos.x + U(38.0f), pos.y + U(12.0f)),
                Colors().danger, message.c_str());
    if (!hint.empty()) {
        dl->AddText(Text().tiny, Text().tiny->FontSize, ImVec2(pos.x + U(38.0f), pos.y + U(33.0f)),
                    ui::Fade(Colors().textDim, 1.0f), hint.c_str());
    }
    ImGui::Dummy(ImVec2(width, height));
}

void SuccessBanner(const std::string& message, const std::string& hint) {
    if (message.empty()) return;
    ImDrawList* dl = ImGui::GetWindowDrawList();
    const float width = ImGui::GetContentRegionAvail().x;
    const float height = hint.empty() ? U(42.0f) : U(58.0f);
    const ImVec2 pos = ImGui::GetCursorScreenPos();

    dl->AddRectFilled(pos, ImVec2(pos.x + width, pos.y + height),
                      ui::Fade(Colors().success, 0.10f), U(10.0f));
    dl->AddRect(pos, ImVec2(pos.x + width, pos.y + height), ui::Fade(Colors().success, 0.30f),
                U(10.0f), 0, U(1.0f));
    ui::DrawIcon(dl, ui::Icon::Check, ImVec2(pos.x + U(20.0f), pos.y + U(21.0f)), U(15.0f),
                 Colors().success);
    dl->AddText(Text().small, Text().small->FontSize, ImVec2(pos.x + U(38.0f), pos.y + U(12.0f)),
                Colors().success, message.c_str());
    if (!hint.empty()) {
        dl->AddText(Text().tiny, Text().tiny->FontSize, ImVec2(pos.x + U(38.0f), pos.y + U(33.0f)),
                    Colors().textDim, hint.c_str());
    }
    ImGui::Dummy(ImVec2(width, height));
}

void InfoBanner(const std::string& message, ui::Icon icon, ImU32 color) {
    if (message.empty()) return;
    const ImU32 tint = color == 0 ? Colors().info : color;
    ImDrawList* dl = ImGui::GetWindowDrawList();
    const float width = ImGui::GetContentRegionAvail().x;
    const float height = U(40.0f);
    const ImVec2 pos = ImGui::GetCursorScreenPos();
    dl->AddRectFilled(pos, ImVec2(pos.x + width, pos.y + height), ui::Fade(tint, 0.08f), U(10.0f));
    ui::DrawIcon(dl, icon, ImVec2(pos.x + U(20.0f), pos.y + U(20.0f)), U(14.0f), tint);
    dl->AddText(Text().tiny, Text().tiny->FontSize, ImVec2(pos.x + U(36.0f), pos.y + U(13.0f)),
                tint, message.c_str());
    ImGui::Dummy(ImVec2(width, height));
}

void SummaryChip(const char* label, const char* value, ImU32 accent) {
    ImDrawList* dl = ImGui::GetWindowDrawList();
    const ImVec2 pos = ImGui::GetCursorScreenPos();
    const ImVec2 labelSize = ImGui::CalcTextSize(label);
    const ImVec2 valueSize = ImGui::CalcTextSize(value);
    const float width = ImMax(labelSize.x, valueSize.x) + U(24.0f);
    const float height = U(50.0f);

    dl->AddRectFilled(pos, ImVec2(pos.x + width, pos.y + height), Colors().surfaceSunken, U(10.0f));
    dl->AddRect(pos, ImVec2(pos.x + width, pos.y + height), ui::Fade(accent, 0.28f), U(10.0f), 0,
                U(1.0f));
    dl->AddText(Text().tiny, Text().tiny->FontSize, ImVec2(pos.x + U(12.0f), pos.y + U(10.0f)),
                Colors().textFaint, label);
    dl->AddText(Text().small, Text().small->FontSize, ImVec2(pos.x + U(12.0f), pos.y + U(25.0f)),
                accent, value);
    ImGui::Dummy(ImVec2(width, height));
}

float CenterVertically(float regionTop, float regionHeight, float contentHeight,
                       float minTopPadding) {
    const float available = regionHeight - minTopPadding * 2.0f;
    if (contentHeight >= available) return regionTop + minTopPadding;
    return regionTop + minTopPadding + (available - contentHeight) * 0.5f;
}

void BeginScreenBody(const char* id) {
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
    ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0, 0, 0, 0));
    ImGui::BeginChild(id, ImVec2(0, 0), ImGuiChildFlags_None,
                      ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoMove);
}

void EndScreenBody() {
    ImGui::EndChild();
    ImGui::PopStyleColor();
    ImGui::PopStyleVar();
}

}  // namespace astra::app::screens
