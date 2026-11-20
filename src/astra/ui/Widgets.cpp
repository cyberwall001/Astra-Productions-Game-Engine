#include "astra/ui/Widgets.h"

#include <cmath>
#include <cstdarg>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

#include "astra/licensing/AccessCode.h"
#include "astra/ui/Branding.h"
#include "imgui_internal.h"

namespace astra::ui {
namespace {

constexpr float kPi = 3.14159265358979f;

struct FontScope {
    explicit FontScope(ImFont* font) : pushed(font != nullptr) {
        if (pushed) ImGui::PushFont(font);
    }
    ~FontScope() {
        if (pushed) ImGui::PopFont();
    }
    bool pushed;
};

float& Anim(ImGuiID id, float initial = 0.0f) {
    return *ImGui::GetStateStorage()->GetFloatRef(id, initial);
}

// Shared button renderer.
bool DrawButton(const char* id, const char* label, ImVec2 size, ImU32 base, ImU32 hover,
                ImU32 pressed, ImU32 textColor, bool bordered, bool enabled, Icon icon,
                bool glow, bool flat) {
    ImGuiWindow* window = ImGui::GetCurrentWindow();
    if (window->SkipItems) return false;

    ImGuiIO& io = ImGui::GetIO();
    const float height = size.y > 0.0f ? size.y : U(42.0f);
    const float width = size.x > 0.0f ? size.x : ImGui::GetContentRegionAvail().x;
    const ImVec2 pos = ImGui::GetCursorScreenPos();
    const ImRect bb(pos, ImVec2(pos.x + width, pos.y + height));
    const ImGuiID wid = window->GetID(id);

    ImGui::ItemSize(bb, 0.0f);
    if (!ImGui::ItemAdd(bb, wid)) return false;

    bool hovered = false;
    bool held = false;
    bool clicked = false;
    if (enabled) {
        clicked = ImGui::ButtonBehavior(bb, wid, &hovered, &held);
    } else if (ImGui::IsMouseHoveringRect(bb.Min, bb.Max) && ImGui::IsWindowHovered()) {
        ImGui::SetMouseCursor(ImGuiMouseCursor_NotAllowed);
    }

    float& hoverT = Anim(wid ^ 0x51, 0.0f);
    hoverT = ImLerp(hoverT, (hovered || held) ? 1.0f : 0.0f, ImMin(1.0f, io.DeltaTime * 16.0f));

    ImDrawList* dl = window->DrawList;
    const float rounding = U(10.0f);
    ImU32 bg = LerpColor(base, pressed, held ? 1.0f : 0.0f);
    bg = LerpColor(bg, hover, hoverT);
    if (!enabled) bg = Fade(bg, 0.55f);

    if (glow && enabled && hoverT > 0.01f) {
        dl->AddRect(ImVec2(bb.Min.x - U(1), bb.Min.y - U(1)),
                    ImVec2(bb.Max.x + U(1), bb.Max.y + U(1)), Fade(Colors().brandGlow, hoverT * 0.55f),
                    rounding + U(1), 0, U(3.0f));
    }

    if (!flat || hoverT > 0.01f) {
        dl->AddRectFilled(bb.Min, bb.Max, bg, rounding);
    }
    if (bordered) {
        dl->AddRect(bb.Min, bb.Max, Fade(Colors().strokeStrong, enabled ? 1.0f : 0.5f), rounding, 0,
                    U(1.0f));
    }
    // Top highlight gives buttons a subtle sheen without a real gradient.
    if (!flat) {
        dl->AddRectFilled(bb.Min, ImVec2(bb.Max.x, bb.Min.y + bb.GetHeight() * 0.5f),
                          Fade(IM_COL32_WHITE, 0.05f), rounding, ImDrawFlags_RoundCornersTop);
    }

    ImFont* font = Text().medium;
    FontScope scope(font);
    const float iconSize = U(16.0f);
    const ImVec2 textSize = ImGui::CalcTextSize(label);
    const float gap = icon == Icon::None ? 0.0f : U(8.0f);
    const float totalWidth = textSize.x + (icon == Icon::None ? 0.0f : iconSize + gap);
    float startX = bb.Min.x + (bb.GetWidth() - totalWidth) * 0.5f;
    const float textY = bb.Min.y + (bb.GetHeight() - textSize.y) * 0.5f;

    ImU32 fg = enabled ? textColor : Fade(textColor, 0.5f);
    if (icon != Icon::None) {
        DrawIcon(dl, icon, ImVec2(startX + iconSize * 0.5f, bb.Min.y + bb.GetHeight() * 0.5f),
                 iconSize, fg, U(1.7f));
        startX += iconSize + gap;
    }
    dl->AddText(ImVec2(startX, textY), fg, label);
    return clicked && enabled;
}

int InputTextCallback(ImGuiInputTextCallbackData* data) {
    if (data->EventFlag == ImGuiInputTextFlags_CallbackResize) {
        auto* str = static_cast<std::string*>(data->UserData);
        IM_ASSERT(data->Buf == str->c_str());
        str->resize(static_cast<std::size_t>(data->BufTextLen));
        data->Buf = const_cast<char*>(str->c_str());
    }
    return 0;
}

bool InputTextStd(const char* id, std::string& value, ImGuiInputTextFlags flags) {
    if (value.capacity() < value.size() + 32) value.reserve(value.size() + 32);
    flags |= ImGuiInputTextFlags_CallbackResize;
    return ImGui::InputText(id, const_cast<char*>(value.c_str()), value.capacity() + 1, flags,
                            InputTextCallback, &value);
}

// Draws the frame every text field shares.
ImRect BeginFieldFrame(const char* id, float width, bool error, float heightPx, bool& focused,
                       float leftPadding, float rightPadding) {
    ImGuiWindow* window = ImGui::GetCurrentWindow();
    const float height = U(heightPx);
    const ImVec2 pos = ImGui::GetCursorScreenPos();
    const float w = width > 0.0f ? width : ImGui::GetContentRegionAvail().x;
    const ImRect bb(pos, ImVec2(pos.x + w, pos.y + height));
    const ImGuiID wid = window->GetID(id);

    float& focusT = Anim(wid ^ 0x99, 0.0f);
    focusT = ImLerp(focusT, focused ? 1.0f : 0.0f, ImMin(1.0f, ImGui::GetIO().DeltaTime * 18.0f));

    ImDrawList* dl = window->DrawList;
    const float rounding = U(10.0f);
    dl->AddRectFilled(bb.Min, bb.Max, Colors().surfaceRaised, rounding);
    ImU32 border = error ? Colors().danger : LerpColor(Colors().stroke, Colors().brand, focusT);
    dl->AddRect(bb.Min, bb.Max, border, rounding, 0, U(1.0f) + focusT * U(0.6f));
    if (focusT > 0.01f) {
        dl->AddRect(ImVec2(bb.Min.x - U(1), bb.Min.y - U(1)),
                    ImVec2(bb.Max.x + U(1), bb.Max.y + U(1)), Fade(Colors().brandGlow, focusT * 0.35f),
                    rounding + U(1), 0, U(2.5f));
    }
    (void)leftPadding;
    (void)rightPadding;
    return bb;
}

}  // namespace

float Scale() { return State().uiScale > 0.0f ? State().uiScale : 1.0f; }
float U(float pixels) { return pixels * Scale(); }

void PushFont(ImFont* font) {
    // Always push so PushFont/PopFont pairs stay balanced even when a size is missing.
    ImGui::PushFont(font ? font : ImGui::GetFont());
}
void PopFont() { ImGui::PopFont(); }

void Text(ImFont* font, ImU32 color, const char* fmt, ...) {
    char buffer[2048];
    va_list args;
    va_start(args, fmt);
    std::vsnprintf(buffer, sizeof(buffer), fmt, args);
    va_end(args);
    FontScope scope(font);
    ImGui::PushStyleColor(ImGuiCol_Text, ToVec4(color));
    ImGui::TextUnformatted(buffer);
    ImGui::PopStyleColor();
}

void TextDefault(const char* fmt, ...) {
    char buffer[2048];
    va_list args;
    va_start(args, fmt);
    std::vsnprintf(buffer, sizeof(buffer), fmt, args);
    va_end(args);
    FontScope scope(Text().body);
    ImGui::PushStyleColor(ImGuiCol_Text, ToVec4(Colors().text));
    ImGui::TextUnformatted(buffer);
    ImGui::PopStyleColor();
}

void TextDim(const char* fmt, ...) {
    char buffer[2048];
    va_list args;
    va_start(args, fmt);
    std::vsnprintf(buffer, sizeof(buffer), fmt, args);
    va_end(args);
    FontScope scope(Text().small);
    ImGui::PushStyleColor(ImGuiCol_Text, ToVec4(Colors().textDim));
    ImGui::TextUnformatted(buffer);
    ImGui::PopStyleColor();
}

void TextFaint(const char* fmt, ...) {
    char buffer[2048];
    va_list args;
    va_start(args, fmt);
    std::vsnprintf(buffer, sizeof(buffer), fmt, args);
    va_end(args);
    FontScope scope(Text().small);
    ImGui::PushStyleColor(ImGuiCol_Text, ToVec4(Colors().textFaint));
    ImGui::TextUnformatted(buffer);
    ImGui::PopStyleColor();
}

void TextBrand(const char* fmt, ...) {
    char buffer[2048];
    va_list args;
    va_start(args, fmt);
    std::vsnprintf(buffer, sizeof(buffer), fmt, args);
    va_end(args);
    FontScope scope(Text().small);
    ImGui::PushStyleColor(ImGuiCol_Text, ToVec4(Colors().brand));
    ImGui::TextUnformatted(buffer);
    ImGui::PopStyleColor();
}

void TextWrapped(const char* text, ImU32 color, float wrapWidth) {
    FontScope scope(Text().small);
    ImGui::PushStyleColor(ImGuiCol_Text, ToVec4(color));
    if (wrapWidth > 0.0f) ImGui::PushTextWrapPos(ImGui::GetCursorPosX() + wrapWidth);
    ImGui::TextWrapped("%s", text);
    if (wrapWidth > 0.0f) ImGui::PopTextWrapPos();
    ImGui::PopStyleColor();
}

void Overline(const char* text, ImU32 color, bool withRule) {
    const ImU32 tint = color == 0 ? Colors().textFaint : color;
    FontScope scope(Text().tiny);
    ImGui::PushStyleColor(ImGuiCol_Text, ToVec4(tint));
    const ImVec2 pos = ImGui::GetCursorScreenPos();
    ImGui::TextUnformatted(text);
    const ImVec2 after = ImGui::GetItemRectSize();
    if (withRule) {
        const float lineY = pos.y + after.y * 0.5f;
        ImGui::GetWindowDrawList()->AddLine(ImVec2(pos.x + after.x + U(10), lineY),
                                            ImVec2(pos.x + ImGui::GetContentRegionAvail().x, lineY),
                                            Fade(Colors().stroke, 1.0f), U(1.0f));
    }
    ImGui::PopStyleColor();
}

void Divider(float alpha, float topGap, float bottomGap) {
    ImGui::Dummy(ImVec2(0, U(topGap)));
    const ImVec2 pos = ImGui::GetCursorScreenPos();
    const float width = ImGui::GetContentRegionAvail().x;
    ImGui::GetWindowDrawList()->AddLine(pos, ImVec2(pos.x + width, pos.y),
                                       Fade(Colors().stroke, alpha), U(1.0f));
    ImGui::Dummy(ImVec2(0, U(bottomGap)));
}

void VSpace(float pixels) { ImGui::Dummy(ImVec2(0, U(pixels))); }

void HSpace(float pixels) { ImGui::SameLine(0.0f, U(pixels)); }

void BeginCard(const char* id, ImVec2 size, ImU32 background, float rounding, bool border) {
    ImGuiWindow* window = ImGui::GetCurrentWindow();
    const ImVec2 pos = ImGui::GetCursorScreenPos();
    const float w = size.x > 0.0f ? size.x : ImGui::GetContentRegionAvail().x;
    const float r = rounding < 0.0f ? U(14.0f) : rounding;

    ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, r);
    ImGui::PushStyleVar(ImGuiStyleVar_ChildBorderSize, border ? 1.0f : 0.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(U(14.0f), U(12.0f)));
    ImGui::PushStyleColor(ImGuiCol_ChildBg, ToVec4(background));
    ImGui::PushStyleColor(ImGuiCol_Border, ToVec4(Colors().stroke));
    ImGui::PushID(id);
    ImGui::BeginChild(id, ImVec2(w, size.y), ImGuiChildFlags_Borders,
                      ImGuiWindowFlags_NoScrollbar | ImGuiChildFlags_AlwaysUseWindowPadding);
    (void)pos;
    (void)window;
}

