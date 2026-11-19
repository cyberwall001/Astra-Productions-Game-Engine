// Astra Productions Game Engine
// core/Types.h - fundamental aliases and small utilities shared by every module.
#pragma once

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <string>
#include <string_view>
#include <vector>

namespace astra {

using u8 = std::uint8_t;
using u16 = std::uint16_t;
using u32 = std::uint32_t;
using u64 = std::uint64_t;
using i8 = std::int8_t;
using i16 = std::int16_t;
using i32 = std::int32_t;
using i64 = std::int64_t;

using Bytes = std::vector<u8>;

// Span-ish view over bytes without owning them.
struct ByteView {
    const u8* data = nullptr;
    std::size_t size = 0;

    ByteView() = default;
    ByteView(const u8* d, std::size_t s) : data(d), size(s) {}
    template <std::size_t N>
    explicit ByteView(const u8 (&arr)[N]) : data(arr), size(N) {}
    ByteView(std::string_view text)  // NOLINT: intentional implicit text->bytes
        : data(reinterpret_cast<const u8*>(text.data())), size(text.size()) {}
    explicit ByteView(const Bytes& b) : data(b.data()), size(b.size()) {}
    explicit ByteView(const std::string& s)
        : data(reinterpret_cast<const u8*>(s.data())), size(s.size()) {}

    bool empty() const { return size == 0; }
    const u8* begin() const { return data; }
    const u8* end() const { return data + size; }
};

// Fixed-size byte buffer used by crypto primitives.
template <std::size_t N>
struct ByteBlock {
    u8 bytes[N]{};

    u8* data() { return bytes; }
    const u8* data() const { return bytes; }
    static constexpr std::size_t size() { return N; }
    u8& operator[](std::size_t i) { return bytes[i]; }
    u8 operator[](std::size_t i) const { return bytes[i]; }
};

inline void Wipe(void* p, std::size_t n) {
    volatile u8* v = static_cast<volatile u8*>(p);
    for (std::size_t i = 0; i < n; ++i) v[i] = 0;
}

template <typename T>
inline void WipeObject(T& obj) {
    Wipe(&obj, sizeof(T));
}

}  // namespace astra
