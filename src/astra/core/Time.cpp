#include "astra/core/Time.h"

#include <chrono>
#include <cstdio>
#include <ctime>

#include "astra/core/Strings.h"

namespace astra::time {
namespace {

constexpr std::int64_t kSecondsPerDay = 86400;

std::tm ToTm(std::int64_t unixSeconds) {
    std::tm tm{};
    const std::time_t t = static_cast<std::time_t>(unixSeconds);
#if defined(_WIN32)
    gmtime_s(&tm, &t);
#else
    gmtime_r(&t, &tm);
#endif
    return tm;
}

}  // namespace

std::int64_t NowUtc() {
    using namespace std::chrono;
    return duration_cast<seconds>(system_clock::now().time_since_epoch()).count();
}

double NowMonotonicMs() {
    using namespace std::chrono;
    static const auto start = steady_clock::now();
    return duration_cast<duration<double, std::milli>>(steady_clock::now() - start).count();
}

std::int64_t DaysSinceEpoch(std::int64_t unixSeconds) {
    const std::int64_t d = unixSeconds / kSecondsPerDay;
    return (unixSeconds % kSecondsPerDay < 0) ? d - 1 : d;
}

std::string FormatUtc(std::int64_t unixSeconds, bool withDate) {
    const std::tm tm = ToTm(unixSeconds);
    char buf[64];
    if (withDate) {
        std::snprintf(buf, sizeof(buf), "%04d-%02d-%02d %02d:%02d", tm.tm_year + 1900,
                      tm.tm_mon + 1, tm.tm_mday, tm.tm_hour, tm.tm_min);
    } else {
        std::snprintf(buf, sizeof(buf), "%02d:%02d", tm.tm_hour, tm.tm_min);
    }
    return buf;
}

std::string ToIso8601(std::int64_t unixSeconds) {
    const std::tm tm = ToTm(unixSeconds);
    char buf[64];
    std::snprintf(buf, sizeof(buf), "%04d-%02d-%02dT%02d:%02d:%02dZ", tm.tm_year + 1900,
                  tm.tm_mon + 1, tm.tm_mday, tm.tm_hour, tm.tm_min, tm.tm_sec);
    return buf;
}

std::int64_t ParseIso8601(std::string_view text) {
    std::tm tm{};
    const std::string s(text);
    if (std::sscanf(s.c_str(), "%4d-%2d-%2dT%2d:%2d:%2d", &tm.tm_year, &tm.tm_mon,
                    &tm.tm_mday, &tm.tm_hour, &tm.tm_min, &tm.tm_sec) != 6) {
        return 0;
    }
    tm.tm_year -= 1900;
    tm.tm_mon -= 1;
#if defined(_WIN32)
    const std::time_t t = _mkgmtime(&tm);
#else
    const std::time_t t = timegm(&tm);
#endif
    return static_cast<std::int64_t>(t);
}

std::string Relative(std::int64_t unixSeconds, std::int64_t nowUtc) {
    if (unixSeconds <= 0) return "never";
    const std::int64_t delta = nowUtc - unixSeconds;
    if (delta < 45) return "just now";
    if (delta < 3600) return str::Format("%lldm ago", static_cast<long long>(delta / 60));
    if (delta < 86400) return str::Format("%lldh ago", static_cast<long long>(delta / 3600));
    if (delta < 86400 * 30) return str::Format("%lldd ago", static_cast<long long>(delta / 86400));
    if (delta < 86400 * 365)
        return str::Format("%lldmo ago", static_cast<long long>(delta / (86400 * 30)));
    return str::Format("%lldy ago", static_cast<long long>(delta / (86400 * 365)));
}

}  // namespace astra::time