void EndCard() {
    ImGui::EndChild();
    ImGui::PopID();
    ImGui::PopStyleColor(2);
    ImGui::PopStyleVar(3);
}

void CardHeader(const char* title, const char* subtitle, Icon icon, ImU32 accent) {
    const ImU32 tint = accent == 0 ? Colors().brand : accent;
    if (icon != Icon::None) {
        const ImVec2 pos = ImGui::GetCursorScreenPos();
        const float box = U(30.0f);
        ImGui::GetWindowDrawList()->AddRectFilled(pos, ImVec2(pos.x + box, pos.y + box),
                                                 Fade(tint, 0.14f), U(9.0f));
        DrawIcon(ImGui::GetWindowDrawList(), icon,
                 ImVec2(pos.x + box * 0.5f, pos.y + box * 0.5f), U(16.0f), tint, U(1.7f));
        ImGui::Dummy(ImVec2(box, box));
        ImGui::SameLine(0.0f, U(12.0f));
    }
    ImGui::BeginGroup();
    Text(Text().h3, Colors().text, "%s", title);
    if (subtitle && *subtitle) {
        Text(Text().small, Colors().textDim, "%s", subtitle);
    }
    ImGui::EndGroup();
    VSpace(6.0f);
}

bool PrimaryButton(const char* label, ImVec2 size, bool enabled, Icon icon) {
    return DrawButton(label, label, size, Colors().brand, Colors().brandBright,
                      Colors().brandDeep, Colors().textOnBrand, false, enabled, icon, true, false);
}

bool SecondaryButton(const char* label, ImVec2 size, bool enabled, Icon icon) {
    return DrawButton(label, label, size, Colors().surfaceRaised, Colors().surfaceHover,
                      Colors().surfaceSunken, Colors().text, true, enabled, icon, false, false);
}

bool GhostButton(const char* label, ImVec2 size, bool enabled, Icon icon, ImU32 textColor) {
    const ImU32 fg = textColor == 0 ? Colors().textDim : textColor;
    return DrawButton(label, label, size, IM_COL32(255, 255, 255, 0), Colors().surfaceHover,
                      Colors().surfaceRaised, fg, false, enabled, icon, false, true);
}

bool DangerButton(const char* label, ImVec2 size, bool enabled, Icon icon) {
    return DrawButton(label, label, size, Fade(Colors().danger, 0.16f), Fade(Colors().danger, 0.26f),
                      Fade(Colors().danger, 0.32f), Colors().danger, true, enabled, icon, false,
                      false);
}

bool LinkButton(const char* label, Icon icon, ImU32 color) {
    ImGuiWindow* window = ImGui::GetCurrentWindow();
    const ImU32 tint = color == 0 ? Colors().brand : color;
    FontScope scope(Text().small);

    const ImVec2 textSize = ImGui::CalcTextSize(label);
    const float iconSize = icon == Icon::None ? 0.0f : U(14.0f);
    const float gap = icon == Icon::None ? 0.0f : U(6.0f);
    const ImVec2 pos = ImGui::GetCursorScreenPos();
    const ImRect bb(pos, ImVec2(pos.x + textSize.x + iconSize + gap, pos.y + ImMax(textSize.y, iconSize)));
    const ImGuiID id = window->GetID(label);

    ImGui::ItemSize(bb, 0.0f);
    if (!ImGui::ItemAdd(bb, id)) return false;

    bool hovered = false, held = false;
    const bool clicked = ImGui::ButtonBehavior(bb, id, &hovered, &held);
    float& hoverT = Anim(id ^ 0x22, 0.0f);
    hoverT = ImLerp(hoverT, hovered ? 1.0f : 0.0f, ImMin(1.0f, ImGui::GetIO().DeltaTime * 18.0f));

    ImDrawList* dl = window->DrawList;
    ImU32 fg = LerpColor(tint, Colors().brandBright, hoverT * 0.7f);
    float x = bb.Min.x;
    if (icon != Icon::None) {
        DrawIcon(dl, icon, ImVec2(x + iconSize * 0.5f, bb.Min.y + bb.GetHeight() * 0.5f), iconSize,
                 fg, U(1.6f));
        x += iconSize + gap;
    }
    dl->AddText(ImVec2(x, bb.Min.y), fg, label);
    if (hoverT > 0.01f) {
        dl->AddLine(ImVec2(x, bb.Max.y - U(1)), ImVec2(bb.Max.x, bb.Max.y - U(1)),
                    Fade(tint, hoverT * 0.8f), U(1.0f));
    }
    if (hovered) ImGui::SetMouseCursor(ImGuiMouseCursor_Hand);
    return clicked;
}

