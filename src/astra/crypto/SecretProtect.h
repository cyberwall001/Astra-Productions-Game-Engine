// crypto/SecretProtect.h - at-rest protection for the stored GitHub token.
//
// Windows: DPAPI (CryptProtectData) bound to the current user account.
// Other platforms: clearly marked reversible encoding - the engine only stores
// secrets there for development tooling, never for shipped Windows builds.
#pragma once

#include <string>

namespace astra::crypto {

std::string ProtectSecret(const std::string& plaintext);
std::string UnprotectSecret(const std::string& protectedText);
bool IsSecureProtectionAvailable();

}  // namespace astra::crypto
