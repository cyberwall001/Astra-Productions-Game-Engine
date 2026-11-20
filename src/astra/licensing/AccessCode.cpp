#include "astra/licensing/AccessCode.h"

#include <array>

#include "astra/core/Strings.h"
#include "astra/core/Time.h"
#include "astra/crypto/Encoding.h"
#include "astra/crypto/Sha256.h"

namespace astra::license {
namespace {

// Day 0 of the issued-day field: 2024-01-01T00:00:00Z.
constexpr std::int64_t kEpochUtc = 1704067200;
constexpr std::int64_t kSecondsPerDay = 86400;

// Bit layout of the 40-bit payload.
constexpr int kVersionShift = 36;
constexpr int kRoleShift = 33;
constexpr int kDurationShift = 27;
constexpr int kIssuedShift = 15;
constexpr int kNonceShift = 0;

void WritePayload(const CodePayload& payload, u8 out[kCodePayloadBytes]) {
    const std::uint64_t value = (static_cast<std::uint64_t>(payload.version & 0xF) << kVersionShift) |
                                (static_cast<std::uint64_t>(static_cast<int>(payload.role) & 0x7) << kRoleShift) |
                                (static_cast<std::uint64_t>(DurationIndexFromDays(payload.durationDays) & 0x3F) << kDurationShift) |
                                (static_cast<std::uint64_t>(payload.issuedDay & 0xFFF) << kIssuedShift) |
                                (static_cast<std::uint64_t>(payload.nonce & 0x7FFF) << kNonceShift);
    for (int i = 0; i < kCodePayloadBytes; ++i) {
        out[i] = static_cast<u8>((value >> (8 * (kCodePayloadBytes - 1 - i))) & 0xFF);
    }
}

bool ReadPayload(const u8 in[kCodePayloadBytes], CodePayload& payload) {
    std::uint64_t value = 0;
    for (int i = 0; i < kCodePayloadBytes; ++i) value = (value << 8) | in[i];

    payload.version = static_cast<int>((value >> kVersionShift) & 0xF);
    const int role = static_cast<int>((value >> kRoleShift) & 0x7);
    if (role > static_cast<int>(Role::Owner)) return false;
    payload.role = static_cast<Role>(role);
    payload.durationDays = DurationDaysFromIndex(static_cast<int>((value >> kDurationShift) & 0x3F));
    payload.issuedDay = static_cast<std::int64_t>((value >> kIssuedShift) & 0xFFF);
    payload.nonce = static_cast<std::uint32_t>((value >> kNonceShift) & 0x7FFF);
    return true;
}

std::array<u8, kCodeTagBytes> ComputeTag(const u8 payload[kCodePayloadBytes], const Bytes& secret) {
    const auto mac = crypto::HmacSha256(ByteView(secret), ByteView(payload, kCodePayloadBytes));
    std::array<u8, kCodeTagBytes> tag{};
    for (int i = 0; i < kCodeTagBytes; ++i) tag[i] = mac[static_cast<std::size_t>(i)];
    return tag;
}

}  // namespace

const char* RoleName(Role role) {
    switch (role) {
        case Role::User: return "User";
        case Role::Staff: return "Staff";
        case Role::Owner: return "Owner";
    }
    return "User";
}

const char* RoleBadge(Role role) {
    switch (role) {
        case Role::User: return "USER";
        case Role::Staff: return "STAFF";
        case Role::Owner: return "OWNER";
    }
    return "USER";
}

const std::vector<DurationOption>& DurationOptions() {
    static const std::vector<DurationOption> kOptions = {
        {"Never expires", 0}, {"24 hours", 1},  {"7 days", 7},   {"14 days", 14},
        {"30 days", 30},      {"90 days", 90},  {"180 days", 180}, {"1 year", 365},
    };
    return kOptions;
}

int DurationDaysFromIndex(int index) {
    const auto& options = DurationOptions();
    if (index < 0 || index >= static_cast<int>(options.size())) return 0;
    return options[static_cast<std::size_t>(index)].days;
}

int DurationIndexFromDays(int days) {
    const auto& options = DurationOptions();
    for (std::size_t i = 0; i < options.size(); ++i) {
        if (options[i].days == days) return static_cast<int>(i);
    }
    return 4;  // fall back to 30 days
}

std::string DurationLabel(int days) { return DurationOptions()[static_cast<std::size_t>(DurationIndexFromDays(days))].label; }

std::string AccessCode::Description() const {
    std::string when;
    if (neverExpires) {
        when = "never expires";
    } else {
        when = "expires " + time::FormatUtc(expiryUtc).substr(0, 10);
    }
    return std::string(RoleName(payload.role)) + " access - " + when;
}

const char* CodeErrorMessage(CodeError error) {
    switch (error) {
        case CodeError::None: return "Code accepted";
        case CodeError::Empty: return "Enter your access code to continue";
        case CodeError::Malformed: return "That code is incomplete - it needs 16 characters";
        case CodeError::BadSignature: return "This access code is not valid";
        case CodeError::WrongVersion: return "This code was made for a newer Astra release";
        case CodeError::NotYetValid: return "This code is not active yet";
        case CodeError::Expired: return "This access code has expired";
        case CodeError::AlreadyUsed: return "This access code has already been used";
        case CodeError::RoleNotAllowed: return "This code cannot register an engine account";
    }
    return "Unknown error";
}

const char* CodeErrorHint(CodeError error) {
    switch (error) {
        case CodeError::None: return "";
        case CodeError::Empty: return "Codes look like K7QM-3XPD-9RTA-B2VH.";
        case CodeError::Malformed: return "Check for a missing or extra group.";
        case CodeError::BadSignature:
            return "Double-check each group. Codes are issued by Astra KeyForge.";
        case CodeError::WrongVersion: return "Update Astra Studio, or ask your owner for a new code.";
        case CodeError::NotYetValid: return "Ask your owner when this code was scheduled to start.";
        case CodeError::Expired: return "Request a fresh code from your studio owner.";
        case CodeError::AlreadyUsed: return "Each code registers exactly one account.";
        case CodeError::RoleNotAllowed: return "Staff codes must be redeemed in KeyForge.";
    }
    return "";
}

std::string FormatCodeForInput(std::string_view raw) {
    const std::string normalized = crypto::NormalizeCode(raw);
    std::string clipped = normalized.size() > static_cast<std::size_t>(kCodeChars)
                              ? normalized.substr(0, kCodeChars)
                              : normalized;
    return crypto::GroupWithDashes(clipped, 4);
}

bool IsCompleteCode(std::string_view raw) {
    return crypto::NormalizeCode(raw).size() == static_cast<std::size_t>(kCodeChars);
}

std::int64_t IssuedUtcFromDay(std::int64_t day, std::int64_t /*nowUtc*/) {
    // The 12-bit field stores whole days since the licensing epoch, so codes carry a
    // day-accurate issue date rather than a full timestamp (keeps them typeable).
    return kEpochUtc + day * kSecondsPerDay;
}

std::int64_t DayFromUtc(std::int64_t unixSeconds) {
    return time::DaysSinceEpoch(unixSeconds) - time::DaysSinceEpoch(kEpochUtc);
}

std::int64_t ExpiryUtc(const CodePayload& payload, std::int64_t nowUtc) {
    if (payload.durationDays <= 0) return 0;
    const std::int64_t issued = IssuedUtcFromDay(payload.issuedDay, nowUtc);
    return issued + (static_cast<std::int64_t>(payload.durationDays) + 1) * kSecondsPerDay - 1;
}

AccessCode IssueCode(const CodePayload& payload, const Bytes& secret) {
    CodePayload normalized = payload;
    normalized.version = normalized.version == 0 ? 1 : normalized.version;

    u8 payloadBytes[kCodePayloadBytes]{};
    WritePayload(normalized, payloadBytes);
    const auto tag = ComputeTag(payloadBytes, secret);

    u8 all[kCodeTotalBytes]{};
    for (int i = 0; i < kCodePayloadBytes; ++i) all[i] = payloadBytes[i];
    for (int i = 0; i < kCodeTagBytes; ++i) all[kCodePayloadBytes + i] = tag[static_cast<std::size_t>(i)];

    AccessCode code;
    code.text = crypto::GroupWithDashes(crypto::Base32Encode(all, sizeof(all)), 4);
    code.payload = normalized;
    const std::int64_t nowUtc = time::NowUtc();
    code.issuedUtc = IssuedUtcFromDay(normalized.issuedDay, nowUtc);
    code.neverExpires = normalized.durationDays <= 0;
    code.expiryUtc = code.neverExpires ? 0 : ExpiryUtc(normalized, nowUtc);
    return code;
}

CodeResult VerifyCode(std::string_view text, const Bytes& secret, std::int64_t nowUtc) {
    CodeResult result;
    const std::string normalized = crypto::NormalizeCode(text);
    if (normalized.empty()) {
        result.error = CodeError::Empty;
        return result;
    }
    if (normalized.size() != static_cast<std::size_t>(kCodeChars)) {
        result.error = CodeError::Malformed;
        return result;
    }

    Bytes raw;
    if (!crypto::Base32Decode(normalized, raw) || raw.size() < static_cast<std::size_t>(kCodeTotalBytes)) {
        result.error = CodeError::Malformed;
        return result;
    }
    raw.resize(static_cast<std::size_t>(kCodeTotalBytes));

    u8 payloadBytes[kCodePayloadBytes]{};
    for (int i = 0; i < kCodePayloadBytes; ++i) payloadBytes[i] = raw[static_cast<std::size_t>(i)];

    const auto expected = ComputeTag(payloadBytes, secret);
    if (!crypto::ConstantTimeEquals(ByteView(expected.data(), expected.size()),
                                    ByteView(raw.data() + kCodePayloadBytes, kCodeTagBytes))) {
        result.error = CodeError::BadSignature;
        return result;
    }

    CodePayload payload;
    if (!ReadPayload(payloadBytes, payload)) {
        result.error = CodeError::BadSignature;
        return result;
    }
    if (payload.version != 1) {
        result.error = CodeError::WrongVersion;
        return result;
    }

    AccessCode code;
    code.text = crypto::GroupWithDashes(normalized, 4);
    code.payload = payload;
    code.issuedUtc = IssuedUtcFromDay(payload.issuedDay, nowUtc);
    code.neverExpires = payload.durationDays <= 0;
    code.expiryUtc = code.neverExpires ? 0 : ExpiryUtc(payload, nowUtc);

    // Codes become valid at the start of their issue day; allow a day of clock skew
    // forward so a machine slightly ahead of the issuer does not reject a fresh code.
    if (code.issuedUtc > nowUtc + kSecondsPerDay) {
        result.error = CodeError::NotYetValid;
        return result;
    }
    if (!code.neverExpires && nowUtc > code.expiryUtc) {
        result.error = CodeError::Expired;
        return result;
    }

    result.code = std::move(code);
    result.error = CodeError::None;
    return result;
}

}  // namespace astra::license
