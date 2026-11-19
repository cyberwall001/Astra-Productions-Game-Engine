// ui/Widgets.h - Astra's hand-drawn widget kit.
//
// Everything here draws itself into the ImGui draw list (rather than relying on
// stock widgets) so the launcher and the editor share one visual language.
#pragma once

#include <string>

#include "astra/ui/Icons.h"
#include "astra/ui/Theme.h"
#include "imgui.h"

namespace astra::ui {

// ---- metrics -------------------------------------------------------------
float Scale();          // UI scale multiplier (1.0 = 100%)
float U(float pixels);  // scales a design-space measurement

// ---- text ----------------------------------------------------------------
void PushFont(ImFont* font);
void PopFont();

void Text(ImFont* font, ImU32 color, const char* fmt, ...);
void TextDefault(const char* fmt, ...);
void TextDim(const char* fmt, ...);
void TextFaint(const char* fmt, ...);
void TextBrand(const char* fmt, ...);
void TextWrapped(const char* text, ImU32 color, float wrapWidth = 0.0f);
void Overline(const char* text, ImU32 color = 0, bool withRule = false);
void Divider(float alpha = 1.0f, float topGap = 8.0f, float bottomGap = 8.0f);
void VSpace(float pixels);
void HSpace(float pixels);

// ---- surfaces ------------------------------------------------------------
void BeginCard(const char* id, ImVec2 size, ImU32 background, float rounding = -1.0f,
               bool border = true);
void EndCard();
void CardHeader(const char* title, const char* subtitle = nullptr, Icon icon = Icon::None,
                ImU32 accent = 0);

// ---- buttons -------------------------------------------------------------
bool PrimaryButton(const char* label, ImVec2 size = ImVec2(0, 0), bool enabled = true,
                   Icon icon = Icon::None);
bool SecondaryButton(const char* label, ImVec2 size = ImVec2(0, 0), bool enabled = true,
                     Icon icon = Icon::None);
bool GhostButton(const char* label, ImVec2 size = ImVec2(0, 0), bool enabled = true,
                 Icon icon = Icon::None, ImU32 textColor = 0);
bool DangerButton(const char* label, ImVec2 size = ImVec2(0, 0), bool enabled = true,
                  Icon icon = Icon::None);
bool LinkButton(const char* label, Icon icon = Icon::None, ImU32 color = 0);
bool IconButton(const char* id, Icon icon, const char* tooltip, bool active = false,
                float size = 30.0f, bool enabled = true);

// ---- inputs --------------------------------------------------------------
void FieldLabel(const char* label, const char* aside = nullptr);
bool TextField(const char* id, std::string& value, const char* placeholder,
               Icon icon = Icon::None, bool* submitted = nullptr, bool error = false,
               float width = 0.0f, ImGuiInputTextFlags flags = 0);
bool PasswordField(const char* id, std::string& value, bool& reveal, const char* placeholder,
                   bool* submitted = nullptr, bool error = false, float width = 0.0f);
bool SearchField(const char* id, std::string& value, float width = 0.0f,
                 const char* placeholder = "Search...");
bool Checkbox(const char* label, bool* value, const char* description = nullptr);
bool Toggle(const char* label, bool* value, const char* description = nullptr);
bool Segmented(const char* id, int* index, const char* const* labels, int count,
               float width = 0.0f);
bool Select(const char* id, int* index, const char* const* labels, int count, float width = 0.0f);

// Compact [-] value [+] control for small integer ranges (1..10 style pickers).
bool Stepper(const char* id, int* value, int minValue, int maxValue, float width = 0.0f);
bool CodeEntry(const char* id, std::string& value, bool error, float width = 0.0f);

// ---- display -------------------------------------------------------------
void Badge(const char* text, ImU32 color, bool solid = false);
void Pill(const char* text, Icon icon, ImU32 color);
void Avatar(const char* initials, ImU32 accent, float diameter = 36.0f, bool ring = false);
void ProgressBar(float fraction, ImVec2 size, ImU32 color = 0, bool indeterminate = false,
                 float t = 0.0f);
void Spinner(ImVec2 center, float radius, float time, ImU32 color = 0, float thickness = 2.0f);
void StatTile(const char* label, const char* value, Icon icon, ImU32 accent, float width = 0.0f);
void Sparkline(const float* values, int count, ImVec2 size, ImU32 color = 0, bool fill = true);
void Ring(float fraction, float diameter, const char* centerLabel, ImU32 color = 0);
void KeyValue(const char* key, const char* value, ImU32 valueColor = 0);
void SeparatorText(const char* text);
void SkeletonBar(ImVec2 size, float time, float rounding = 6.0f);
void EmptyState(Icon icon, const char* title, const char* body, const char* action = nullptr,
                bool* actionClicked = nullptr);

// ---- brand / decoration --------------------------------------------------
// Crescent + monogram: the Astra mark, drawn procedurally so it stays crisp.
void BrandMark(ImDrawList* drawList, ImVec2 center, float radius, ImU32 color,
               ImU32 innerColor, bool withMonogram, float glowStrength = 0.0f);
// Draws the packaged PNG logo when available, falling back to BrandMark.
void LogoImage(ImVec2 size, float glowStrength = 0.35f);
void RadialGlow(ImDrawList* drawList, ImVec2 center, float radius, ImU32 color, float strength);
void Backdrop(ImDrawList* drawList, ImVec2 min, ImVec2 max, float time, bool withGrid = true);
void Grid(ImDrawList* drawList, ImVec2 min, ImVec2 max, float cell, ImU32 color);
void Tooltip(const char* text);

// ---- animation -----------------------------------------------------------
float EaseOutCubic(float t);
float EaseInOutCubic(float t);
float EaseOutQuint(float t);
ImU32 Fade(ImU32 color, float alpha);  // alpha 0..1

}  // namespace astra::ui
