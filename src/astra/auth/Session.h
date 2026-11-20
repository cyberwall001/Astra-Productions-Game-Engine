// auth/Session.h - "remember me" state so the launcher can skip the sign-in screen.
#pragma once

#include <string>
#include <cstdint>

namespace astra::auth {

struct SessionData {
    bool remember = false;
    std::string accountId;
    std::string token;      // random opaque value, rotated on every sign-in
    std::int64_t issuedUtc = 0;
    std::int64_t expiresUtc = 0;

    bool IsValid(std::int64_t nowUtc) const {
        return remember && !accountId.empty() && !token.empty() && expiresUtc > nowUtc;
    }
    void Clear() { *this = SessionData{}; }
};

class SessionStore {
public:
    explicit SessionStore(std::string path);

    static SessionStore ForEngine();

    bool Load();
    bool Save() const;
    bool Clear();

    const SessionData& Data() const { return data_; }
    SessionData& Mutable() { return data_; }

    // Default remember window.
    static constexpr std::int64_t kRememberSeconds = 60 * 60 * 24 * 30;  // 30 days

    const std::string& Path() const { return path_; }

private:
    std::string path_;
    SessionData data_;
};

}  // namespace astra::auth
