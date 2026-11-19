#include "astra/auth/AuthService.h"

#include <algorithm>

#include "astra/core/Log.h"
#include "astra/core/Strings.h"
#include "astra/core/Time.h"
#include "astra/crypto/Encoding.h"
#include "astra/crypto/Random.h"
#include "astra/licensing/MasterKey.h"

namespace astra::auth {
namespace {

constexpr int kMaxFailures = 5;
constexpr double kLockoutMs = 30.0 * 1000.0;

std::string LowerAscii(std::string_view text) { return str::ToLower(text); }

}  // namespace

AuthService::AuthService()
    : accounts_(AccountStore::ForEngine()),
      registry_(license::CodeRegistry::ForEngine()),
      session_(SessionStore::ForEngine()) {}

void AuthService::Initialize() {
    accounts_.Load();
    registry_.Load();
    session_.Load();
    log::Info("auth", str::Format("account registry: %zu account(s)", accounts_.Accounts().size()));
}

std::vector<const Account*> AuthService::RecentAccounts() const {
    std::vector<const Account*> out;
    for (const auto& account : accounts_.Accounts()) out.push_back(&account);
    std::sort(out.begin(), out.end(), [](const Account* a, const Account* b) {
        if (a->lastLoginUtc != b->lastLoginUtc) return a->lastLoginUtc > b->lastLoginUtc;
        return a->username < b->username;
    });
    return out;
}

const Account* AuthService::Current() const {
    if (currentAccountId_.empty()) return nullptr;
    return accounts_.FindById(currentAccountId_);
}

std::string AuthService::CurrentAccountId() const { return currentAccountId_; }

void AuthService::EstablishSession(const Account& account, bool remember) {
    currentAccountId_ = account.id;
    accounts_.UpdateLastLogin(account.id, time::NowUtc());

    SessionData data;
    data.remember = remember;
    if (remember) {
        data.accountId = account.id;
        data.token = crypto::RandomHex(24);
        data.issuedUtc = time::NowUtc();
        data.expiresUtc = data.issuedUtc + SessionStore::kRememberSeconds;
    }
    session_.Mutable() = data;
    session_.Save();

    log::Info("auth", str::Format("%s signed in as %s", account.displayName.c_str(),
                                  license::RoleName(account.role)));
}

SignInResult AuthService::SignIn(std::string_view usernameOrEmail, std::string_view password,
                                 bool remember) {
    SignInResult result;
    const std::string key = LowerAscii(str::Trim(usernameOrEmail));
    if (key.empty()) {
        result.error = "Enter your username";
        return result;
    }
    if (password.empty()) {
        result.error = "Enter your password";
        return result;
    }

    auto& throttle = throttles_[key];
    const double now = time::NowMonotonicMs();
    if (throttle.lockedUntilMs > now) {
        const int seconds = static_cast<int>((throttle.lockedUntilMs - now) / 1000.0) + 1;
        result.error = str::Format("Too many attempts - try again in %ds", seconds);
        result.hint = "Astra locks sign-in for 30 seconds after 5 wrong passwords.";
        return result;
    }

    const Account* account = accounts_.FindByUsername(key);
    if (!account) account = accounts_.FindByEmail(key);
    if (!account) {
        // Same message for unknown user and bad password: do not leak which usernames exist.
        result.error = "That username and password do not match";
        throttle.failures += 1;
        if (throttle.failures >= kMaxFailures) throttle.lockedUntilMs = now + kLockoutMs;
        return result;
    }

    if (!VerifyPasswordHash(password, account->saltHex, account->hashHex, account->iterations)) {
        result.error = "That username and password do not match";
        result.hint = str::Format("%d of %d attempts before lockout", throttle.failures + 1,
                                  kMaxFailures);
        throttle.failures += 1;
        if (throttle.failures >= kMaxFailures) {
            throttle.lockedUntilMs = now + kLockoutMs;
            log::Warn("auth", "sign-in throttled for " + key);
        }
        return result;
    }

    throttle.failures = 0;
    throttle.lockedUntilMs = 0.0;
    EstablishSession(*account, remember);
    result.ok = true;
    result.account = Current();
    return result;
}

SignInResult AuthService::RestoreSession() {
    SignInResult result;
    const SessionData& data = session_.Data();
    if (!data.IsValid(time::NowUtc())) {
        result.error = "No saved session";
        return result;
    }
    const Account* account = accounts_.FindById(data.accountId);
    if (!account) {
        session_.Clear();
        result.error = "The saved account no longer exists";
        return result;
    }
    currentAccountId_ = account->id;
    result.ok = true;
    result.account = account;
    log::Info("auth", "restored saved session for " + account->username);
    return result;
}

void AuthService::SignOut() {
    if (const Account* account = Current()) {
        log::Info("auth", "signed out " + account->username);
    }
    currentAccountId_.clear();
    session_.Clear();
}

license::CodeResult AuthService::CheckAccessCode(std::string_view codeText) const {
    const Bytes secret = license::MasterKeyBytes();
    license::CodeResult result = license::VerifyCode(codeText, secret, time::NowUtc());
    if (!result.ok()) return result;

    const std::string normalized = crypto::NormalizeCode(codeText);
    if (registry_.IsUsed(normalized)) {
        result.error = license::CodeError::AlreadyUsed;
        result.code = license::AccessCode{};
        return result;
    }
    return result;
}

RegisterResult AuthService::Register(std::string_view codeText, std::string_view username,
                                     std::string_view displayName, std::string_view password,
                                     std::string_view confirmPassword, std::string_view email,
                                     bool acceptedTerms) {
    RegisterResult result;
    if (!acceptedTerms) {
        result.error = "Please accept the Astra Productions terms to continue";
        return result;
    }

    license::CodeResult code = CheckAccessCode(codeText);
    if (!code.ok()) {
        result.error = license::CodeErrorMessage(code.error);
        result.hint = license::CodeErrorHint(code.error);
        return result;
    }
    if (code.code.payload.role == license::Role::Owner) {
        result.error = "Owner codes cannot register a studio account";
        result.hint = "Owner codes are for Astra internal tooling only.";
        return result;
    }

    auto invalid = [&](const ValidationResult& validation) {
        result.error = validation.message;
        return result;
    };
    if (auto v = ValidateDisplayName(displayName); !v.ok) return invalid(v);
    if (auto v = ValidateUsername(username); !v.ok) return invalid(v);
    if (auto v = ValidateEmail(str::Trim(email)); !v.ok) return invalid(v);
    if (auto v = ValidatePassword(password, code.code.payload.role); !v.ok) return invalid(v);
    if (password != confirmPassword) {
        result.error = "Those passwords do not match";
        return result;
    }
    if (accounts_.FindByUsername(username)) {
        result.error = "That username is already registered";
        result.hint = "Pick another handle, or sign in with the existing account.";
        return result;
    }

    const std::string masked = license::MaskCode(crypto::NormalizeCode(codeText));
    Account account = AccountStore::CreateAccount(std::string(username), std::string(displayName),
                                                  std::string(password), code.code.payload.role,
                                                  str::Trim(email), masked);

    std::string error;
    const std::string accountId = account.id;
    if (!accounts_.Add(std::move(account), &error)) {
        result.error = error;
        return result;
    }

    // Consume the code only after the account exists, so a failed write never burns a code.
    if (!registry_.MarkUsed(crypto::NormalizeCode(codeText), accountId, time::NowUtc())) {
        log::Warn("auth", "account created but access code could not be marked used");
    }
    log::Info("auth", str::Format("registered %s (%s) with code %s",
                                  std::string(displayName).c_str(),
                                  license::RoleName(code.code.payload.role), masked.c_str()));

    result.ok = true;
    result.accountId = accountId;
    if (const Account* created = accounts_.FindById(accountId)) {
        EstablishSession(*created, true);
    }
    return result;
}

}  // namespace astra::auth