bool IconButton(const char* id, Icon icon, const char* tooltip, bool active, float size,
                bool enabled) {
    ImGuiWindow* window = ImGui::GetCurrentWindow();
    const float box = U(size);
    const ImVec2 pos = ImGui::GetCursorScreenPos();
    const ImRect bb(pos, ImVec2(pos.x + box, pos.y + box));
    const ImGuiID wid = window->GetID(id);

    ImGui::ItemSize(bb, 0.0f);
    if (!ImGui::ItemAdd(bb, wid)) return false;

    bool hovered = false, held = false;
    bool clicked = false;
    if (enabled) {
        clicked = ImGui::ButtonBehavior(bb, wid, &hovered, &held);
    }

    float& hoverT = Anim(wid ^ 0x33, 0.0f);
    hoverT = ImLerp(hoverT, (hovered || held || active) ? 1.0f : 0.0f,
                    ImMin(1.0f, ImGui::GetIO().DeltaTime * 18.0f));

    ImDrawList* dl = window->DrawList;
    if (hoverT > 0.01f) {
        dl->AddRectFilled(bb.Min, bb.Max,
                          active ? Fade(Colors().brand, 0.16f) : Fade(Colors().surfaceHover, hoverT),
                          U(8.0f));
    }
    ImU32 fg = active ? Colors().brand : LerpColor(Colors().textFaint, Colors().text, hoverT);
    if (!enabled) fg = Fade(Colors().textFaint, 0.5f);
    DrawIcon(dl, icon, ImVec2(bb.Min.x + box * 0.5f, bb.Min.y + box * 0.5f), box * 0.52f, fg,
             U(1.7f));

    if (tooltip && *tooltip && hovered && enabled && ImGui::IsWindowHovered()) {
        Tooltip(tooltip);
    }
    return clicked && enabled;
}

void FieldLabel(const char* label, const char* aside) {
    FontScope scope(Text().small);
    ImGui::PushStyleColor(ImGuiCol_Text, ToVec4(Colors().textDim));
    ImGui::TextUnformatted(label);
    ImGui::PopStyleColor();
    if (aside && *aside) {
        ImGui::SameLine();
        const float width = ImGui::CalcTextSize(aside).x;
        ImGui::SetCursorPosX(ImGui::GetCursorPosX() + ImGui::GetContentRegionAvail().x - width);
        FontScope small(Text().small);
        ImGui::PushStyleColor(ImGuiCol_Text, ToVec4(Colors().textFaint));
        ImGui::TextUnformatted(aside);
        ImGui::PopStyleColor();
    }
    ImGui::Dummy(ImVec2(0, U(4.0f)));
}

bool TextField(const char* id, std::string& value, const char* placeholder, Icon icon,
               bool* submitted, bool error, float width, ImGuiInputTextFlags flags) {
    const float height = 44.0f;
    const float padLeft = icon == Icon::None ? U(14.0f) : U(42.0f);
    const float padRight = U(14.0f);

    ImGuiWindow* window = ImGui::GetCurrentWindow();
    const float w = width > 0.0f ? width : ImGui::GetContentRegionAvail().x;
    const ImVec2 size(w, U(height));
    const ImGuiID wid = window->GetID(id);

    // Reserve the row so layout matches the frame we draw.
    const float focusPrev = *ImGui::GetStateStorage()->GetFloatRef(wid ^ 0x99, 0.0f);
    (void)focusPrev;

    bool focusedPrev = Anim(wid ^ 0xA1, 0.0f) > 0.5f;
    bool focused = focusedPrev;
    const ImRect bb = BeginFieldFrame(id, w, error, height, focused, padLeft, padRight);
    ImGui::ItemSize(size, 0.0f);
    ImGui::ItemAdd(bb, wid);

    if (icon != Icon::None) {
        DrawIcon(window->DrawList, icon, ImVec2(bb.Min.x + U(24.0f), bb.Min.y + size.y * 0.5f),
                 U(16.0f), Colors().textFaint, U(1.6f));
    }

    const float textHeight = ImGui::GetTextLineHeight();
    ImGui::SetCursorScreenPos(ImVec2(bb.Min.x + padLeft, bb.Min.y + (size.y - textHeight) * 0.5f));
    ImGui::PushStyleColor(ImGuiCol_FrameBg, ImVec4(0, 0, 0, 0));
    ImGui::PushStyleColor(ImGuiCol_FrameBgHovered, ImVec4(0, 0, 0, 0));
    ImGui::PushStyleColor(ImGuiCol_FrameBgActive, ImVec4(0, 0, 0, 0));
    ImGui::PushStyleColor(ImGuiCol_Text, ToVec4(Colors().text));
    ImGui::PushStyleColor(ImGuiCol_TextSelectedBg, ToVec4(Fade(Colors().brand, 0.35f)));
    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(0, 0));
    ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, 0.0f);
    ImGui::PushItemWidth(w - padLeft - padRight);
    FontScope scope(Text().body);
    ImGui::PushID(id);
    const bool changed = InputTextStd("##input", value, flags);
    const bool active = ImGui::IsItemActive();
    ImGui::PopID();
    ImGui::PopItemWidth();
    ImGui::PopStyleVar(2);
    ImGui::PopStyleColor(5);

    Anim(wid ^ 0xA1, 0.0f) = active ? 1.0f : 0.0f;

    if (value.empty() && placeholder && !active) {
        // Placeholder sits exactly where the caret would be.
        FontScope placeholderFont(Text().body);
        window->DrawList->AddText(ImVec2(bb.Min.x + padLeft, bb.Min.y + (size.y - textHeight) * 0.5f),
                                  Colors().textFaint, placeholder);
    }
    if (active) ImGui::SetMouseCursor(ImGuiMouseCursor_TextInput);

    bool submit = false;
    if (submitted) {
        submit = active && ImGui::IsKeyPressed(ImGuiKey_Enter, false);
        *submitted = submit;
    }
    return changed || submit;
}

bool PasswordField(const char* id, std::string& value, bool& reveal, const char* placeholder,
                   bool* submitted, bool error, float width) {
    const float height = 44.0f;
    ImGuiWindow* window = ImGui::GetCurrentWindow();
    const float w = width > 0.0f ? width : ImGui::GetContentRegionAvail().x;
    const ImVec2 size(w, U(height));
    const ImGuiID wid = window->GetID(id);
    const float padLeft = U(42.0f);
    const float padRight = U(46.0f);

    bool focused = Anim(wid ^ 0xA1, 0.0f) > 0.5f;
    const ImRect bb = BeginFieldFrame(id, w, error, height, focused, padLeft, padRight);
    ImGui::ItemSize(size, 0.0f);
    ImGui::ItemAdd(bb, wid);

    DrawIcon(window->DrawList, Icon::Lock,
             ImVec2(bb.Min.x + U(24.0f), bb.Min.y + size.y * 0.5f), U(16.0f),
             Colors().textFaint, U(1.6f));

    const float textHeight = ImGui::GetTextLineHeight();
    ImGui::SetCursorScreenPos(ImVec2(bb.Min.x + padLeft, bb.Min.y + (size.y - textHeight) * 0.5f));
    ImGui::PushStyleColor(ImGuiCol_FrameBg, ImVec4(0, 0, 0, 0));
    ImGui::PushStyleColor(ImGuiCol_FrameBgHovered, ImVec4(0, 0, 0, 0));
    ImGui::PushStyleColor(ImGuiCol_FrameBgActive, ImVec4(0, 0, 0, 0));
    ImGui::PushStyleColor(ImGuiCol_Text, ToVec4(Colors().text));
    ImGui::PushStyleColor(ImGuiCol_TextSelectedBg, ToVec4(Fade(Colors().brand, 0.35f)));
    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(0, 0));
    ImGui::PushItemWidth(w - padLeft - padRight);
    FontScope scope(Text().body);
    ImGui::PushID(id);
    ImGuiInputTextFlags flags = reveal ? 0 : ImGuiInputTextFlags_Password;
    const bool changed = InputTextStd("##password", value, flags);
    const bool active = ImGui::IsItemActive();
    ImGui::PopID();
    ImGui::PopItemWidth();
    ImGui::PopStyleVar();
    ImGui::PopStyleColor(5);

    Anim(wid ^ 0xA1, 0.0f) = active ? 1.0f : 0.0f;

    if (value.empty() && placeholder && !active) {
        FontScope placeholderFont(Text().body);
        window->DrawList->AddText(ImVec2(bb.Min.x + padLeft, bb.Min.y + (size.y - textHeight) * 0.5f),
                                  Colors().textFaint, placeholder);
    }

    // Reveal toggle, drawn inside the frame at the right edge.
    ImGui::SetCursorScreenPos(ImVec2(bb.Max.x - U(40.0f), bb.Min.y + (size.y - U(28.0f)) * 0.5f));
    ImGui::PushID(id);
    ImGui::PushID("reveal");
    if (IconButton("##reveal", reveal ? Icon::EyeOff : Icon::Eye,
                   reveal ? "Hide password" : "Show password", false, 28.0f)) {
        reveal = !reveal;
    }
    ImGui::PopID();
    ImGui::PopID();

    bool submit = false;
    if (submitted) {
        submit = active && ImGui::IsKeyPressed(ImGuiKey_Enter, false);
        *submitted = submit;
    }
    return changed || submit;
}

