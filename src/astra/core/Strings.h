// core/Strings.h - small, dependency-free string helpers used across Astra.
#pragma once

#include <string>
#include <vector>

namespace astra::str {

std::string ToLower(std::string_view s);
std::string ToUpper(std::string_view s);
std::string Trim(std::string_view s);
std::vector<std::string> Split(std::string_view s, char sep);
std::string Join(const std::vector<std::string>& parts, std::string_view sep);
bool StartsWith(std::string_view s, std::string_view prefix);
bool EndsWith(std::string_view s, std::string_view suffix);
bool Contains(std::string_view s, std::string_view needle);
bool EqualsIgnoreCase(std::string_view a, std::string_view b);
std::string ReplaceAll(std::string s, std::string_view from, std::string_view to);

// Removes characters that are unsafe in file names.
std::string SanitizeFileName(std::string_view s);

// Escapes a string for embedding inside a JSON string literal (no quotes added).
std::string JsonEscape(std::string_view s);

// Formats with printf-like syntax and returns a std::string.
std::string Format(const char* fmt, ...);

// Formats a byte count as "1.4 MB".
std::string HumanBytes(std::uint64_t bytes);

// Formats an age in seconds as "3s", "5m", "2h", "4d".
std::string HumanAge(std::int64_t seconds);

// Truncates with an ellipsis so the result fits in maxChars.
std::string Ellipsis(std::string_view s, std::size_t maxChars);

// Initials for an avatar: "Ada Lovelace" -> "AL".
std::string Initials(std::string_view displayName);

// Converts UTF-8 text into a display string safe for ImGui's default glyph range,
// replacing unsupported code points with '?'. Keeps ASCII verbatim.
std::string AsciiFold(std::string_view s);

}  // namespace astra::str
