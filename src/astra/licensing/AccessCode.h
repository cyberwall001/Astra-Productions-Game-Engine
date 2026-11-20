// licensing/AccessCode.h - self-contained, offline-verifiable one-time access codes.
//
// Wire format (10 bytes -> 16 Crockford base32 chars, grouped 4-4-4-4):
//
//   payload (5 bytes, 40 bits)              tag (5 bytes)
//   ------------------------------------    --------------------------------
//   version : 4 bits   (currently 1)        HMAC-SHA256(masterKey, payload)[0..5]
//   role    : 3 bits   (user/staff/owner)
//   duration: 6 bits   (index into kDurations, 0 = never expires)
//   issued  : 12 bits  (days since 2024-01-01)
//   nonce   : 15 bits  (collision resistance between codes)
//
// Example: "K7QM-3XPD-9RTA-B2VH"
#pragma once

#include <string>
#include <string_view>
#include <vector>

#include "astra/core/Types.h"

namespace astra::license {

inline constexpr int kCodePayloadBytes = 5;
inline constexpr int kCodeTagBytes = 5;
inline constexpr int kCodeTotalBytes = kCodePayloadBytes + kCodeTagBytes;
inline constexpr int kCodeChars = 16;

enum class Role { User = 0, Staff = 1, Owner = 2 };
const char* RoleName(Role role);       // "User", "Staff", "Owner"
const char* RoleBadge(Role role);      // "USER", "STAFF", "OWNER"

struct DurationOption {
    const char* label;   // "30 days"
    int days;            // 0 = never expires
};
const std::vector<DurationOption>& DurationOptions();
int DurationDaysFromIndex(int index);
int DurationIndexFromDays(int days);
std::string DurationLabel(int days);

struct CodePayload {
    int version = 1;
    Role role = Role::User;
    int durationDays = 30;   // 0 = never
    std::int64_t issuedDay = 0;  // days since 2024-01-01
    std::uint32_t nonce = 0;
};

// A fully parsed + verified code.
struct AccessCode {
    std::string text;          // canonical "XXXX-XXXX-XXXX-XXXX"
    CodePayload payload;
    std::int64_t issuedUtc = 0;
    std::int64_t expiryUtc = 0;   // 0 when the code never expires
    bool neverExpires = false;

    bool IsStaff() const { return payload.role != Role::User; }
    std::string Description() const;  // "Staff access - expires 2026-12-19"
};

enum class CodeError {
    None = 0,
    Empty,
    Malformed,
    BadSignature,
    WrongVersion,
    NotYetValid,
    Expired,
    AlreadyUsed,
    RoleNotAllowed,
};

const char* CodeErrorMessage(CodeError error);
const char* CodeErrorHint(CodeError error);

// Formats up to 16 base32 characters as groups while typing ("4/4/4/4").
std::string FormatCodeForInput(std::string_view raw);
bool IsCompleteCode(std::string_view raw);

// Verifies signature + version + window. Does NOT check single use (see CodeRegistry).
struct CodeResult {
    CodeError error = CodeError::None;
    AccessCode code;
    bool ok() const { return error == CodeError::None; }
};
CodeResult VerifyCode(std::string_view text, const Bytes& secret, std::int64_t nowUtc);

// Issues a new code (used by KeyForge only).
AccessCode IssueCode(const CodePayload& payload, const Bytes& secret);

// Expiry helpers.
std::int64_t IssuedUtcFromDay(std::int64_t day, std::int64_t nowUtc = 0);
std::int64_t ExpiryUtc(const CodePayload& payload, std::int64_t nowUtc);
// Converts a unix timestamp into the 12-bit "days since epoch" field.
std::int64_t DayFromUtc(std::int64_t unixSeconds);

}  // namespace astra::license
