// auth/Account.h - local engine account registry (users and staff).
#pragma once

#include <string>
#include <string_view>
#include <vector>
#include <cstdint>
#include <utility>

#include "astra/core/Types.h"
#include "astra/licensing/AccessCode.h"

namespace astra::auth {

struct Account {
    std::string id;            // "acc_9f3c1a7b2e5d"
    std::string username;      // unique, lower-case, login handle
    std::string displayName;   // shown in the UI
    std::string email;         // optional
    license::Role role = license::Role::User;
    std::string saltHex;       // 16 random bytes
    std::string hashHex;       // PBKDF2-HMAC-SHA256(password, salt, iterations)
    int iterations = 120000;
    std::int64_t createdUtc = 0;
    std::int64_t lastLoginUtc = 0;
    int signInCount = 0;
    std::string accessCodeMasked;  // "K7QM-****-****-B2VH" for support
    std::string accentHex;         // avatar colour, chosen at registration

    bool IsStaff() const { return role != license::Role::User; }
    std::string Initials() const;
};

struct PasswordPolicy {
    std::size_t minLength = 10;
    std::size_t staffMinLength = 12;
    bool requireLetterAndDigit = true;
};

// 0..4 (weak..strong) plus a short label, for the registration meter.
struct PasswordStrength {
    int score = 0;
    std::string label;
    std::string hint;
};
PasswordStrength EvaluatePassword(std::string_view password, license::Role role,
                                  const PasswordPolicy& policy = {});

struct ValidationResult {
    bool ok = true;
    std::string message;
    static ValidationResult Ok() { return {}; }
    static ValidationResult Fail(std::string message) { return {false, std::move(message)}; }
};

ValidationResult ValidateUsername(std::string_view username);
ValidationResult ValidateDisplayName(std::string_view displayName);
ValidationResult ValidateEmail(std::string_view email);
ValidationResult ValidatePassword(std::string_view password, license::Role role,
                                  const PasswordPolicy& policy = {});

// Password hashing helpers (PBKDF2-HMAC-SHA256).
std::string NewSaltHex();
std::string HashPassword(std::string_view password, std::string_view saltHex, int iterations);
bool VerifyPasswordHash(std::string_view password, std::string_view saltHex,
                        std::string_view expectedHashHex, int iterations);

// Random avatar accent from the Astra palette.
std::string PickAccentHex();

class AccountStore {
public:
    explicit AccountStore(std::string path);

    static AccountStore ForEngine();

    // Loads accounts.json; a missing file is not an error (empty registry).
    bool Load();
    bool Save() const;

    const std::vector<Account>& Accounts() const { return accounts_; }
    const Account* FindById(std::string_view id) const;
    const Account* FindByUsername(std::string_view username) const;
    const Account* FindByEmail(std::string_view email) const;

    // Creates the account (fails on duplicate username/email) and persists.
    bool Add(Account account, std::string* error);
    bool UpdateLastLogin(std::string_view accountId, std::int64_t nowUtc);
    bool UpdatePassword(std::string_view accountId, std::string_view password);
    bool Remove(std::string_view accountId);

    const std::string& Path() const { return path_; }
    std::string LastError() const { return lastError_; }

    static Account CreateAccount(std::string username, std::string displayName, std::string password,
                                 license::Role role, std::string email = {},
                                 std::string accessCodeMasked = {});

private:
    std::string path_;
    std::vector<Account> accounts_;
    mutable std::string lastError_;
};

}  // namespace astra::auth