bool SearchField(const char* id, std::string& value, float width, const char* placeholder) {
    const float height = 36.0f;
    ImGuiWindow* window = ImGui::GetCurrentWindow();
    const float w = width > 0.0f ? width : ImGui::GetContentRegionAvail().x;
    const ImVec2 size(w, U(height));
    const ImGuiID wid = window->GetID(id);
    const float padLeft = U(36.0f);

    bool focused = Anim(wid ^ 0xA1, 0.0f) > 0.5f;
    const ImRect bb = BeginFieldFrame(id, w, false, height, focused, padLeft, U(10.0f));
    ImGui::ItemSize(size, 0.0f);
    ImGui::ItemAdd(bb, wid);

    DrawIcon(window->DrawList, Icon::Search,
             ImVec2(bb.Min.x + U(20.0f), bb.Min.y + size.y * 0.5f), U(14.0f),
             Colors().textFaint, U(1.6f));

    const float textHeight = ImGui::CalcTextSize("Ag").y;
    ImGui::SetCursorScreenPos(ImVec2(bb.Min.x + padLeft, bb.Min.y + (size.y - ImGui::GetTextLineHeight()) * 0.5f));
    ImGui::PushStyleColor(ImGuiCol_FrameBg, ImVec4(0, 0, 0, 0));
    ImGui::PushStyleColor(ImGuiCol_FrameBgHovered, ImVec4(0, 0, 0, 0));
    ImGui::PushStyleColor(ImGuiCol_FrameBgActive, ImVec4(0, 0, 0, 0));
    ImGui::PushStyleColor(ImGuiCol_Text, ToVec4(Colors().text));
    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(0, 0));
    ImGui::PushItemWidth(w - padLeft - U(10.0f));
    FontScope scope(Text().small);
    ImGui::PushID(id);
    const bool changed = InputTextStd("##search", value, 0);
    const bool active = ImGui::IsItemActive();
    ImGui::PopID();
    ImGui::PopItemWidth();
    ImGui::PopStyleVar();
    ImGui::PopStyleColor(4);
    (void)textHeight;

    Anim(wid ^ 0xA1, 0.0f) = active ? 1.0f : 0.0f;

    if (value.empty() && !active) {
        FontScope placeholderFont(Text().small);
        window->DrawList->AddText(
            ImVec2(bb.Min.x + padLeft, bb.Min.y + (size.y - ImGui::GetTextLineHeight()) * 0.5f),
            Colors().textFaint, placeholder);
    }
    return changed;
}

bool Checkbox(const char* label, bool* value, const char* description) {
    ImGuiWindow* window = ImGui::GetCurrentWindow();
    const float box = U(20.0f);
    const float lineHeight = ImMax(ImGui::GetTextLineHeight(), box);
    FontScope scope(Text().body);
    const ImVec2 labelSize = ImGui::CalcTextSize(label);
    const ImVec2 pos = ImGui::GetCursorScreenPos();
    const float width = ImGui::GetContentRegionAvail().x;
    const ImRect bb(pos, ImVec2(pos.x + width, pos.y + lineHeight));
    const ImGuiID id = window->GetID(label);

    ImGui::ItemSize(bb, 0.0f);
    if (!ImGui::ItemAdd(bb, id)) return false;

    bool hovered = false, held = false;
    const bool clicked = ImGui::ButtonBehavior(bb, id, &hovered, &held);
    if (clicked) *value = !*value;

    float& hoverT = Anim(id ^ 0x44, 0.0f);
    hoverT = ImLerp(hoverT, hovered ? 1.0f : 0.0f, ImMin(1.0f, ImGui::GetIO().DeltaTime * 16.0f));

    ImDrawList* dl = window->DrawList;
    const ImVec2 boxMin(pos.x, pos.y + (lineHeight - box) * 0.5f);
    const ImVec2 boxMax(boxMin.x + box, boxMin.y + box);
    const float rounding = U(6.0f);

    dl->AddRectFilled(boxMin, boxMax, *value ? Colors().brand : Colors().surfaceRaised, rounding);
    if (!*value) {
        dl->AddRect(boxMin, boxMax, LerpColor(Colors().strokeStrong, Colors().brand, hoverT),
                    rounding, 0, U(1.0f));
    }
    if (*value) {
        const ImVec2 p0(boxMin.x + box * 0.24f, boxMin.y + box * 0.52f);
        const ImVec2 p1(boxMin.x + box * 0.44f, boxMin.y + box * 0.72f);
        const ImVec2 p2(boxMin.x + box * 0.78f, boxMin.y + box * 0.30f);
        dl->AddLine(p0, p1, Colors().textOnBrand, U(2.0f));
        dl->AddLine(p1, p2, Colors().textOnBrand, U(2.0f));
    }
    dl->AddText(ImVec2(boxMax.x + U(10.0f), pos.y + (lineHeight - labelSize.y) * 0.5f),
                Colors().text, label);

    if (description && *description) {
        FontScope small(Text().small);
        ImGui::SetCursorScreenPos(ImVec2(pos.x, bb.Max.y + U(2.0f)));
        ImGui::PushStyleColor(ImGuiCol_Text, ToVec4(Colors().textFaint));
        ImGui::PushTextWrapPos(ImGui::GetCursorPosX() + width);
        ImGui::TextUnformatted(description);
        ImGui::PopTextWrapPos();
        ImGui::PopStyleColor();
    } else {
        ImGui::SetCursorScreenPos(ImVec2(pos.x, bb.Max.y));
    }
    if (hovered) ImGui::SetMouseCursor(ImGuiMouseCursor_Hand);
    return clicked;
}

bool Toggle(const char* label, bool* value, const char* description) {
    ImGuiWindow* window = ImGui::GetCurrentWindow();
    const float trackW = U(40.0f);
    const float trackH = U(22.0f);
    FontScope scope(Text().body);
    const ImVec2 labelSize = label ? ImGui::CalcTextSize(label) : ImVec2(0, 0);
    const float width = ImGui::GetContentRegionAvail().x;
    const ImVec2 pos = ImGui::GetCursorScreenPos();
    const ImRect bb(pos, ImVec2(pos.x + width, pos.y + ImMax(trackH, labelSize.y)));
    const ImGuiID id = window->GetID(label ? label : "##toggle");

    ImGui::ItemSize(bb, 0.0f);
    if (!ImGui::ItemAdd(bb, id)) return false;

    bool hovered = false, held = false;
    const bool clicked = ImGui::ButtonBehavior(bb, id, &hovered, &held);
    if (clicked) *value = !*value;

    float& anim = Anim(id ^ 0x55, *value ? 1.0f : 0.0f);
    anim = ImLerp(anim, *value ? 1.0f : 0.0f, ImMin(1.0f, ImGui::GetIO().DeltaTime * 14.0f));

    ImDrawList* dl = window->DrawList;
    const float trackY = pos.y + (bb.GetHeight() - trackH) * 0.5f;
    const ImVec2 tMin(pos.x, trackY);
    const ImVec2 tMax(pos.x + trackW, trackY + trackH);
    dl->AddRectFilled(tMin, tMax,
                      LerpColor(Fade(Colors().strokeStrong, 1.0f), Colors().brand, anim),
                      trackH * 0.5f);
    const float knobR = trackH * 0.5f - U(3.0f);
    const float knobX = ImLerp(tMin.x + trackH * 0.5f, tMax.x - trackH * 0.5f, anim);
    dl->AddCircleFilled(ImVec2(knobX, trackY + trackH * 0.5f), knobR,
                        anim > 0.5f ? Colors().textOnBrand : Colors().text, 20);

    if (label) {
        dl->AddText(ImVec2(tMax.x + U(12.0f), pos.y + (bb.GetHeight() - labelSize.y) * 0.5f),
                    Colors().text, label);
    }

    if (description && *description) {
        FontScope small(Text().small);
        ImGui::SetCursorScreenPos(ImVec2(pos.x, bb.Max.y + U(2.0f)));
        ImGui::PushStyleColor(ImGuiCol_Text, ToVec4(Colors().textFaint));
        ImGui::TextUnformatted(description);
        ImGui::PopStyleColor();
    } else {
        ImGui::SetCursorScreenPos(ImVec2(pos.x, bb.Max.y));
    }
    if (hovered) ImGui::SetMouseCursor(ImGuiMouseCursor_Hand);
    return clicked;
}

