#include "astra/core/Strings.h"

#include <algorithm>
#include <cctype>
#include <cstdarg>
#include <cstdio>
#include <cstring>

namespace astra::str {

std::string ToLower(std::string_view s) {
    std::string out(s);
    std::transform(out.begin(), out.end(), out.begin(),
                   [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return out;
}

std::string ToUpper(std::string_view s) {
    std::string out(s);
    std::transform(out.begin(), out.end(), out.begin(),
                   [](unsigned char c) { return static_cast<char>(std::toupper(c)); });
    return out;
}

std::string Trim(std::string_view s) {
    std::size_t b = 0;
    std::size_t e = s.size();
    auto isSpace = [](unsigned char c) { return std::isspace(c) != 0; };
    while (b < e && isSpace(static_cast<unsigned char>(s[b]))) ++b;
    while (e > b && isSpace(static_cast<unsigned char>(s[e - 1]))) --e;
    return std::string(s.substr(b, e - b));
}

std::vector<std::string> Split(std::string_view s, char sep) {
    std::vector<std::string> out;
    std::size_t start = 0;
    while (true) {
        const std::size_t pos = s.find(sep, start);
        if (pos == std::string_view::npos) {
            out.emplace_back(s.substr(start));
            break;
        }
        out.emplace_back(s.substr(start, pos - start));
        start = pos + 1;
    }
    return out;
}

std::string Join(const std::vector<std::string>& parts, std::string_view sep) {
    std::string out;
    for (std::size_t i = 0; i < parts.size(); ++i) {
        if (i) out += sep;
        out += parts[i];
    }
    return out;
}

bool StartsWith(std::string_view s, std::string_view prefix) {
    return s.size() >= prefix.size() && s.compare(0, prefix.size(), prefix) == 0;
}

bool EndsWith(std::string_view s, std::string_view suffix) {
    return s.size() >= suffix.size() &&
           s.compare(s.size() - suffix.size(), suffix.size(), suffix) == 0;
}

bool Contains(std::string_view s, std::string_view needle) {
    return needle.empty() || s.find(needle) != std::string_view::npos;
}

bool EqualsIgnoreCase(std::string_view a, std::string_view b) {
    if (a.size() != b.size()) return false;
    for (std::size_t i = 0; i < a.size(); ++i) {
        if (std::tolower(static_cast<unsigned char>(a[i])) !=
            std::tolower(static_cast<unsigned char>(b[i])))
            return false;
    }
    return true;
}

std::string ReplaceAll(std::string s, std::string_view from, std::string_view to) {
    if (from.empty()) return s;
    std::size_t pos = 0;
    while ((pos = s.find(from, pos)) != std::string::npos) {
        s.replace(pos, from.size(), to);
        pos += to.size();
    }
    return s;
}

std::string SanitizeFileName(std::string_view s) {
    std::string out;
    out.reserve(s.size());
    for (char c : s) {
        const unsigned char u = static_cast<unsigned char>(c);
        if (std::isalnum(u) || c == '-' || c == '_' || c == '.') {
            out += c;
        } else if (c == ' ' || c == '/') {
            out += '_';
        }
    }
    if (out.empty()) out = "untitled";
    return out;
}

std::string JsonEscape(std::string_view s) {
    std::string out;
    out.reserve(s.size() + 8);
    for (char c : s) {
        switch (c) {
            case '"': out += "\\\""; break;
            case '\\': out += "\\\\"; break;
            case '\n': out += "\\n"; break;
            case '\r': out += "\\r"; break;
            case '\t': out += "\\t"; break;
            default:
                if (static_cast<unsigned char>(c) < 0x20) {
                    out += Format("\\u%04x", static_cast<unsigned>(c));
                } else {
                    out += c;
                }
        }
    }
    return out;
}

std::string Format(const char* fmt, ...) {
    va_list args;
    va_start(args, fmt);
    va_list copy;
    va_copy(copy, args);
    const int needed = std::vsnprintf(nullptr, 0, fmt, copy);
    va_end(copy);
    if (needed <= 0) {
        va_end(args);
        return {};
    }
    std::string out(static_cast<std::size_t>(needed), '\0');
    std::vsnprintf(out.data(), out.size() + 1, fmt, args);
    va_end(args);
    return out;
}

std::string HumanBytes(std::uint64_t bytes) {
    static const char* kUnits[] = {"B", "KB", "MB", "GB", "TB"};
    double value = static_cast<double>(bytes);
    int unit = 0;
    while (value >= 1024.0 && unit < 4) {
        value /= 1024.0;
        ++unit;
    }
    return (unit == 0) ? Format("%llu B", static_cast<unsigned long long>(bytes))
                       : Format("%.1f %s", value, kUnits[unit]);
}

std::string HumanAge(std::int64_t seconds) {
    if (seconds < 0) seconds = 0;
    if (seconds < 60) return Format("%llds", static_cast<long long>(seconds));
    if (seconds < 3600) return Format("%lldm", static_cast<long long>(seconds / 60));
    if (seconds < 86400) return Format("%lldh", static_cast<long long>(seconds / 3600));
    return Format("%lldd", static_cast<long long>(seconds / 86400));
}

std::string Ellipsis(std::string_view s, std::size_t maxChars) {
    if (s.size() <= maxChars) return std::string(s);
    if (maxChars <= 3) return std::string(s.substr(0, maxChars));
    return std::string(s.substr(0, maxChars - 3)) + "...";
}

std::string Initials(std::string_view displayName) {
    std::string out;
    bool newWord = true;
    for (char c : displayName) {
        if (c == ' ' || c == '-' || c == '_' || c == '.') {
            newWord = true;
            continue;
        }
        if (newWord) {
            out += static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
            newWord = false;
            if (out.size() == 2) break;
        }
    }
    if (out.empty()) out = "?";
    return out;
}

std::string AsciiFold(std::string_view s) {
    std::string out;
    out.reserve(s.size());
    for (std::size_t i = 0; i < s.size();) {
        const unsigned char c = static_cast<unsigned char>(s[i]);
        if (c < 0x80) {
            out += static_cast<char>(c);
            ++i;
        } else {
            // Skip a full UTF-8 sequence; ImGui's default glyph range is Latin-1 only.
            std::size_t len = 1;
            if ((c & 0xE0) == 0xC0) len = 2;
            else if ((c & 0xF0) == 0xE0) len = 3;
            else if ((c & 0xF8) == 0xF0) len = 4;
            i += len;
            out += '?';
        }
    }
    return out;
}

}  // namespace astra::str
