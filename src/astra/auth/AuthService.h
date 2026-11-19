// auth/AuthService.h - the single entry point the UI uses for sign-in, registration
// and access-code validation.
#pragma once

#include <map>
#include <string>
#include <vector>

#include "astra/auth/Account.h"
#include "astra/auth/Session.h"
#include "astra/licensing/AccessCode.h"
#include "astra/licensing/CodeRegistry.h"

namespace astra::auth {

struct SignInResult {
    bool ok = false;
    std::string error;
    std::string hint;
    const Account* account = nullptr;
};

struct RegisterResult {
    bool ok = false;
    std::string error;
    std::string hint;
    std::string accountId;
};

class AuthService {
public:
    AuthService();

    // Loads accounts.json, used_codes.json and session.json. Safe to call twice.
    void Initialize();

    // ---- registry ----
    AccountStore& Accounts() { return accounts_; }
    const AccountStore& Accounts() const { return accounts_; }

    // Accounts ordered by most recent sign-in, for the saved-account chips.
    std::vector<const Account*> RecentAccounts() const;
    std::size_t AccountCount() const { return accounts_.Accounts().size(); }

    // ---- session ----
    const Account* Current() const;
    bool IsSignedIn() const { return Current() != nullptr; }
    std::string CurrentAccountId() const;

    SignInResult SignIn(std::string_view usernameOrEmail, std::string_view password, bool remember);
    // Signs in using the saved remember-me token. Returns ok=false when there is none.
    SignInResult RestoreSession();
    void SignOut();
    const SessionData& Session() const { return session_.Data(); }

    // ---- registration ----
    // Verifies the access code (signature, window, single use) without consuming it.
    license::CodeResult CheckAccessCode(std::string_view codeText) const;

    // Redeems `codeText` and creates the account in one atomic step.
    RegisterResult Register(std::string_view codeText, std::string_view username,
                            std::string_view displayName, std::string_view password,
                            std::string_view confirmPassword, std::string_view email,
                            bool acceptedTerms);

    // Persists the session and marks the account as signed in.
    void EstablishSession(const Account& account, bool remember);

private:
    struct Throttle {
        int failures = 0;
        double lockedUntilMs = 0.0;
    };

    AccountStore accounts_;
    license::CodeRegistry registry_;
    SessionStore session_;
    std::string currentAccountId_;
    std::map<std::string, Throttle> throttles_;
};

}  // namespace astra::auth
