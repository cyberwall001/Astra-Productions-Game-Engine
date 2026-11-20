// crypto/Random.h - OS-backed cryptographic randomness with a deterministic test mode.
#pragma once

#include <string>

#include "astra/core/Types.h"

namespace astra::crypto {

// Fills `out` with cryptographically secure random bytes.
// Windows: BCryptGenRandom, Linux: getrandom() with /dev/urandom fallback.
void SecureRandom(void* out, std::size_t size);

Bytes SecureRandomBytes(std::size_t size);

// Hex string of `bytes` random bytes (2 * bytes characters).
std::string RandomHex(std::size_t bytes);

// Deterministic generator for unit tests only. Never enable in a shipped build.
void SetDeterministicSeedForTesting(unsigned seed);  // 0 disables

}  // namespace astra::crypto
