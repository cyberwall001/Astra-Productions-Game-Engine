// core/Time.h - UTC clock, monotonic timing and small date helpers.
#pragma once

#include <cstdint>
#include <string>

namespace astra::time {

// Seconds since the unix epoch, UTC.
std::int64_t NowUtc();

// Monotonic milliseconds since process start (never jumps backwards).
double NowMonotonicMs();

// Whole days since 1970-01-01 UTC for a unix timestamp.
std::int64_t DaysSinceEpoch(std::int64_t unixSeconds);

// Formats a unix timestamp as "2026-11-19 14:03" (UTC).
std::string FormatUtc(std::int64_t unixSeconds, bool withDate = true);

// Formats a unix timestamp as an ISO-8601 string, e.g. "2026-11-19T14:03:12Z".
std::string ToIso8601(std::int64_t unixSeconds);

// Parses an ISO-8601 timestamp; returns 0 on failure.
std::int64_t ParseIso8601(std::string_view text);

// "just now", "3m ago", "5h ago", "2d ago".
std::string Relative(std::int64_t unixSeconds, std::int64_t nowUtc);

}  // namespace astra::time
