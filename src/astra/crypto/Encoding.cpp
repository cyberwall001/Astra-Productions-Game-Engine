#include "astra/crypto/Encoding.h"

#include <cctype>

namespace astra::crypto {
namespace {

constexpr const char* kBase32Alphabet = "0123456789ABCDEFGHJKMNPQRSTVWXYZ";

int Base32Value(char c) {
    switch (c) {
        case '0': case 'O': case 'o': return 0;
        case '1': case 'I': case 'i': case 'L': case 'l': return 1;
        case '2': return 2;
        case '3': return 3;
        case '4': return 4;
        case '5': return 5;
        case '6': return 6;
        case '7': return 7;
        case '8': return 8;
        case '9': return 9;
        default: break;
    }
    for (int i = 10; i < 32; ++i) {
        const char upper = static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
        if (upper == kBase32Alphabet[i]) return i;
    }
    return -1;
}

int HexValue(char c) {
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    return -1;
}

}  // namespace

std::string HexEncode(const u8* data, std::size_t size) {
    static const char* kHex = "0123456789abcdef";
    std::string out;
    out.reserve(size * 2);
    for (std::size_t i = 0; i < size; ++i) {
        out += kHex[data[i] >> 4];
        out += kHex[data[i] & 0x0F];
    }
    return out;
}

std::string HexEncode(const Bytes& data) { return HexEncode(data.data(), data.size()); }

bool HexDecode(std::string_view hex, Bytes& out) {
    if (hex.size() % 2 != 0) return false;
    out.clear();
    out.reserve(hex.size() / 2);
    for (std::size_t i = 0; i < hex.size(); i += 2) {
        const int hi = HexValue(hex[i]);
        const int lo = HexValue(hex[i + 1]);
        if (hi < 0 || lo < 0) return false;
        out.push_back(static_cast<u8>((hi << 4) | lo));
    }
    return true;
}

std::string Base32Encode(const u8* data, std::size_t size) {
    std::string out;
    out.reserve((size * 8 + 4) / 5);
    std::uint32_t buffer = 0;
    int bits = 0;
    for (std::size_t i = 0; i < size; ++i) {
        buffer = (buffer << 8) | data[i];
        bits += 8;
        while (bits >= 5) {
            out += kBase32Alphabet[(buffer >> (bits - 5)) & 0x1F];
            bits -= 5;
        }
    }
    if (bits > 0) {
        out += kBase32Alphabet[(buffer << (5 - bits)) & 0x1F];
    }
    return out;
}

std::string Base32Encode(const Bytes& data) { return Base32Encode(data.data(), data.size()); }

bool Base32Decode(std::string_view text, Bytes& out) {
    const std::string normalized = NormalizeCode(text);
    if (normalized.empty()) return false;
    out.clear();
    std::uint32_t buffer = 0;
    int bits = 0;
    for (char c : normalized) {
        const int value = Base32Value(c);
        if (value < 0) return false;
        buffer = (buffer << 5) | static_cast<std::uint32_t>(value);
        bits += 5;
        if (bits >= 8) {
            out.push_back(static_cast<u8>((buffer >> (bits - 8)) & 0xFF));
            bits -= 8;
        }
    }
    return true;
}

std::string GroupWithDashes(std::string_view text, std::size_t groupSize) {
    if (groupSize == 0) return std::string(text);
    std::string out;
    for (std::size_t i = 0; i < text.size(); ++i) {
        if (i > 0 && i % groupSize == 0) out += '-';
        out += text[i];
    }
    return out;
}

std::string NormalizeCode(std::string_view text) {
    std::string out;
    out.reserve(text.size());
    for (char c : text) {
        if (Base32Value(c) >= 0) {
            out += static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
        }
    }
    return out;
}

}  // namespace astra::crypto