bool Segmented(const char* id, int* index, const char* const* labels, int count, float width) {
    ImGuiWindow* window = ImGui::GetCurrentWindow();
    const float height = U(36.0f);
    const float w = width > 0.0f ? width : ImGui::GetContentRegionAvail().x;
    const ImVec2 pos = ImGui::GetCursorScreenPos();
    const ImRect bb(pos, ImVec2(pos.x + w, pos.y + height));
    const ImGuiID wid = window->GetID(id);

    ImGui::ItemSize(bb, 0.0f);
    if (!ImGui::ItemAdd(bb, wid)) return false;

    ImDrawList* dl = window->DrawList;
    dl->AddRectFilled(bb.Min, bb.Max, Colors().surfaceSunken, U(10.0f));
    dl->AddRect(bb.Min, bb.Max, Colors().stroke, U(10.0f), 0, U(1.0f));

    const float segW = w / static_cast<float>(count);
    bool changed = false;
    for (int i = 0; i < count; ++i) {
        const ImVec2 segMin(bb.Min.x + segW * static_cast<float>(i), bb.Min.y);
        const ImVec2 segMax(segMin.x + segW, bb.Max.y);
        ImGui::PushID(i);
        ImGui::SetCursorScreenPos(ImVec2(segMin.x, segMin.y));
        ImGui::InvisibleButton("##seg", ImVec2(segW, height));
        const bool hovered = ImGui::IsItemHovered();
        if (ImGui::IsItemClicked() && *index != i) {
            *index = i;
            changed = true;
        }
        ImGui::PopID();

        const bool active = *index == i;
        if (active) {
            dl->AddRectFilled(ImVec2(segMin.x + U(3), segMin.y + U(3)),
                              ImVec2(segMax.x - U(3), segMax.y - U(3)), Colors().surfaceRaised,
                              U(8.0f));
            dl->AddRect(ImVec2(segMin.x + U(3), segMin.y + U(3)),
                        ImVec2(segMax.x - U(3), segMax.y - U(3)), Colors().strokeStrong, U(8.0f),
                        0, U(1.0f));
        }
        FontScope scope(Text().small);
        const ImVec2 textSize = ImGui::CalcTextSize(labels[i]);
        const ImU32 color = active ? Colors().text : (hovered ? Colors().textDim : Colors().textFaint);
        dl->AddText(ImVec2(segMin.x + (segW - textSize.x) * 0.5f,
                           segMin.y + (height - textSize.y) * 0.5f),
                    color, labels[i]);
    }
    return changed;
}

bool Stepper(const char* id, int* value, int minValue, int maxValue, float width) {
    const float h = U(38.0f);
    const float w = width > 0.0f ? width : ImGui::GetContentRegionAvail().x;
    const float buttonWidth = U(38.0f);
    const ImVec2 start = ImGui::GetCursorScreenPos();
    ImDrawList* dl = ImGui::GetWindowDrawList();

    bool changed = false;
    ImGui::PushID(id);
    dl->AddRectFilled(start, ImVec2(start.x + w, start.y + h), Colors().surfaceSunken, U(9.0f),
                      ImDrawFlags_RoundCornersAll);
    dl->AddRect(start, ImVec2(start.x + w, start.y + h), Colors().stroke, U(9.0f), 0, U(1.0f));

    auto drawButton = [&](float x, Icon icon, bool enabled, int delta) {
        ImGui::SetCursorScreenPos(ImVec2(x, start.y + U(4.0f)));
        const ImVec2 pos = ImGui::GetCursorScreenPos();
        ImGui::InvisibleButton("##step", ImVec2(buttonWidth - U(8.0f), h - U(8.0f)));
        const bool hovered = ImGui::IsItemHovered() && enabled;
        if (hovered) {
            dl->AddRectFilled(pos, ImVec2(pos.x + buttonWidth - U(8.0f), pos.y + h - U(8.0f)),
                              Colors().surfaceHover, U(7.0f));
        }
        DrawIcon(dl, icon, ImVec2(pos.x + (buttonWidth - U(8.0f)) * 0.5f, pos.y + (h - U(8.0f)) * 0.5f),
                 U(13.0f), enabled ? (hovered ? Colors().text : Colors().textDim) : Colors().strokeStrong,
                 U(1.8f));
        if (hovered && ImGui::IsItemClicked()) {
            *value = ImClamp(*value + delta, minValue, maxValue);
            changed = true;
        }
    };

    drawButton(start.x, Icon::Minus, *value > minValue, -1);
    drawButton(start.x + w - buttonWidth, Icon::Plus, *value < maxValue, 1);

    char label[16];
    std::snprintf(label, sizeof(label), "%d", *value);
    const ImVec2 size = Text().medium->CalcTextSizeA(Text().medium->FontSize, FLT_MAX, 0.0f, label);
    dl->AddText(Text().medium, Text().medium->FontSize,
                ImVec2(start.x + (w - size.x) * 0.5f, start.y + (h - size.y) * 0.5f), Colors().text,
                label);
    ImGui::SetCursorScreenPos(ImVec2(start.x, start.y + h));
    ImGui::PopID();
    return changed;
}

bool Select(const char* id, int* index, const char* const* labels, int count, float width) {
    ImGuiWindow* window = ImGui::GetCurrentWindow();
    const float height = U(36.0f);
    const float w = width > 0.0f ? width : ImGui::GetContentRegionAvail().x;
    const ImVec2 pos = ImGui::GetCursorScreenPos();
    const ImRect bb(pos, ImVec2(pos.x + w, pos.y + height));
    const ImGuiID wid = window->GetID(id);

    ImGui::ItemSize(bb, 0.0f);
    if (!ImGui::ItemAdd(bb, wid)) return false;

    bool hovered = false, held = false;
    const bool clicked = ImGui::ButtonBehavior(bb, wid, &hovered, &held);

    float& hoverT = Anim(wid ^ 0x66, 0.0f);
    hoverT = ImLerp(hoverT, hovered ? 1.0f : 0.0f, ImMin(1.0f, ImGui::GetIO().DeltaTime * 16.0f));

    ImDrawList* dl = window->DrawList;
    dl->AddRectFilled(bb.Min, bb.Max, LerpColor(Colors().surfaceRaised, Colors().surfaceHover, hoverT),
                      U(10.0f));
    dl->AddRect(bb.Min, bb.Max, Colors().stroke, U(10.0f), 0, U(1.0f));

    const int safeIndex = (*index >= 0 && *index < count) ? *index : 0;
    FontScope scope(Text().small);
    dl->AddText(ImVec2(bb.Min.x + U(12.0f), bb.Min.y + (height - ImGui::GetTextLineHeight()) * 0.5f),
                Colors().text, labels[safeIndex]);
    DrawIcon(dl, Icon::ChevronDown, ImVec2(bb.Max.x - U(16.0f), bb.Min.y + height * 0.5f), U(14.0f),
             Colors().textDim, U(1.6f));

    ImGui::PushID(id);
    bool changed = false;
    if (clicked) ImGui::OpenPopup("##select");
    if (ImGui::BeginPopup("##select")) {
        ImGui::PushStyleColor(ImGuiCol_WindowBg, ToVec4(Colors().surfaceRaised));
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(U(6), U(6)));
        for (int i = 0; i < count; ++i) {
            ImGui::PushID(i);
            const bool active = safeIndex == i;
            ImGui::PushStyleColor(ImGuiCol_Text, ToVec4(active ? Colors().brand : Colors().text));
            ImGui::PushStyleColor(ImGuiCol_Header, ToVec4(Fade(Colors().brand, 0.16f)));
            ImGui::PushStyleColor(ImGuiCol_HeaderHovered, ToVec4(Colors().surfaceHover));
            ImGui::PushStyleColor(ImGuiCol_HeaderActive, ToVec4(Colors().surfaceHover));
            if (ImGui::Selectable(labels[i], active, 0, ImVec2(w - U(16), U(28)))) {
                *index = i;
                changed = true;
                ImGui::CloseCurrentPopup();
            }
            ImGui::PopStyleColor(4);
            ImGui::PopID();
        }
        ImGui::PopStyleVar();
        ImGui::PopStyleColor();
        ImGui::EndPopup();
    }
    ImGui::PopID();
    return changed;
}

