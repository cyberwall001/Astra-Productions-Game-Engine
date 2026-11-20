#include "astra/licensing/MasterKey.h"

#include "astra/crypto/Encoding.h"
#include "astra/crypto/Sha256.h"

namespace astra::license {
namespace {

// Mask is combined with the stored bytes at runtime; both halves live in separate
// translation-unit-local constants so neither one alone reveals the key.
constexpr u8 kMask[8] = {0x5A, 0xC3, 0x17, 0x8E, 0x2D, 0xF1, 0x64, 0x9B};

constexpr u8 kStored[32] = {
    0x42, 0x03, 0x0A, 0x1C, 0x4B, 0x52, 0xD2, 0xE0,
    0x43, 0xEB, 0xE5, 0xFB, 0xB8, 0xD2, 0xD0, 0xD0,
    0x55, 0xD9, 0x48, 0xE1, 0xAD, 0x4C, 0x90, 0x6C,
    0x12, 0x09, 0x28, 0xF6, 0x9C, 0x92, 0x47, 0x27,
};

}  // namespace

Bytes MasterKeyBytes() {
    Bytes key(32);
    for (std::size_t i = 0; i < key.size(); ++i) {
        key[i] = static_cast<u8>(kStored[i] ^ kMask[i % 8]);
    }
    return key;
}

std::string MasterKeyFingerprint() {
    Bytes key = MasterKeyBytes();
    const auto digest = crypto::Sha256::Digest(ByteView(key));
    Wipe(key.data(), key.size());
    return crypto::HexEncode(digest.data(), 8);
}

}  // namespace astra::license
