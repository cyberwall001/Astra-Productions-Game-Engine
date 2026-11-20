// ui/Theme.h - Astra's visual language: palette, fonts and ImGui style.
//
// The palette is derived from the Astra Productions logo: a coral crescent over a
// near-black field. Coral is reserved for intent (primary actions, active state,
// brand accents); everything else lives on a neutral graphite ramp.
#pragma once

#include <string>

#include "imgui.h"
#include "imgui_internal.h"  // ImClamp/ImMin/ImRect used across the UI layer

namespace astra::ui {

struct Palette {
    ImU32 brand = IM_COL32(255, 95, 90, 255);          // #FF5F5A coral
    ImU32 brandDeep = IM_COL32(214, 62, 58, 255);      // pressed / gradient end
    ImU32 brandBright = IM_COL32(255, 122, 108, 255);  // hover
    ImU32 brandSoft = IM_COL32(255, 95, 90, 34);       // tinted fills
    ImU32 brandGlow = IM_COL32(255, 95, 90, 90);

    ImU32 bg = IM_COL32(9, 9, 11, 255);              // #09090B window
    ImU32 bgDeep = IM_COL32(6, 6, 8, 255);
    ImU32 surface = IM_COL32(18, 18, 22, 255);       // cards
    ImU32 surfaceRaised = IM_COL32(23, 23, 28, 255); // popovers, inputs
    ImU32 surfaceHover = IM_COL32(29, 29, 35, 255);
    ImU32 surfaceSunken = IM_COL32(13, 13, 16, 255); // wells, viewport

    ImU32 stroke = IM_COL32(38, 38, 46, 255);
    ImU32 strokeStrong = IM_COL32(54, 54, 64, 255);
    ImU32 strokeSoft = IM_COL32(255, 255, 255, 16);

    ImU32 text = IM_COL32(242, 242, 245, 255);
    ImU32 textDim = IM_COL32(160, 160, 172, 255);
    ImU32 textFaint = IM_COL32(106, 106, 120, 255);
    ImU32 textOnBrand = IM_COL32(18, 10, 10, 255);

    ImU32 success = IM_COL32(95, 207, 158, 255);
    ImU32 warn = IM_COL32(240, 180, 94, 255);
    ImU32 danger = IM_COL32(242, 88, 91, 255);
    ImU32 info = IM_COL32(111, 168, 220, 255);
    ImU32 violet = IM_COL32(169, 139, 224, 255);

    // Editor chrome
    ImU32 editorBar = IM_COL32(14, 14, 17, 255);
    ImU32 editorPanel = IM_COL32(16, 16, 20, 255);
    ImU32 editorHeader = IM_COL32(21, 21, 26, 255);
    ImU32 viewport = IM_COL32(10, 10, 13, 255);

    // GitHub language dots.
    ImU32 LanguageColor(const std::string& language) const;
};

struct Fonts {
    ImFont* tiny = nullptr;     // 11.5px - status bar, meta
    ImFont* small = nullptr;    // 13px - labels, secondary
    ImFont* body = nullptr;     // 15px - default UI text
    ImFont* medium = nullptr;   // 16.5px - emphasised rows, buttons
    ImFont* h3 = nullptr;       // 19px - panel titles
    ImFont* h2 = nullptr;       // 24px - card/page headings
    ImFont* h1 = nullptr;       // 32px - screen titles
    ImFont* display = nullptr;  // 54px - boot wordmark
    ImFont* mono = nullptr;     // 15px - codes, paths, console
    ImFont* monoSmall = nullptr;// 13px - console, ids
};

struct ThemeState {
    Palette colors;
    Fonts fonts;
    float uiScale = 1.0f;
    bool loaded = false;
};

ThemeState& State();
const Palette& Colors();
const Fonts& Text();

// Loads the bundled fonts and applies ImGuiStyle. `scale` multiplies every size.
// Returns false when the fonts could not be found (falls back to ImGui's default).
bool LoadFonts(float scale);
void ApplyStyle();

// Colour helpers.
inline ImU32 WithAlpha(ImU32 color, int alpha) {
    return (color & 0x00FFFFFFu) | (static_cast<ImU32>(alpha & 0xFF) << 24);
}
inline ImU32 LerpColor(ImU32 a, ImU32 b, float t) {
    const int r = static_cast<int>(((a >> 0) & 0xFF) + (((b >> 0) & 0xFF) - ((a >> 0) & 0xFF)) * t);
    const int g = static_cast<int>(((a >> 8) & 0xFF) + (((b >> 8) & 0xFF) - ((a >> 8) & 0xFF)) * t);
    const int bl = static_cast<int>(((a >> 16) & 0xFF) + (((b >> 16) & 0xFF) - ((a >> 16) & 0xFF)) * t);
    const int al = static_cast<int>(((a >> 24) & 0xFF) + (((b >> 24) & 0xFF) - ((a >> 24) & 0xFF)) * t);
    return IM_COL32(r, g, bl, al);
}
inline ImVec4 ToVec4(ImU32 color) { return ImGui::ColorConvertU32ToFloat4(color); }
inline ImU32 FromVec4(const ImVec4& c) { return ImGui::ColorConvertFloat4ToU32(c); }

// Applies the palette to ImGui's stock widgets (used inside docked editor windows so
// stock ImGui widgets match the custom-drawn launcher).
void PushEditorStyle();
void PopEditorStyle();

}  // namespace astra::ui