bool CodeEntry(const char* id, std::string& value, bool error, float width) {
    const float height = 58.0f;
    ImGuiWindow* window = ImGui::GetCurrentWindow();
    const float w = width > 0.0f ? width : ImGui::GetContentRegionAvail().x;
    const ImVec2 size(w, U(height));
    const ImGuiID wid = window->GetID(id);

    bool focused = Anim(wid ^ 0xA1, 0.0f) > 0.5f;
    const ImRect bb = BeginFieldFrame(id, w, error, height, focused, U(16), U(16));
    ImGui::ItemSize(size, 0.0f);
    ImGui::ItemAdd(bb, wid);

    const std::string formatted = license::FormatCodeForInput(value);
    const float textHeight = ImGui::GetTextLineHeight();
    ImGui::SetCursorScreenPos(ImVec2(bb.Min.x + U(18.0f), bb.Min.y + (size.y - textHeight) * 0.5f));
    ImGui::PushStyleColor(ImGuiCol_FrameBg, ImVec4(0, 0, 0, 0));
    ImGui::PushStyleColor(ImGuiCol_FrameBgHovered, ImVec4(0, 0, 0, 0));
    ImGui::PushStyleColor(ImGuiCol_FrameBgActive, ImVec4(0, 0, 0, 0));
    ImGui::PushStyleColor(ImGuiCol_Text, ToVec4(error ? Colors().danger : Colors().text));
    ImGui::PushStyleColor(ImGuiCol_TextSelectedBg, ToVec4(Fade(Colors().brand, 0.35f)));
    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(0, 0));
    ImGui::PushItemWidth(w - U(36));
    FontScope scope(Text().h2);
    ImGui::PushID(id);
    std::string buffer = formatted;
    const bool changed = InputTextStd("##code", buffer,
                                      ImGuiInputTextFlags_CharsUppercase |
                                          ImGuiInputTextFlags_AutoSelectAll);
    const bool active = ImGui::IsItemActive();
    if (changed) value = license::FormatCodeForInput(buffer);
    ImGui::PopID();
    ImGui::PopItemWidth();
    ImGui::PopStyleVar();
    ImGui::PopStyleColor(5);

    Anim(wid ^ 0xA1, 0.0f) = active ? 1.0f : 0.0f;

    if (value.empty() && !active) {
        FontScope placeholderFont(Text().h2);
        window->DrawList->AddText(ImVec2(bb.Min.x + U(18.0f), bb.Min.y + (size.y - textHeight) * 0.5f),
                                  Colors().textFaint, "XXXX-XXXX-XXXX-XXXX");
    }
    return changed;
}

void Badge(const char* text, ImU32 color, bool solid) {
    FontScope scope(Text().tiny);
    const ImVec2 textSize = ImGui::CalcTextSize(text);
    const ImVec2 pos = ImGui::GetCursorScreenPos();
    const float h = U(20.0f);
    const float w = textSize.x + U(14.0f);
    ImDrawList* dl = ImGui::GetWindowDrawList();
    dl->AddRectFilled(pos, ImVec2(pos.x + w, pos.y + h),
                      solid ? color : Fade(color, 0.14f), h * 0.5f);
    dl->AddText(ImVec2(pos.x + U(7.0f), pos.y + (h - textSize.y) * 0.5f),
                solid ? Colors().textOnBrand : color, text);
    ImGui::Dummy(ImVec2(w, h));
}

void Pill(const char* text, Icon icon, ImU32 color) {
    FontScope scope(Text().tiny);
    const ImVec2 textSize = ImGui::CalcTextSize(text);
    const ImVec2 pos = ImGui::GetCursorScreenPos();
    const float h = U(22.0f);
    const float iconSize = U(12.0f);
    const float w = textSize.x + (icon == Icon::None ? U(14.0f) : iconSize + U(20.0f));
    ImDrawList* dl = ImGui::GetWindowDrawList();
    dl->AddRectFilled(pos, ImVec2(pos.x + w, pos.y + h), Fade(color, 0.12f), h * 0.5f);
    dl->AddRect(pos, ImVec2(pos.x + w, pos.y + h), Fade(color, 0.30f), h * 0.5f, 0, U(1.0f));
    float x = pos.x + U(8.0f);
    if (icon != Icon::None) {
        DrawIcon(dl, icon, ImVec2(x + iconSize * 0.5f, pos.y + h * 0.5f), iconSize, color, U(1.5f));
        x += iconSize + U(6.0f);
    }
    dl->AddText(ImVec2(x, pos.y + (h - textSize.y) * 0.5f), color, text);
    ImGui::Dummy(ImVec2(w, h));
}

void Avatar(const char* initials, ImU32 accent, float diameter, bool ring) {
    const float d = U(diameter);
    const ImVec2 pos = ImGui::GetCursorScreenPos();
    ImDrawList* dl = ImGui::GetWindowDrawList();
    const ImVec2 center(pos.x + d * 0.5f, pos.y + d * 0.5f);
    dl->AddCircleFilled(center, d * 0.5f, Fade(accent, 0.20f), 32);
    dl->AddCircle(center, d * 0.5f - U(0.5f), Fade(accent, ring ? 0.75f : 0.35f), 32, U(1.2f));
    FontScope scope(diameter >= 44 ? Text().h3 : Text().small);
    const ImVec2 textSize = ImGui::CalcTextSize(initials);
    dl->AddText(ImVec2(center.x - textSize.x * 0.5f, center.y - textSize.y * 0.5f), accent, initials);
    ImGui::Dummy(ImVec2(d, d));
}

void ProgressBar(float fraction, ImVec2 size, ImU32 color, bool indeterminate, float t) {
    const ImVec2 pos = ImGui::GetCursorScreenPos();
    const float w = size.x > 0.0f ? size.x : ImGui::GetContentRegionAvail().x;
    const float h = size.y > 0.0f ? size.y : U(6.0f);
    ImDrawList* dl = ImGui::GetWindowDrawList();
    const ImU32 fill = color == 0 ? Colors().brand : color;

    dl->AddRectFilled(pos, ImVec2(pos.x + w, pos.y + h), Colors().surfaceRaised, h * 0.5f);
    if (indeterminate) {
        const float segment = w * 0.32f;
        const float phase = std::fmod(t * 0.55f, 1.0f);
        const float x = pos.x - segment + (w + segment) * phase;
        const float a = ImClamp(x, pos.x, pos.x + w);
        const float b = ImClamp(x + segment, pos.x, pos.x + w);
        if (b > a) dl->AddRectFilled(ImVec2(a, pos.y), ImVec2(b, pos.y + h), fill, h * 0.5f);
    } else {
        const float clamped = ImClamp(fraction, 0.0f, 1.0f);
        if (clamped > 0.0f) {
            dl->AddRectFilled(pos, ImVec2(pos.x + w * clamped, pos.y + h), fill, h * 0.5f);
        }
    }
    ImGui::Dummy(ImVec2(w, h));
}

void Spinner(ImVec2 center, float radius, float time, ImU32 color, float thickness) {
    ImDrawList* dl = ImGui::GetWindowDrawList();
    const ImU32 tint = color == 0 ? Colors().brand : color;
    dl->AddCircle(center, radius, Fade(tint, 0.18f), 32, thickness);
    const float start = time * 4.2f;
    dl->PathArcTo(center, radius, start, start + 1.6f, 24);
    dl->PathStroke(tint, 0, thickness);
}

