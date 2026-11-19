// crypto/Sha256.h - FIPS 180-4 SHA-256 and HMAC-SHA256 (no external dependencies).
#pragma once

#include <array>
#include <string>

#include "astra/core/Types.h"

namespace astra::crypto {

class Sha256 {
public:
    static constexpr std::size_t kDigestSize = 32;
    static constexpr std::size_t kBlockSize = 64;

    Sha256();
    void Update(const void* data, std::size_t size);
    void Update(ByteView view) { Update(view.data, view.size); }
    void Update(std::string_view text) { Update(text.data(), text.size()); }
    // Finalizes the digest. The object must be re-created to hash again.
    void Final(u8 out[kDigestSize]);

    static std::array<u8, kDigestSize> Digest(ByteView data);
    static std::string HexDigest(ByteView data);
    static std::string HexDigest(std::string_view text);

private:
    void Transform(const u8 block[kBlockSize]);

    u32 state_[8];
    u64 bitCount_ = 0;
    u8 buffer_[kBlockSize]{};
    std::size_t bufferSize_ = 0;
};

// HMAC-SHA256 over arbitrary data; returns the raw 32-byte tag.
std::array<u8, 32> HmacSha256(ByteView key, ByteView message);
std::array<u8, 32> HmacSha256(ByteView key, std::string_view message);

// PBKDF2-HMAC-SHA256. Output length is fixed at 32 bytes (one block), which is all
// we need for password storage.
std::array<u8, 32> Pbkdf2Sha256(std::string_view password, ByteView salt, u32 iterations);

// Constant-time comparison; use for every secret/tag check.
bool ConstantTimeEquals(ByteView a, ByteView b);

}  // namespace astra::crypto
