#include "astra/crypto/SecretProtect.h"

#include "astra/core/Log.h"
#include "astra/crypto/Encoding.h"

#if defined(_WIN32)
#include <windows.h>
#include <dpapi.h>
#pragma comment(lib, "crypt32.lib")
#endif

namespace astra::crypto {
namespace {

bool gWarnedAboutWeakProtection = false;

std::string Obfuscate(const std::string& plaintext) {
    // Non-Windows dev builds: reversible scrambling, never used for shipped Windows builds.
    Bytes bytes(plaintext.begin(), plaintext.end());
    const u8 mask = 0x5B;
    for (auto& b : bytes) b = static_cast<u8>(b ^ mask);
    return "raw:" + HexEncode(bytes);
}

std::string Deobfuscate(const std::string& encoded) {
    Bytes bytes;
    if (!HexDecode(encoded.substr(4), bytes)) return {};
    const u8 mask = 0x5B;
    std::string out;
    out.reserve(bytes.size());
    for (auto b : bytes) out += static_cast<char>(b ^ mask);
    return out;
}

}  // namespace

bool IsSecureProtectionAvailable() {
#if defined(_WIN32)
    return true;
#else
    return false;
#endif
}

std::string ProtectSecret(const std::string& plaintext) {
    if (plaintext.empty()) return {};
#if defined(_WIN32)
    DATA_BLOB input;
    input.pbData = reinterpret_cast<BYTE*>(const_cast<char*>(plaintext.data()));
    input.cbData = static_cast<DWORD>(plaintext.size());

    DATA_BLOB output{};
    if (CryptProtectData(&input,, L"Astra Studio GitHub token", nullptr, nullptr, nullptr,
                         CRYPTPROTECT_UI_FORBIDDEN, &output)) {
        const std::string encoded = "dpapi:" + HexEncode(output.pbData, output.cbData);
        LocalFree(output.pbData);
        return encoded;
    }
    log::Warn("crypto", "DPAPI protection failed; token will not be stored");
    return {};
#else
    if (!gWarnedAboutWeakProtection) {
        gWarnedAboutWeakProtection = true;
        log::Warn("crypto", "DPAPI unavailable on this platform - token storage is obfuscated only");
    }
    return Obfuscate(plaintext);
#endif
}

std::string UnprotectSecret(const std::string& protectedText) {
    if (protectedText.empty()) return {};
    if (protectedText.rfind("raw:", 0) == 0) return Deobfuscate(protectedText);
    if (protectedText.rfind("dpapi:", 0) != 0) return {};

    Bytes bytes;
    if (!HexDecode(protectedText.substr(6), bytes)) return {};

#if defined(_WIN32)
    DATA_BLOB input;
    input.pbData = bytes.data();
    input.cbData = static_cast<DWORD>(bytes.size());
    DATA_BLOB output{};
    if (CryptUnprotectData(&input, nullptr, nullptr, nullptr, nullptr, CRYPTPROTECT_UI_FORBIDDEN,
                           &output)) {
        std::string plain(reinterpret_cast<char*>(output.pbData), output.cbData);
        SecureZeroMemory(output.pbData, output.cbData);
        LocalFree(output.pbData);
        return plain;
    }
    return {};
#else
    return {};
#endif
}

}  // namespace astra::crypto