void StatTile(const char* label, const char* value, Icon icon, ImU32 accent, float width) {
    const ImVec2 pos = ImGui::GetCursorScreenPos();
    const float w = width > 0.0f ? width : ImGui::GetContentRegionAvail().x;
    const float h = U(76.0f);
    ImDrawList* dl = ImGui::GetWindowDrawList();
    dl->AddRectFilled(pos, ImVec2(pos.x + w, pos.y + h), Colors().surface, U(12.0f));
    dl->AddRect(pos, ImVec2(pos.x + w, pos.y + h), Colors().stroke, U(12.0f), 0, U(1.0f));
    dl->AddRectFilled(pos, ImVec2(pos.x + U(3.0f), pos.y + h), accent, U(2.0f));

    if (icon != Icon::None) {
        DrawIcon(dl, icon, ImVec2(pos.x + U(26.0f), pos.y + U(24.0f)), U(16.0f), accent, U(1.7f));
    }
    FontScope valueFont(Text().h3);
    dl->AddText(ImVec2(pos.x + U(16.0f), pos.y + U(34.0f)), Colors().text, value);
    FontScope labelFont(Text().tiny);
    dl->AddText(ImVec2(pos.x + U(16.0f), pos.y + U(14.0f)), Colors().textFaint, label);
    ImGui::Dummy(ImVec2(w, h));
}

void Sparkline(const float* values, int count, ImVec2 size, ImU32 color, bool fill) {
    if (!values || count < 2) {
        ImGui::Dummy(size);
        return;
    }
    const ImVec2 pos = ImGui::GetCursorScreenPos();
    ImDrawList* dl = ImGui::GetWindowDrawList();
    const ImU32 tint = color == 0 ? Colors().brand : color;

    float minV = values[0], maxV = values[0];
    for (int i = 1; i < count; ++i) {
        minV = ImMin(minV, values[i]);
        maxV = ImMax(maxV, values[i]);
    }
    // Pad the range so a perfectly flat series still renders as a line, not a clipped edge.
    float low = minV;
    float high = maxV;
    if (high - low < 0.5f) {
        const float mid = (low + high) * 0.5f;
        low = mid - 0.5f;
        high = mid + 0.5f;
    } else {
        const float pad = (high - low) * 0.12f;
        low -= pad;
        high += pad;
    }
    const float span = ImMax(0.0001f, high - low);

    std::vector<ImVec2> points;
    points.reserve(static_cast<std::size_t>(count));
    for (int i = 0; i < count; ++i) {
        const float x = pos.x + size.x * (static_cast<float>(i) / static_cast<float>(count - 1));
        const float y = pos.y + size.y - ((values[i] - low) / span) * (size.y - U(4.0f)) - U(2.0f);
        points.push_back(ImVec2(x, y));
    }
    if (fill) {
        std::vector<ImVec2> area = points;
        area.push_back(ImVec2(pos.x + size.x, pos.y + size.y));
        area.push_back(ImVec2(pos.x, pos.y + size.y));
        dl->AddConcavePolyFilled(area.data(), static_cast<int>(area.size()), Fade(tint, 0.14f));
    }
    dl->AddPolyline(points.data(), static_cast<int>(points.size()), tint, 0, U(1.8f));
    dl->AddCircleFilled(points.back(), U(2.6f), tint, 10);
    ImGui::Dummy(size);
}

void Ring(float fraction, float diameter, const char* centerLabel, ImU32 color) {
    const float d = U(diameter);
    const ImVec2 pos = ImGui::GetCursorScreenPos();
    ImDrawList* dl = ImGui::GetWindowDrawList();
    const ImVec2 center(pos.x + d * 0.5f, pos.y + d * 0.5f);
    const ImU32 tint = color == 0 ? Colors().brand : color;
    const float thickness = U(5.0f);
    dl->AddCircle(center, d * 0.5f - thickness, Fade(Colors().strokeStrong, 0.9f), 48, thickness);
    if (fraction > 0.001f) {
        dl->PathArcTo(center, d * 0.5f - thickness, -kPi * 0.5f,
                      -kPi * 0.5f + kPi * 2.0f * ImClamp(fraction, 0.0f, 1.0f), 48);
        dl->PathStroke(tint, 0, thickness);
    }
    if (centerLabel) {
        FontScope scope(Text().medium);
        const ImVec2 textSize = ImGui::CalcTextSize(centerLabel);
        dl->AddText(ImVec2(center.x - textSize.x * 0.5f, center.y - textSize.y * 0.5f),
                    Colors().text, centerLabel);
    }
    ImGui::Dummy(ImVec2(d, d));
}

void KeyValue(const char* key, const char* value, ImU32 valueColor) {
    FontScope scope(Text().small);
    ImGui::PushStyleColor(ImGuiCol_Text, ToVec4(Colors().textFaint));
    ImGui::TextUnformatted(key);
    ImGui::PopStyleColor();
    ImGui::SameLine();
    const float width = ImGui::CalcTextSize(value).x;
    ImGui::SetCursorPosX(ImGui::GetCursorPosX() + ImGui::GetContentRegionAvail().x - width);
    ImGui::PushStyleColor(ImGuiCol_Text,
                          ToVec4(valueColor == 0 ? Colors().textDim : valueColor));
    ImGui::TextUnformatted(value);
    ImGui::PopStyleColor();
}

void SeparatorText(const char* text) {
    FontScope scope(Text().tiny);
    ImGui::PushStyleColor(ImGuiCol_Text, ToVec4(Colors().textFaint));
    ImGui::PushStyleVar(ImGuiStyleVar_SeparatorTextBorderSize, U(1.0f));
    ImGui::PushStyleColor(ImGuiCol_Separator, ToVec4(Colors().stroke));
    ImGui::SeparatorText(text);
    ImGui::PopStyleColor(2);
    ImGui::PopStyleVar();
}

void SkeletonBar(ImVec2 size, float time, float rounding) {
    const ImVec2 pos = ImGui::GetCursorScreenPos();
    ImDrawList* dl = ImGui::GetWindowDrawList();
    const float pulse = 0.35f + 0.25f * std::sin(time * 3.0f);
    dl->AddRectFilled(pos, ImVec2(pos.x + size.x, pos.y + size.y),
                      Fade(Colors().surfaceHover, pulse), U(rounding));
    ImGui::Dummy(size);
}

void EmptyState(Icon icon, const char* title, const char* body, const char* action,
                bool* actionClicked) {
    const float width = ImGui::GetContentRegionAvail().x;
    const float circle = U(56.0f);
    const ImVec2 pos = ImGui::GetCursorScreenPos();
    ImDrawList* dl = ImGui::GetWindowDrawList();
    const ImVec2 center(pos.x + width * 0.5f, pos.y + circle * 0.5f + U(8));
    dl->AddCircleFilled(center, circle * 0.5f, Fade(Colors().brand, 0.10f), 32);
    dl->AddCircle(center, circle * 0.5f, Fade(Colors().brand, 0.28f), 32, U(1.0f));
    DrawIcon(dl, icon, center, U(24.0f), Colors().brand, U(1.6f));
    ImGui::Dummy(ImVec2(width, circle + U(16)));

    FontScope titleFont(Text().h3);
    const ImVec2 titleSize = ImGui::CalcTextSize(title);
    ImGui::SetCursorPosX(ImGui::GetCursorPosX() + (width - titleSize.x) * 0.5f);
    ImGui::PushStyleColor(ImGuiCol_Text, ToVec4(Colors().text));
    ImGui::TextUnformatted(title);
    ImGui::PopStyleColor();

    if (body && *body) {
        FontScope bodyFont(Text().small);
        const float wrap = width * 0.75f;
        ImGui::SetCursorPosX(ImGui::GetCursorPosX() + (width - wrap) * 0.5f);
        ImGui::PushStyleColor(ImGuiCol_Text, ToVec4(Colors().textDim));
        ImGui::PushTextWrapPos(ImGui::GetCursorPosX() + wrap);
        ImGui::TextWrapped("%s", body);
        ImGui::PopTextWrapPos();
        ImGui::PopStyleColor();
    }

    if (action && actionClicked) {
        VSpace(10.0f);
        const float buttonWidth = U(200.0f);
        ImGui::SetCursorPosX(ImGui::GetCursorPosX() + (width - buttonWidth) * 0.5f);
        *actionClicked = SecondaryButton(action, ImVec2(buttonWidth, U(36.0f)));
    }
}

