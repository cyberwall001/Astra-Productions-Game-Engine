#include "astra/crypto/Sha256.h"

#include <cstring>

namespace astra::crypto {
namespace {

inline u32 Rotr(u32 x, int n) { return (x >> n) | (x << (32 - n)); }

constexpr u32 kK[64] = {
    0x428a2f98u, 0x71374491u, 0xb5c0fbcfu, 0xe9b5dba5u, 0x3956c25bu, 0x59f111f1u,
    0x923f82a4u, 0xab1c5ed5u, 0xd807aa98u, 0x12835b01u, 0x243185beu, 0x550c7dc3u,
    0x72be5d74u, 0x80deb1feu, 0x9bdc06a7u, 0xc19bf174u, 0xe49b69c1u, 0xefbe4786u,
    0x0fc19dc6u, 0x240ca1ccu, 0x2de92c6fu, 0x4a7484aau, 0x5cb0a9dcu, 0x76f988dau,
    0x983e5152u, 0xa831c66du, 0xb00327c8u, 0xbf597fc7u, 0xc6e00bf3u, 0xd5a79147u,
    0x06ca6351u, 0x14292967u, 0x27b70a85u, 0x2e1b2138u, 0x4d2c6dfcu, 0x53380d13u,
    0x650a7354u, 0x766a0abbu, 0x81c2c92eu, 0x92722c85u, 0xa2bfe8a1u, 0xa81a664bu,
    0xc24b8b70u, 0xc76c51a3u, 0xd192e819u, 0xd6990624u, 0xf40e3585u, 0x106aa070u,
    0x19a4c116u, 0x1e376c08u, 0x2748774cu, 0x34b0bcb5u, 0x391c0cb3u, 0x4ed8aa4au,
    0x5b9cca4fu, 0x682e6ff3u, 0x748f82eeu, 0x78a5636fu, 0x84c87814u, 0x8cc70208u,
    0x90befffau, 0xa4506cebu, 0xbef9a3f7u, 0xc67178f2u};

}  // namespace

Sha256::Sha256() {
    state_[0] = 0x6a09e667u;
    state_[1] = 0xbb67ae85u;
    state_[2] = 0x3c6ef372u;
    state_[3] = 0xa54ff53au;
    state_[4] = 0x510e527fu;
    state_[5] = 0x9b05688cu;
    state_[6] = 0x1f83d9abu;
    state_[7] = 0x5be0cd19u;
}

void Sha256::Transform(const u8 block[kBlockSize]) {
    u32 w[64];
    for (int i = 0; i < 16; ++i) {
        w[i] = (static_cast<u32>(block[i * 4]) << 24) |
               (static_cast<u32>(block[i * 4 + 1]) << 16) |
               (static_cast<u32>(block[i * 4 + 2]) << 8) |
               static_cast<u32>(block[i * 4 + 3]);
    }
    for (int i = 16; i < 64; ++i) {
        const u32 s0 = Rotr(w[i - 15], 7) ^ Rotr(w[i - 15], 18) ^ (w[i - 15] >> 3);
        const u32 s1 = Rotr(w[i - 2], 17) ^ Rotr(w[i - 2], 19) ^ (w[i - 2] >> 10);
        w[i] = w[i - 16] + s0 + w[i - 7] + s1;
    }

    u32 a = state_[0], b = state_[1], c = state_[2], d = state_[3];
    u32 e = state_[4], f = state_[5], g = state_[6], h = state_[7];

    for (int i = 0; i < 64; ++i) {
        const u32 s1 = Rotr(e, 6) ^ Rotr(e, 11) ^ Rotr(e, 25);
        const u32 ch = (e & f) ^ (~e & g);
        const u32 temp1 = h + s1 + ch + kK[i] + w[i];
        const u32 s0 = Rotr(a, 2) ^ Rotr(a, 13) ^ Rotr(a, 22);
        const u32 maj = (a & b) ^ (a & c) ^ (b & c);
        const u32 temp2 = s0 + maj;
        h = g;
        g = f;
        f = e;
        e = d + temp1;
        d = c;
        c = b;
        b = a;
        a = temp1 + temp2;
    }

    state_[0] += a;
    state_[1] += b;
    state_[2] += c;
    state_[3] += d;
    state_[4] += e;
    state_[5] += f;
    state_[6] += g;
    state_[7] += h;
}

void Sha256::Update(const void* data, std::size_t size) {
    const u8* p = static_cast<const u8*>(data);
    bitCount_ += static_cast<u64>(size) * 8;

    if (bufferSize_ > 0) {
        const std::size_t need = kBlockSize - bufferSize_;
        const std::size_t take = size < need ? size : need;
        std::memcpy(buffer_ + bufferSize_, p, take);
        bufferSize_ += take;
        p += take;
        size -= take;
        if (bufferSize_ == kBlockSize) {
            Transform(buffer_);
            bufferSize_ = 0;
        }
    }

    while (size >= kBlockSize) {
        Transform(p);
        p += kBlockSize;
        size -= kBlockSize;
    }

    if (size > 0) {
        std::memcpy(buffer_, p, size);
        bufferSize_ = size;
    }
}

void Sha256::Final(u8 out[kDigestSize]) {
    const u64 bitCount = bitCount_;
    u8 pad = 0x80;
    Update(&pad, 1);
    u8 zero = 0x00;
    while (bufferSize_ != 56) Update(&zero, 1);

    u8 lengthBytes[8];
    for (int i = 0; i < 8; ++i) lengthBytes[i] = static_cast<u8>(bitCount >> (56 - i * 8));
    // Update() would corrupt bitCount_, so write the length block directly.
    std::memcpy(buffer_ + 56, lengthBytes, 8);
    Transform(buffer_);
    bufferSize_ = 0;

    for (int i = 0; i < 8; ++i) {
        out[i * 4] = static_cast<u8>(state_[i] >> 24);
        out[i * 4 + 1] = static_cast<u8>(state_[i] >> 16);
        out[i * 4 + 2] = static_cast<u8>(state_[i] >> 8);
        out[i * 4 + 3] = static_cast<u8>(state_[i]);
    }
}

std::array<u8, Sha256::kDigestSize> Sha256::Digest(ByteView data) {
    Sha256 hasher;
    hasher.Update(data);
    std::array<u8, kDigestSize> out{};
    hasher.Final(out.data());
    return out;
}

namespace {
std::string ToHex(const u8* data, std::size_t size) {
    static const char* kHex = "0123456789abcdef";
    std::string out;
    out.reserve(size * 2);
    for (std::size_t i = 0; i < size; ++i) {
        out += kHex[data[i] >> 4];
        out += kHex[data[i] & 0x0F];
    }
    return out;
}
}  // namespace

std::string Sha256::HexDigest(ByteView data) {
    const auto digest = Digest(data);
    return ToHex(digest.data(), digest.size());
}

std::string Sha256::HexDigest(std::string_view text) {
    return HexDigest(ByteView(text));
}

std::array<u8, 32> HmacSha256(ByteView key, ByteView message) {
    constexpr std::size_t kBlock = 64;
    u8 k0[kBlock]{};
    if (key.size > kBlock) {
        const auto hashed = Sha256::Digest(key);
        std::memcpy(k0, hashed.data(), hashed.size());
    } else {
        std::memcpy(k0, key.data, key.size);
    }

    u8 innerPad[kBlock];
    u8 outerPad[kBlock];
    for (std::size_t i = 0; i < kBlock; ++i) {
        innerPad[i] = k0[i] ^ 0x36;
        outerPad[i] = k0[i] ^ 0x5c;
    }

    Sha256 inner;
    inner.Update(innerPad, kBlock);
    inner.Update(message);
    std::array<u8, 32> innerDigest{};
    inner.Final(innerDigest.data());

    Sha256 outer;
    outer.Update(outerPad, kBlock);
    outer.Update(innerDigest.data(), innerDigest.size());
    std::array<u8, 32> tag{};
    outer.Final(tag.data());

    Wipe(k0, sizeof(k0));
    Wipe(innerPad, sizeof(innerPad));
    Wipe(outerPad, sizeof(outerPad));
    Wipe(innerDigest.data(), innerDigest.size());
    return tag;
}

std::array<u8, 32> HmacSha256(ByteView key, std::string_view message) {
    return HmacSha256(key, ByteView(message));
}

std::array<u8, 32> Pbkdf2Sha256(std::string_view password, ByteView salt, u32 iterations) {
    if (iterations == 0) iterations = 1;
    // Single block: INT(1) appended to the salt.
    u8 saltPlusIndex[1024];
    const std::size_t saltLen = salt.size > sizeof(saltPlusIndex) - 4 ? sizeof(saltPlusIndex) - 4
                                                                    : salt.size;
    std::memcpy(saltPlusIndex, salt.data, saltLen);
    const std::size_t blockLen = saltLen + 4;
    saltPlusIndex[saltLen + 0] = 0;
    saltPlusIndex[saltLen + 1] = 0;
    saltPlusIndex[saltLen + 2] = 0;
    saltPlusIndex[saltLen + 3] = 1;

    const ByteView passwordView(password);
    auto u = HmacSha256(passwordView, ByteView(saltPlusIndex, blockLen));
    auto result = u;

    for (u32 i = 1; i < iterations; ++i) {
        u = HmacSha256(passwordView, ByteView(u.data(), u.size()));
        for (std::size_t b = 0; b < result.size(); ++b) result[b] ^= u[b];
    }

    Wipe(u.data(), u.size());
    Wipe(saltPlusIndex, sizeof(saltPlusIndex));
    return result;
}

bool ConstantTimeEquals(ByteView a, ByteView b) {
    if (a.size != b.size) return false;
    u8 diff = 0;
    for (std::size_t i = 0; i < a.size; ++i) diff |= static_cast<u8>(a.data[i] ^ b.data[i]);
    return diff == 0;
}

}  // namespace astra::crypto
