#include "astra/ui/Theme.h"

#include <fstream>

#include "astra/core/Log.h"
#include "astra/core/Paths.h"

namespace astra::ui {
namespace {

ThemeState gState;

ImVec4 FromU32(ImU32 color) { return ImGui::ColorConvertU32ToFloat4(color); }

std::string FontPath(const char* name) { return paths::Asset(std::string("fonts/") + name); }

// Latin-1 plus the punctuation the studio actually uses. Without this the default
// range drops em dashes and quotes, which ImGui then renders as '?'.
const ImWchar* StudioGlyphRanges() {
    static ImVector<ImWchar> ranges;
    if (ranges.empty()) {
        ImFontGlyphRangesBuilder builder;
        builder.AddRanges(ImGui::GetIO().Fonts->GetGlyphRangesDefault());
        builder.AddText("\xE2\x80\x93\xE2\x80\x94"                 // en dash, em dash
                        "\xE2\x80\x98\xE2\x80\x99\xE2\x80\x9C\xE2\x80\x9D"  // single/double quotes
                        "\xE2\x80\xA2\xE2\x80\xA6"                 // bullet, ellipsis
                        "\xE2\x86\x90\xE2\x86\x91\xE2\x86\x92\xE2\x86\x93"  // arrows
                        "\xC3\x97\xE2\x9C\x93\xE2\x9C\x95");      // multiply, check, cross
        builder.BuildRanges(&ranges);
    }
    return ranges.Data;
}

ImFont* AddFont(ImFontAtlas* atlas, const std::string& path, float px) {
    ImFontConfig config;
    config.OversampleH = 2;
    config.OversampleV = 4;
    config.PixelSnapH = false;
    config.RasterizerDensity = 1.0f;
    config.GlyphRanges = StudioGlyphRanges();
    return atlas->AddFontFromFileTTF(path.c_str(), px, &config);
}

}  // namespace

ThemeState& State() { return gState; }
const Palette& Colors() { return gState.colors; }
const Fonts& Text() { return gState.fonts; }

ImU32 Palette::LanguageColor(const std::string& language) const {
    if (language == "C++") return IM_COL32(243, 75, 125, 255);
    if (language == "C") return IM_COL32(85, 85, 85, 255);
    if (language == "Rust") return IM_COL32(222, 165, 132, 255);
    if (language == "Python") return IM_COL32(53, 114, 165, 255);
    if (language == "TypeScript") return IM_COL32(49, 120, 198, 255);
    if (language == "C#") return IM_COL32(23, 134, 0, 255);
    if (language == "Lua") return IM_COL32(0, 0, 128, 200);
    if (language == "GLSL") return IM_COL32(120, 160, 220, 255);
    if (language.empty()) return textFaint;
    return textDim;
}

bool LoadFonts(float scale) {
    ImGuiIO& io = ImGui::GetIO();
    ImFontAtlas* atlas = io.Fonts;
    if (!atlas) return false;

    const std::string body = FontPath("Roboto-Medium.ttf");
    const std::string mono = FontPath("Cousine-Regular.ttf");
    if (!std::ifstream(body.c_str()).good()) {
        log::Warn("ui", "font file not found: " + body);
        return false;
    }

    atlas->Clear();
    // Sizes are tuned for a 15px body at 100% scale.
    gState.fonts.tiny = AddFont(atlas, body, 11.5f * scale);
    gState.fonts.small = AddFont(atlas, body, 13.0f * scale);
    gState.fonts.body = AddFont(atlas, body, 15.0f * scale);
    gState.fonts.medium = AddFont(atlas, body, 16.5f * scale);
    gState.fonts.h3 = AddFont(atlas, body, 19.0f * scale);
    gState.fonts.h2 = AddFont(atlas, body, 24.0f * scale);
    gState.fonts.h1 = AddFont(atlas, body, 32.0f * scale);
    gState.fonts.display = AddFont(atlas, body, 54.0f * scale);
    gState.fonts.mono = AddFont(atlas, mono, 15.0f * scale);
    gState.fonts.monoSmall = AddFont(atlas, mono, 13.0f * scale);

    io.FontDefault = gState.fonts.body;
    gState.uiScale = scale;
    gState.loaded = true;
    return true;
}

void ApplyStyle() {
    ImGuiStyle& style = ImGui::GetStyle();
    ImGui::StyleColorsDark(&style);

    const Palette& c = gState.colors;
    style.WindowPadding = ImVec2(0, 0);
    style.FramePadding = ImVec2(12, 9);
    style.CellPadding = ImVec2(8, 6);
    style.ItemSpacing = ImVec2(12, 10);
    style.ItemInnerSpacing = ImVec2(8, 6);
    style.TouchExtraPadding = ImVec2(0, 0);
    style.IndentSpacing = 18.0f;
    style.ScrollbarSize = 11.0f;
    style.GrabMinSize = 10.0f;

    style.WindowBorderSize = 0.0f;
    style.ChildBorderSize = 1.0f;
    style.PopupBorderSize = 1.0f;
    style.FrameBorderSize = 0.0f;
    style.TabBorderSize = 0.0f;

    style.WindowRounding = 0.0f;
    style.ChildRounding = 12.0f;
    style.FrameRounding = 8.0f;
    style.PopupRounding = 10.0f;
    style.ScrollbarRounding = 8.0f;
    style.GrabRounding = 6.0f;
    style.TabRounding = 6.0f;

    style.WindowTitleAlign = ImVec2(0.0f, 0.5f);
    style.WindowMenuButtonPosition = ImGuiDir_None;
    style.ColorButtonPosition = ImGuiDir_Right;
    style.ButtonTextAlign = ImVec2(0.5f, 0.5f);
    style.SelectableTextAlign = ImVec2(0.0f, 0.5f);
    style.SeparatorTextBorderSize = 1.0f;
    style.SeparatorTextPadding = ImVec2(16, 10);
    style.SeparatorTextAlign = ImVec2(0.0f, 0.5f);

    ImVec4* colors = style.Colors;
    colors[ImGuiCol_Text] = ToVec4(c.text);
    colors[ImGuiCol_TextDisabled] = ToVec4(c.textFaint);
    colors[ImGuiCol_WindowBg] = ToVec4(c.bg);
    colors[ImGuiCol_ChildBg] = ImVec4(0, 0, 0, 0);
    colors[ImGuiCol_PopupBg] = ToVec4(c.surfaceRaised);
    colors[ImGuiCol_Border] = ToVec4(c.stroke);
    colors[ImGuiCol_BorderShadow] = ImVec4(0, 0, 0, 0);
    colors[ImGuiCol_FrameBg] = ToVec4(c.surfaceRaised);
    colors[ImGuiCol_FrameBgHovered] = ToVec4(c.surfaceHover);
    colors[ImGuiCol_FrameBgActive] = ToVec4(c.surfaceHover);
    colors[ImGuiCol_TitleBg] = ToVec4(c.bgDeep);
    colors[ImGuiCol_TitleBgActive] = ToVec4(c.editorBar);
    colors[ImGuiCol_TitleBgCollapsed] = ToVec4(c.bgDeep);
    colors[ImGuiCol_MenuBarBg] = ToVec4(c.editorBar);
    colors[ImGuiCol_ScrollbarBg] = ImVec4(0, 0, 0, 0);
    colors[ImGuiCol_ScrollbarGrab] = FromU32(IM_COL32(255, 255, 255, 26));
    colors[ImGuiCol_ScrollbarGrabHovered] = FromU32(IM_COL32(255, 255, 255, 46));
    colors[ImGuiCol_ScrollbarGrabActive] = FromU32(WithAlpha(c.brand, 150));
    colors[ImGuiCol_CheckMark] = ToVec4(c.brand);
    colors[ImGuiCol_SliderGrab] = ToVec4(c.brand);
    colors[ImGuiCol_SliderGrabActive] = ToVec4(c.brandBright);
    colors[ImGuiCol_Button] = ToVec4(c.surfaceRaised);
    colors[ImGuiCol_ButtonHovered] = ToVec4(c.surfaceHover);
    colors[ImGuiCol_ButtonActive] = FromU32(WithAlpha(c.brand, 60));
    colors[ImGuiCol_Header] = FromU32(WithAlpha(c.brand, 28));
    colors[ImGuiCol_HeaderHovered] = FromU32(WithAlpha(c.brand, 40));
    colors[ImGuiCol_HeaderActive] = FromU32(WithAlpha(c.brand, 60));
    colors[ImGuiCol_Separator] = ToVec4(c.stroke);
    colors[ImGuiCol_SeparatorHovered] = FromU32(WithAlpha(c.brand, 120));
    colors[ImGuiCol_SeparatorActive] = ToVec4(c.brand);
    colors[ImGuiCol_ResizeGrip] = FromU32(IM_COL32(255, 255, 255, 12));
    colors[ImGuiCol_ResizeGripHovered] = FromU32(WithAlpha(c.brand, 60));
    colors[ImGuiCol_ResizeGripActive] = FromU32(WithAlpha(c.brand, 120));
    colors[ImGuiCol_Tab] = ToVec4(c.editorHeader);
    colors[ImGuiCol_TabHovered] = FromU32(WithAlpha(c.brand, 40));
    colors[ImGuiCol_TabSelected] = ToVec4(c.editorPanel);
    colors[ImGuiCol_TabSelectedOverline] = ToVec4(c.brand);
    colors[ImGuiCol_TabDimmed] = ToVec4(c.editorBar);
    colors[ImGuiCol_TabDimmedSelected] = ToVec4(c.editorPanel);
    colors[ImGuiCol_TabDimmedSelectedOverline] = FromU32(WithAlpha(c.brand, 90));
    colors[ImGuiCol_DockingPreview] = FromU32(WithAlpha(c.brand, 70));
    colors[ImGuiCol_DockingEmptyBg] = ToVec4(c.viewport);
    colors[ImGuiCol_PlotLines] = ToVec4(c.brand);
    colors[ImGuiCol_PlotLinesHovered] = ToVec4(c.brandBright);
    colors[ImGuiCol_PlotHistogram] = ToVec4(c.brand);
    colors[ImGuiCol_PlotHistogramHovered] = ToVec4(c.brandBright);
    colors[ImGuiCol_TableHeaderBg] = ToVec4(c.editorHeader);
    colors[ImGuiCol_TableBorderStrong] = ToVec4(c.stroke);
    colors[ImGuiCol_TableBorderLight] = ToVec4(c.strokeSoft);
    colors[ImGuiCol_TableRowBg] = ImVec4(0, 0, 0, 0);
    colors[ImGuiCol_TableRowBgAlt] = FromU32(IM_COL32(255, 255, 255, 5));
    colors[ImGuiCol_TextLink] = ToVec4(c.brand);
    colors[ImGuiCol_TextSelectedBg] = FromU32(WithAlpha(c.brand, 70));
    colors[ImGuiCol_DragDropTarget] = ToVec4(c.brand);
    colors[ImGuiCol_NavCursor] = ToVec4(c.brand);
    colors[ImGuiCol_NavWindowingHighlight] = FromU32(WithAlpha(c.brand, 180));
    colors[ImGuiCol_NavWindowingDimBg] = FromU32(IM_COL32(0, 0, 0, 140));
    colors[ImGuiCol_ModalWindowDimBg] = FromU32(IM_COL32(4, 4, 6, 200));
}

void PushEditorStyle() {
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(10, 10));
    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(9, 6));
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(9, 7));
    ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, ImVec2(6, 6));
    ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, ImVec2(8, 8));
}

void PopEditorStyle() { ImGui::PopStyleVar(5); }

}  // namespace astra::ui