void BrandMark(ImDrawList* dl, ImVec2 center, float radius, ImU32 color, ImU32 innerColor,
               bool withMonogram, float glowStrength) {
    if (glowStrength > 0.0f) {
        RadialGlow(dl, center, radius * 2.6f, color, glowStrength);
    }

    const float R = radius;
    const float r = R * 0.80f;
    const float biteAngle = -kPi * 0.28f;
    const ImVec2 dir(std::cos(biteAngle), std::sin(biteAngle));
    const float dist = R * 0.60f;
    const ImVec2 c2(center.x + dir.x * dist, center.y + dir.y * dist);

    // Circle/circle intersection gives the two crescent tips.
    const float a = (dist * dist + R * R - r * r) / (2.0f * dist);
    const float h = std::sqrt(ImMax(0.0f, R * R - a * a));
    const ImVec2 mid(center.x + dir.x * a, center.y + dir.y * a);
    const ImVec2 perp(-dir.y, dir.x);
    const ImVec2 tip1(mid.x + perp.x * h, mid.y + perp.y * h);
    const ImVec2 tip2(mid.x - perp.x * h, mid.y - perp.y * h);

    const float a1 = std::atan2(tip1.y - center.y, tip1.x - center.x);
    const float a2 = std::atan2(tip2.y - center.y, tip2.x - center.x);
    float delta = a2 - a1;
    while (delta > kPi) delta -= 2.0f * kPi;
    while (delta < -kPi) delta += 2.0f * kPi;
    const float longDelta = delta > 0.0f ? delta - 2.0f * kPi : delta + 2.0f * kPi;

    // Outer boundary: take the sweep that stays far from the bite circle.
    auto midDistance = [&](float start, float sweep, float radiusUsed, ImVec2 origin) {
        const float midAngle = start + sweep * 0.5f;
        const ImVec2 p(origin.x + std::cos(midAngle) * radiusUsed,
                       origin.y + std::sin(midAngle) * radiusUsed);
        return std::sqrt((p.x - c2.x) * (p.x - c2.x) + (p.y - c2.y) * (p.y - c2.y));
    };
    const float outerSweep =
        midDistance(a1, delta, R, center) > midDistance(a1, longDelta, R, center) ? delta : longDelta;

    std::vector<ImVec2> poly;
    const int outerSegments = 64;
    for (int i = 0; i <= outerSegments; ++i) {
        const float angle = a1 + outerSweep * (static_cast<float>(i) / outerSegments);
        poly.push_back(ImVec2(center.x + std::cos(angle) * R, center.y + std::sin(angle) * R));
    }

    // Inner boundary: the short arc of the bite circle, walking back to the first tip.
    const float b1 = std::atan2(tip2.y - c2.y, tip2.x - c2.x);
    const float b2 = std::atan2(tip1.y - c2.y, tip1.x - c2.x);
    float bDelta = b2 - b1;
    while (bDelta > kPi) bDelta -= 2.0f * kPi;
    while (bDelta < -kPi) bDelta += 2.0f * kPi;
    auto distanceToCenter = [&](float start, float sweep) {
        const float midAngle = start + sweep * 0.5f;
        const ImVec2 p(c2.x + std::cos(midAngle) * r, c2.y + std::sin(midAngle) * r);
        return std::sqrt((p.x - center.x) * (p.x - center.x) + (p.y - center.y) * (p.y - center.y));
    };
    const float innerSweep =
        distanceToCenter(b1, bDelta) < distanceToCenter(b1, bDelta > 0 ? bDelta - 2.0f * kPi
                                                                      : bDelta + 2.0f * kPi)
            ? bDelta
            : (bDelta > 0 ? bDelta - 2.0f * kPi : bDelta + 2.0f * kPi);
    const int innerSegments = 48;
    for (int i = 0; i <= innerSegments; ++i) {
        const float angle = b1 + innerSweep * (static_cast<float>(i) / innerSegments);
        poly.push_back(ImVec2(c2.x + std::cos(angle) * r, c2.y + std::sin(angle) * r));
    }

    dl->AddConcavePolyFilled(poly.data(), static_cast<int>(poly.size()), color);

    if (withMonogram) {
        // A monogram inside the body of the crescent.
        const ImVec2 mc(center.x - dir.x * R * 0.18f, center.y - dir.y * R * 0.18f);
        const float top = mc.y - R * 0.30f;
        const float bottom = mc.y + R * 0.32f;
        const float halfWidth = R * 0.30f;
        const float thick = ImMax(1.4f, R * 0.055f);
        dl->AddLine(ImVec2(mc.x, top), ImVec2(mc.x - halfWidth, bottom), innerColor, thick);
        dl->AddLine(ImVec2(mc.x, top), ImVec2(mc.x + halfWidth, bottom), innerColor, thick);
        dl->AddLine(ImVec2(mc.x - halfWidth * 0.45f, mc.y + R * 0.08f),
                    ImVec2(mc.x + halfWidth * 0.45f, mc.y + R * 0.08f), innerColor, thick * 0.9f);
    }
}

void LogoImage(ImVec2 size, float glowStrength) {
    const ImVec2 pos = ImGui::GetCursorScreenPos();
    const ImVec2 center(pos.x + size.x * 0.5f, pos.y + size.y * 0.5f);
    ImDrawList* dl = ImGui::GetWindowDrawList();
    const branding::Image* logo = branding::Logo();
    if (logo && logo->texture != 0) {
        if (glowStrength > 0.0f) {
            RadialGlow(dl, center, size.x * 0.85f, Colors().brand, glowStrength);
        }
        dl->AddImage(logo->texture, ImVec2(center.x - size.x * 0.5f, center.y - size.y * 0.5f),
                     ImVec2(center.x + size.x * 0.5f, center.y + size.y * 0.5f));
    } else {
        BrandMark(dl, center, size.x * 0.42f, Colors().brand, Colors().brandDeep, true, glowStrength);
    }
    ImGui::Dummy(size);
}

void RadialGlow(ImDrawList* dl, ImVec2 center, float radius, ImU32 color, float strength) {
    const int layers = 18;
    for (int i = layers; i > 0; --i) {
        const float t = static_cast<float>(i) / static_cast<float>(layers);
        const float r = radius * t;
        const float alpha = (1.0f - t) * 0.05f * strength;
        dl->AddCircleFilled(center, r, Fade(color, alpha), 48);
    }
}

void Backdrop(ImDrawList* dl, ImVec2 min, ImVec2 max, float time, bool withGrid) {
    dl->AddRectFilledMultiColor(min, max, Colors().bgDeep, Colors().bgDeep, Colors().bg, Colors().bg);
    RadialGlow(dl, ImVec2(min.x + (max.x - min.x) * 0.18f, min.y - (max.y - min.y) * 0.05f),
               (max.x - min.x) * 0.65f, Colors().brand, 0.55f);
    RadialGlow(dl, ImVec2(max.x * 0.95f, max.y * 0.95f), (max.x - min.x) * 0.35f,
               IM_COL32(120, 90, 255, 255), 0.20f);
    if (withGrid) {
        Grid(dl, min, max, U(48.0f), IM_COL32(255, 255, 255, 7));
        const float drift = std::fmod(time * 6.0f, U(220.0f));
        dl->AddLine(ImVec2(min.x, min.y + drift), ImVec2(max.x, min.y + drift),
                    IM_COL32(255, 255, 255, 4), U(1.0f));
    }
}

void Grid(ImDrawList* dl, ImVec2 min, ImVec2 max, float cell, ImU32 color) {
    if (cell <= 1.0f) return;
    for (float x = min.x; x < max.x; x += cell) {
        dl->AddLine(ImVec2(x, min.y), ImVec2(x, max.y), color, 1.0f);
    }
    for (float y = min.y; y < max.y; y += cell) {
        dl->AddLine(ImVec2(min.x, y), ImVec2(max.x, y), color, 1.0f);
    }
}

void Tooltip(const char* text) {
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(U(10), U(7)));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, U(8));
    ImGui::PushStyleColor(ImGuiCol_PopupBg, ToVec4(Colors().surfaceRaised));
    ImGui::PushStyleColor(ImGuiCol_Border, ToVec4(Colors().strokeStrong));
    ImGui::BeginTooltip();
    FontScope scope(Text().small);
    ImGui::PushStyleColor(ImGuiCol_Text, ToVec4(Colors().text));
    ImGui::TextUnformatted(text);
    ImGui::PopStyleColor();
    ImGui::EndTooltip();
    ImGui::PopStyleColor(2);
    ImGui::PopStyleVar(2);
}

float EaseOutCubic(float t) { return 1.0f - std::pow(1.0f - t, 3.0f); }
float EaseInOutCubic(float t) {
    return t < 0.5f ? 4.0f * t * t * t : 1.0f - std::pow(-2.0f * t + 2.0f, 3.0f) * 0.5f;
}
float EaseOutQuint(float t) { return 1.0f - std::pow(1.0f - t, 5.0f); }

ImU32 Fade(ImU32 color, float alpha) {
    const int base = static_cast<int>((color >> 24) & 0xFF);
    const int result = ImClamp(static_cast<int>(base * ImClamp(alpha, 0.0f, 1.0f)), 0, 255);
    return WithAlpha(color, result);
}

}  // namespace astra::ui
