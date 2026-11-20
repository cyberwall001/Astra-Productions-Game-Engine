#include "astra/crypto/Random.h"

#include <cstring>
#include <random>

#include "astra/crypto/Encoding.h"
#include "astra/core/Log.h"

#if defined(_WIN32)
#include <windows.h>
#include <bcrypt.h>
#pragma comment(lib, "bcrypt.lib")
#else
#include <errno.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#if defined(__linux__)
#include <sys/random.h>
#endif
#endif

namespace astra::crypto {
namespace {

unsigned gSeed = 0;  // 0 = secure mode

bool PlatformRandom(void* out, std::size_t size) {
#if defined(_WIN32)
    const NTSTATUS status = BCryptGenRandom(nullptr, static_cast<PUCHAR>(out),
                                            static_cast<ULONG>(size),
                                            BCRYPT_USE_SYSTEM_PREFERRED_RNG);
    return BCRYPT_SUCCESS(status);
#else
    std::size_t filled = 0;
#if defined(__linux__)
    while (filled < size) {
        const ssize_t n = getrandom(static_cast<char*>(out) + filled, size - filled, 0);
        if (n <= 0) {
            if (errno == EINTR) continue;
            break;
        }
        filled += static_cast<std::size_t>(n);
    }
    if (filled == size) return true;
    filled = 0;
#endif
    const int fd = open("/dev/urandom", O_RDONLY);
    if (fd < 0) return false;
    while (filled < size) {
        const ssize_t n = read(fd, static_cast<char*>(out) + filled, size - filled);
        if (n <= 0) {
            if (n < 0 && errno == EINTR) continue;
            break;
        }
        filled += static_cast<std::size_t>(n);
    }
    close(fd);
    return filled == size;
#endif
}

}  // namespace

void SetDeterministicSeedForTesting(unsigned seed) { gSeed = seed; }

void SecureRandom(void* out, std::size_t size) {
    if (gSeed != 0) {
        std::mt19937 rng(gSeed);
        auto* bytes = static_cast<unsigned char*>(out);
        for (std::size_t i = 0; i < size; ++i) bytes[i] = static_cast<unsigned char>(rng() & 0xFF);
        return;
    }
    if (!PlatformRandom(out, size)) {
        log::Error("crypto", "platform RNG unavailable - refusing to fall back to weak randomness");
        std::memset(out, 0, size);
    }
}

Bytes SecureRandomBytes(std::size_t size) {
    Bytes out(size);
    SecureRandom(out.data(), out.size());
    return out;
}

std::string RandomHex(std::size_t bytes) {
    const Bytes data = SecureRandomBytes(bytes);
    return HexEncode(data);
}

}  // namespace astra::crypto
