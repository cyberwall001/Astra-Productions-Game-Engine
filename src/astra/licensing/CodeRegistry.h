// licensing/CodeRegistry.h - local single-use enforcement plus the owner's issue ledger.
//
// Single-use is enforced per installation (Studio) and the ledger lives with KeyForge.
// A global "used by anyone, once" guarantee requires the licensing service planned for
// v0.2; see docs/SECURITY.md. Everything here is expressed so that swapping the local
// registry for a remote one only touches this file.
#pragma once

#include <string>
#include <string_view>
#include <vector>

#include "astra/core/Types.h"
#include "astra/licensing/AccessCode.h"

namespace astra::license {

// SHA-256 of the normalized (dash-free, upper-case) code. Only hashes are ever stored.
std::string CodeHash(std::string_view normalizedCode);

// "K7QM-****-****-B2VH" - safe for logs and support tickets.
std::string MaskCode(std::string_view normalizedCode);

struct UsedCode {
    std::string hash;
    std::string masked;
    std::string accountId;
    std::int64_t usedUtc = 0;
};

// Tracks which codes this installation has already redeemed.
class CodeRegistry {
public:
    explicit CodeRegistry(std::string path);

    static CodeRegistry ForEngine();  // <DataRoot>/engine/used_codes.json

    bool Load();
    bool Save() const;

    bool IsUsed(std::string_view normalizedCode) const;
    bool MarkUsed(std::string_view normalizedCode, std::string_view accountId, std::int64_t nowUtc);
    const std::vector<UsedCode>& Used() const { return used_; }
    const std::string& Path() const { return path_; }

private:
    std::string path_;
    std::vector<UsedCode> used_;
};

// Owner-side record of every code KeyForge has minted.
struct IssuedCode {
    std::string hash;
    std::string masked;
    std::string code;      // full code, kept locally so the owner can re-copy it
    std::string role;      // "User" / "Staff" / "Owner"
    std::string duration;  // human label
    std::string note;
    std::string batch;
    std::int64_t issuedUtc = 0;
    std::int64_t expiryUtc = 0;
    bool neverExpires = false;
    bool revoked = false;
};

class IssuedLedger {
public:
    IssuedLedger();  // defaults to the KeyForge ledger path
    explicit IssuedLedger(std::string path);

    static IssuedLedger ForKeyForge();

    bool Load();
    bool Save() const;

    void Add(const IssuedCode& record);
    bool Revoke(std::string_view hash);
    const std::vector<IssuedCode>& Codes() const { return codes_; }
    std::vector<IssuedCode> Active() const;
    const std::string& Path() const { return path_; }

private:
    std::string path_;
    std::vector<IssuedCode> codes_;
};

}  // namespace astra::license
