// crypto/Encoding.h - hex and Crockford base32 codecs.
#pragma once

#include <string>
#include <string_view>

#include "astra/core/Types.h"

namespace astra::crypto {

std::string HexEncode(const u8* data, std::size_t size);
std::string HexEncode(const Bytes& data);
bool HexDecode(std::string_view hex, Bytes& out);

// Crockford base32: no I, L, O, U - safe for humans to read from a screen and retype.
// Decoding is forgiving: lower case, dashes, spaces, O->0, I/L->1 are all accepted.
std::string Base32Encode(const u8* data, std::size_t size);
std::string Base32Encode(const Bytes& data);
bool Base32Decode(std::string_view text, Bytes& out);

// Groups text into chunks, e.g. "A1B2C3D4" with group 4 -> "A1B2-C3D4".
std::string GroupWithDashes(std::string_view text, std::size_t groupSize);

// Removes all characters that are not part of the base32 alphabet and upper-cases.
std::string NormalizeCode(std::string_view text);

}  // namespace astra::crypto
