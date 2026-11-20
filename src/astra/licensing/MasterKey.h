// licensing/MasterKey.h - the shared code-signing secret used by Astra Studio (verify)
// and Astra KeyForge (issue).
//
// SECURITY NOTE (read docs/SECURITY.md):
//   This build uses a symmetric HMAC tag so that codes can be verified fully offline
//   with zero infrastructure. That means Astra Studio necessarily contains the same
//   key that KeyForge signs with, so anyone who reverse-engineers the shipped binary
//   could mint codes. It is the right trade-off for a small studio shipping its own
//   tools in this milestone; the v0.2 licensing service replaces this file with an
//   Ed25519 public key, and only KeyForge keeps the private half. The interface below
//   does not change when that happens.
#pragma once

#include <string>

#include "astra/core/Types.h"

namespace astra::license {

// 32-byte HMAC-SHA256 key, reconstructed at runtime (stored XOR-masked, never as a
// contiguous string literal, to keep casual `strings` scrapes from finding it).
Bytes MasterKeyBytes();

// Short, non-secret identifier of the master key (first 8 bytes of SHA-256, hex).
// Shown in KeyForge + Studio settings so an owner can confirm both tools match.
std::string MasterKeyFingerprint();

}  // namespace astra::license
