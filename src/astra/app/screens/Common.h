// app/screens/Common.h - small helpers shared by the launcher screens.
#pragma once

#include <string>

#include "astra/app/AstraApp.h"
#include "astra/ui/Icons.h"
#include "astra/ui/Theme.h"

namespace astra::app::screens {

// Draws text with manual letter spacing (used for the tracked wordmark).
void SpacedText(ImDrawList* dl, ImFont* font, ImU32 color, ImVec2 pos, const char* text,
                float spacing);
float SpacedWidth(ImFont* font, const char* text, float spacing);

// "ASTRA" + "PRODUCTIONS" lockup used under the logo.
void Wordmark(ImU32 mainColor, ImU32 subColor, bool centered, float width);

// Registration progress: 1 = access code, 2 = account, 3 = ready.
void StepIndicator(int current, ImVec2 size);

// Inline message blocks.
void ErrorBanner(const std::string& message, const std::string& hint = {});
void SuccessBanner(const std::string& message, const std::string& hint = {});
void InfoBanner(const std::string& message, ui::Icon icon = ui::Icon::Info, ImU32 color = 0);

// Label/value line used in the code summary chips.
void SummaryChip(const char* label, const char* value, ImU32 accent);

// Vertical centring helper: returns the y offset that centres `contentHeight`
// inside a region, clamped so content never overlaps the top bar.
float CenterVertically(float regionTop, float regionHeight, float contentHeight,
                       float minTopPadding = 24.0f);

// Starts a full-area child window (no background) so screens can scroll.
void BeginScreenBody(const char* id);
void EndScreenBody();

}  // namespace astra::app::screens
