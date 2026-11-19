#include "astra/auth/Account.h"

#include <algorithm>
#include <cctype>
#include <cmath>

#include "astra/core/FileIO.h"
#include "astra/core/Log.h"
#include "astra/core/Paths.h"
#include "astra/core/Strings.h"
#include "astra/core/Time.h"
#include "astra/core/Version.h"
#include "astra/crypto/Encoding.h"
#include "astra/crypto/Random.h"
#include "astra/crypto/Sha256.h"
#include "astra/data/Json.h"

namespace astra::auth {
namespace {

constexpr int kSchema = kAccountSchemaVersion;
constexpr std::size_t kSaltBytes = 16;
constexpr int kDefaultIterations = 120000;

const char* kAccents[] = {"F26B5E", "F09A5B", "E4C05E", "7FC8A9",
                          "6FA8DC", "A98BE0", "E07BA8", "67C5C0"};

std::string LowerAscii(std::string_view text) { return str::ToLower(text); }

bool IsAlnum(char c) { return std::isalnum(static_cast<unsigned char>(c)) != 0; }

bool IsUsernameChar(char c) {
    return IsAlnum(c) || c == '_' || c == '-' || c == '.';
}

}  // namespace

std::string Account::Initials() const { return str::Initials(displayName.empty() ? username : displayName); }

PasswordStrength EvaluatePassword(std::string_view password, license::Role role,
                                  const PasswordPolicy& policy) {
    PasswordStrength strength;
    bool hasLower = false, hasUpper = false, hasDigit = false, hasSymbol = false;
    for (char c : password) {
        const unsigned char u = static_cast<unsigned char>(c);
        if (std::islower(u)) hasLower = true;
        else if (std::isupper(u)) hasUpper = true;
        else if (std::isdigit(u)) hasDigit = true;
        else hasSymbol = true;
    }

    int score = 0;
    if (password.size() >= (role == license::Role::User ? policy.minLength : policy.staffMinLength)) ++score;
    if (password.size() >= 14) ++score;
    if (hasLower && hasUpper) ++score;
    if (hasDigit) ++score;
    if (hasSymbol) ++score;

    // Normalise the 0..5 raw score onto 0..4.
    score = std::min(4, score);

    static const char* kLabels[] = {"Too short", "Weak", "Fair", "Good", "Strong"};
    static const char* kHints[] = {
        "Use at least 10 characters.",
        "Add a few more characters.",
        "Mix upper and lower case, plus a digit.",
        "Add a symbol for a stronger password.",
        "Nice - this one will hold up.",
    };
    strength.score = score;
    strength.label = kLabels[score];
    strength.hint = kHints[score];
    return strength;
}

ValidationResult ValidateUsername(std::string_view username) {
    if (username.size() < 3) return ValidationResult::Fail("Usernames need at least 3 characters");
    if (username.size() > 24) return ValidationResult::Fail("Usernames are limited to 24 characters");
    for (char c : username) {
        if (!IsUsernameChar(c)) {
            return ValidationResult::Fail("Use letters, numbers, dots, dashes or underscores");
        }
    }
    if (!IsAlnum(username.front())) return ValidationResult::Fail("Usernames must start with a letter or number");
    return ValidationResult::Ok();
}

ValidationResult ValidateDisplayName(std::string_view displayName) {
    if (str::Trim(displayName).size() < 2) return ValidationResult::Fail("Tell us what to call you");
    if (displayName.size() > 48) return ValidationResult::Fail("Display names are limited to 48 characters");
    return ValidationResult::Ok();
}

ValidationResult ValidateEmail(std::string_view email) {
    if (email.empty()) return ValidationResult::Ok();
    const std::size_t at = email.find('@');
    if (at == std::string_view::npos || at == 0 || at + 2 > email.size()) {
        return ValidationResult::Fail("That email address looks incomplete");
    }
    if (email.find('.', at) == std::string_view::npos) {
        return ValidationResult::Fail("That email address looks incomplete");
    }
    if (email.find(' ') != std::string_view::npos) return ValidationResult::Fail("Email cannot contain spaces");
    return ValidationResult::Ok();
}

ValidationResult ValidatePassword(std::string_view password, license::Role role,
                                  const PasswordPolicy& policy) {
    const std::size_t minimum =
        role == license::Role::User ? policy.minLength : policy.staffMinLength;
    if (password.size() < minimum) {
        return ValidationResult::Fail(str::Format(
            "%s passwords need at least %zu characters",
            role == license::Role::User ? "User" : "Staff", minimum));
    }
    if (password.size() > 200) return ValidationResult::Fail("Passwords are limited to 200 characters");
    if (policy.requireLetterAndDigit) {
        bool hasLetter = false, hasDigit = false;
        for (char c : password) {
            const unsigned char u = static_cast<unsigned char>(c);
            if (std::isalpha(u)) hasLetter = true;
            if (std::isdigit(u)) hasDigit = true;
        }
        if (!hasLetter || !hasDigit) {
            return ValidationResult::Fail("Mix in at least one letter and one number");
        }
    }
    return ValidationResult::Ok();
}

std::string NewSaltHex() { return crypto::RandomHex(kSaltBytes); }

std::string HashPassword(std::string_view password, std::string_view saltHex, int iterations) {
    Bytes salt;
    if (!crypto::HexDecode(saltHex, salt)) salt = crypto::SecureRandomBytes(kSaltBytes);
    const auto derived = crypto::Pbkdf2Sha256(password, ByteView(salt),
                                             static_cast<u32>(iterations <= 0 ? kDefaultIterations : iterations));
    return crypto::HexEncode(derived.data(), derived.size());
}

bool VerifyPasswordHash(std::string_view password, std::string_view saltHex,
                        std::string_view expectedHashHex, int iterations) {
    const std::string computed = HashPassword(password, saltHex, iterations);
    return crypto::ConstantTimeEquals(ByteView(computed), ByteView(std::string(expectedHashHex)));
}

std::string PickAccentHex() {
    const Bytes bytes = crypto::SecureRandomBytes(1);
    return kAccents[bytes[0] % (sizeof(kAccents) / sizeof(kAccents[0]))];
}

// ---------------------------------------------------------------------------
// AccountStore
// ---------------------------------------------------------------------------

AccountStore::AccountStore(std::string path) : path_(std::move(path)) {}

AccountStore AccountStore::ForEngine() {
    return AccountStore(paths::Join(paths::EngineDataDir(), "accounts.json"));
}

bool AccountStore::Load() {
    accounts_.clear();
    lastError_.clear();
    std::string text;
    if (!fs::ReadTextFile(path_, text)) return false;

    json::Value root;
    std::string error;
    if (!json::Value::Parse(text, root, &error)) {
        lastError_ = "accounts.json could not be parsed: " + error;
        log::Error("auth", lastError_);
        return false;
    }

    for (const auto& item : root["accounts"].Elements()) {
        Account account;
        account.id = item["id"].AsString();
        account.username = item["username"].AsString();
        account.displayName = item["display"].AsString();
        account.email = item["email"].AsString();
        const std::string role = item["role"].AsString("user");
        account.role = role == "staff" ? license::Role::Staff
                                       : (role == "owner" ? license::Role::Owner : license::Role::User);
        account.saltHex = item["salt"].AsString();
        account.hashHex = item["hash"].AsString();
        account.iterations = static_cast<int>(item["iterations"].AsInt(kDefaultIterations));
        account.createdUtc = item["created"].AsInt();
        account.lastLoginUtc = item["lastLogin"].AsInt();
        account.signInCount = static_cast<int>(item["signIns"].AsInt());
        account.accessCodeMasked = item["accessCode"].AsString();
        account.accentHex = item["accent"].AsString("F26B5E");
        if (!account.id.empty() && !account.username.empty()) {
            accounts_.push_back(std::move(account));
        }
    }
    return true;
}

bool AccountStore::Save() const {
    json::Value root = json::Value::MakeObject();
    root.Set("schema", json::Value(kSchema));
    root.Set("engineVersion", json::Value(kEngineVersion));
    json::Value list = json::Value::MakeArray();
    for (const auto& account : accounts_) {
        json::Value item = json::Value::MakeObject();
        item.Set("id", json::Value(account.id));
        item.Set("username", json::Value(account.username));
        item.Set("display", json::Value(account.displayName));
        item.Set("email", json::Value(account.email));
        const char* role = account.role == license::Role::Staff
                               ? "staff"
                               : (account.role == license::Role::Owner ? "owner" : "user");
        item.Set("role", json::Value(role));
        item.Set("salt", json::Value(account.saltHex));
        item.Set("hash", json::Value(account.hashHex));
        item.Set("iterations", json::Value(account.iterations));
        item.Set("created", json::Value(account.createdUtc));
        item.Set("lastLogin", json::Value(account.lastLoginUtc));
        item.Set("signIns", json::Value(account.signInCount));
        item.Set("accessCode", json::Value(account.accessCodeMasked));
        item.Set("accent", json::Value(account.accentHex));
        list.Push(std::move(item));
    }
    root.Set("accounts", std::move(list));

    // Registered accounts hold credential material - restrict to the current user.
    fs::CreateDirectories(paths::EngineDataDir());
    const bool ok = fs::WriteTextFileAtomic(path_, root.Dump(true));
    if (!ok) lastError_ = "Could not write " + path_;
    return ok;
}

const Account* AccountStore::FindById(std::string_view id) const {
    for (const auto& account : accounts_) {
        if (account.id == id) return &account;
    }
    return nullptr;
}

const Account* AccountStore::FindByUsername(std::string_view username) const {
    const std::string lowered = LowerAscii(username);
    for (const auto& account : accounts_) {
        if (LowerAscii(account.username) == lowered) return &account;
    }
    return nullptr;
}

const Account* AccountStore::FindByEmail(std::string_view email) const {
    if (email.empty()) return nullptr;
    const std::string lowered = LowerAscii(email);
    for (const auto& account : accounts_) {
        if (!account.email.empty() && LowerAscii(account.email) == lowered) return &account;
    }
    return nullptr;
}

bool AccountStore::Add(Account account, std::string* error) {
    if (FindByUsername(account.username)) {
        if (error) *error = "That username is already registered on this machine";
        return false;
    }
    if (!account.email.empty() && FindByEmail(account.email)) {
        if (error) *error = "That email is already registered on this machine";
        return false;
    }
    accounts_.push_back(std::move(account));
    if (!Save()) {
        if (error) *error = lastError_.empty() ? "Could not save the account registry" : lastError_;
        accounts_.pop_back();
        return false;
    }
    return true;
}

bool AccountStore::UpdateLastLogin(std::string_view accountId, std::int64_t nowUtc) {
    for (auto& account : accounts_) {
        if (account.id == accountId) {
            account.lastLoginUtc = nowUtc;
            account.signInCount += 1;
            return Save();
        }
    }
    return false;
}

bool AccountStore::UpdatePassword(std::string_view accountId, std::string_view password) {
    for (auto& account : accounts_) {
        if (account.id == accountId) {
            account.saltHex = NewSaltHex();
            account.hashHex = HashPassword(password, account.saltHex, account.iterations);
            return Save();
        }
    }
    return false;
}

bool AccountStore::Remove(std::string_view accountId) {
    const auto before = accounts_.size();
    accounts_.erase(std::remove_if(accounts_.begin(), accounts_.end(),
                                   [&](const Account& a) { return a.id == accountId; }),
                    accounts_.end());
    return accounts_.size() != before && Save();
}

Account AccountStore::CreateAccount(std::string username, std::string displayName,
                                    std::string password, license::Role role, std::string email,
                                    std::string accessCodeMasked) {
    Account account;
    account.username = LowerAscii(str::Trim(username));
    account.displayName = str::Trim(displayName);
    account.email = str::Trim(email);
    account.role = role;
    account.saltHex = NewSaltHex();
    account.iterations = kDefaultIterations;
    account.hashHex = HashPassword(password, account.saltHex, account.iterations);
    account.createdUtc = time::NowUtc();
    account.accessCodeMasked = std::move(accessCodeMasked);
    account.accentHex = PickAccentHex();
    account.id = "acc_" + crypto::RandomHex(6);
    return account;
}

}  // namespace astra::auth
