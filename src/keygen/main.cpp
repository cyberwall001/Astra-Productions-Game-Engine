// src/keygen/main.cpp - Astra KeyForge (owner-only access code issuer).
//
// GUI:  AstraKeyForge.exe
// CLI:  AstraKeyForge.exe --issue-code --passphrase <owner passphrase> [--role staff]
//                        [--days 30] [--count 1] [--note "text"]
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

#include "astra/auth/Account.h"
#include "astra/core/FileIO.h"
#include "astra/crypto/Encoding.h"
#include "astra/core/Paths.h"
#include "astra/core/Strings.h"
#include "astra/core/Time.h"
#include "astra/core/Version.h"
#include "astra/data/Json.h"
#include "astra/licensing/AccessCode.h"
#include "astra/licensing/CodeRegistry.h"
#include "astra/licensing/MasterKey.h"
#include "astra/platform/Application.h"

namespace {

int PrintUsage() {
    std::printf(
        "Astra KeyForge %s - owner-only access code issuer\n\n"
        "  AstraKeyForge.exe                       open the owner tool\n"
        "  AstraKeyForge.exe --issue-code ...      mint codes headlessly\n\n"
        "Options for --issue-code:\n"
        "  --passphrase <text>   owner passphrase (or ASTRA_KEYFORGE_PASSPHRASE)\n"
        "  --role user|staff|owner   default: user\n"
        "  --days <n>                default: 30 (0 = never expires)\n"
        "  --count <n>               default: 1\n"
        "  --note \"text\"             ledger note\n"
        "  --json                    machine readable output\n\n"
        "  --fingerprint             print the master key fingerprint\n",
        astra::kEngineVersion);
    return 0;
}

std::string ReadOwnerHash(const std::string& path, std::string& salt, int& iterations) {
    std::string text;
    if (!astra::fs::ReadTextFile(path, text)) return {};
    astra::json::Value root;
    if (!astra::json::Value::Parse(text, root)) return {};
    salt = root["salt"].AsString();
    iterations = static_cast<int>(root["iterations"].AsInt(200000));
    return root["hash"].AsString();
}

bool VerifyOwnerPassphrase(const std::string& passphrase) {
    const std::string path = astra::paths::Join(astra::paths::KeyForgeDataDir(), "owner.json");
    std::string salt;
    int iterations = 200000;
    const std::string hash = ReadOwnerHash(path, salt, iterations);
    if (hash.empty() || salt.empty()) {
        std::fprintf(stderr,
                     "No owner record at %s.\nOpen AstraKeyForge.exe once to create the owner "
                     "passphrase, then retry.\n",
                     path.c_str());
        return false;
    }

    // Local PBKDF2 check reuses the account hashing helper.
    std::string derived = astra::auth::HashPassword(passphrase, salt, iterations);
    const bool ok = derived == hash;
    astra::Wipe(derived.data(), derived.size());
    if (!ok) std::fprintf(stderr, "Owner passphrase rejected.\n");
    return ok;
}

astra::license::Role ParseRole(const std::string& text) {
    const std::string lowered = astra::str::ToLower(text);
    if (lowered == "staff") return astra::license::Role::Staff;
    if (lowered == "owner") return astra::license::Role::Owner;
    return astra::license::Role::User;
}

int IssueFromCommandLine(char** argv, int argc) {
    std::string passphrase = std::getenv("ASTRA_KEYFORGE_PASSPHRASE")
                                 ? std::getenv("ASTRA_KEYFORGE_PASSPHRASE")
                                 : "";
    std::string roleText = "user";
    int days = 30;
    int count = 1;
    std::string note;
    bool json = false;

    for (int i = 2; i < argc; ++i) {
        const std::string arg = argv[i];
        if (arg == "--passphrase" && i + 1 < argc) passphrase = argv[++i];
        else if (arg == "--role" && i + 1 < argc) roleText = argv[++i];
        else if (arg == "--days" && i + 1 < argc) days = std::atoi(argv[++i]);
        else if (arg == "--count" && i + 1 < argc) count = std::atoi(argv[++i]);
        else if (arg == "--note" && i + 1 < argc) note = argv[++i];
        else if (arg == "--json") json = true;
    }

    if (passphrase.empty()) {
        std::fprintf(stderr, "An owner passphrase is required (--passphrase or ASTRA_KEYFORGE_PASSPHRASE).\n");
        return 2;
    }
    if (!VerifyOwnerPassphrase(passphrase)) return 3;
    if (count < 1) count = 1;
    if (count > 50) count = 50;

    const astra::license::Role role = ParseRole(roleText);
    const astra::Bytes secret = astra::license::MasterKeyBytes();
    const std::int64_t nowUtc = astra::time::NowUtc();

    astra::license::IssuedLedger ledger = astra::license::IssuedLedger::ForKeyForge();
    ledger.Load();

    if (json) std::printf("{\"codes\":[");
    for (int i = 0; i < count; ++i) {
        astra::license::CodePayload payload;
        payload.version = astra::kAccessCodeVersion;
        payload.role = role;
        payload.durationDays = days;
        payload.issuedDay = astra::license::DayFromUtc(nowUtc);
        payload.nonce = static_cast<std::uint32_t>(std::rand()) & 0x7FFF;

        const astra::license::AccessCode code = astra::license::IssueCode(payload, secret);

        astra::license::IssuedCode record;
        record.hash = astra::license::CodeHash(code.text);
        record.masked = astra::license::MaskCode(astra::crypto::NormalizeCode(code.text));
        record.code = code.text;
        record.role = astra::license::RoleName(role);
        record.duration = astra::license::DurationLabel(days);
        record.note = note;
        record.batch = "cli";
        record.issuedUtc = nowUtc;
        record.expiryUtc = code.expiryUtc;
        record.neverExpires = code.neverExpires;
        ledger.Add(record);

        if (json) {
            std::printf("%s{\"code\":\"%s\",\"role\":\"%s\",\"duration\":\"%s\",\"expires\":\"%s\"}",
                        i ? "," : "", code.text.c_str(), astra::license::RoleName(role),
                        record.duration.c_str(),
                        code.neverExpires ? "never" : astra::time::ToIso8601(code.expiryUtc).c_str());
        } else {
            std::printf("%s   %-5s %-12s %s\n", code.text.c_str(), astra::license::RoleName(role),
                        record.duration.c_str(),
                        code.neverExpires ? "never expires" : astra::time::ToIso8601(code.expiryUtc).c_str());
        }
    }
    if (json) std::printf("]}\n");
    ledger.Save();
    astra::Wipe(secret.data(), secret.size());
    return 0;
}

}  // namespace

int RunCli(int argc, char** argv) {
    for (int i = 1; i < argc; ++i) {
        const std::string arg = argv[i];
        if (arg == "--help" || arg == "-h") return PrintUsage();
        if (arg == "--fingerprint") {
            std::printf("%s\n", astra::license::MasterKeyFingerprint().c_str());
            return 0;
        }
        if (arg == "--issue-code") return IssueFromCommandLine(argv, argc);
    }
    return -1;  // no CLI request: open the window
}

#if defined(_WIN32) && defined(ASTRA_GUI_SUBSYSTEM)

#include <shellapi.h>
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>

int WINAPI WinMain(HINSTANCE, HINSTANCE, LPSTR, int) {
    // A GUI-subsystem process has no argv, so rebuild it from the command line when the
    // owner uses the CLI (--issue-code / --fingerprint / --help).
    int argc = 0;
    LPWSTR* wideArgv = CommandLineToArgvW(GetCommandLineW(), &argc);
    if (!wideArgv || argc <= 1) {
        if (wideArgv) LocalFree(wideArgv);
        return astra::platform::RunKeyForgeWindow();
    }

    std::vector<std::string> storage;
    storage.reserve(static_cast<std::size_t>(argc));
    for (int i = 0; i < argc; ++i) {
        const int needed = WideCharToMultiByte(CP_UTF8, 0, wideArgv[i], -1, nullptr, 0, nullptr,
                                               nullptr);
        std::string text(static_cast<std::size_t>(needed > 0 ? needed - 1 : 0), '\0');
        if (needed > 1) {
            WideCharToMultiByte(CP_UTF8, 0, wideArgv[i], -1, text.data(), needed, nullptr, nullptr);
        }
        storage.push_back(std::move(text));
    }
    LocalFree(wideArgv);

    std::vector<char*> argv;
    argv.reserve(storage.size());
    for (auto& item : storage) argv.push_back(item.data());

    const int cliResult = RunCli(static_cast<int>(argv.size()), argv.data());
    if (cliResult >= 0) return cliResult;
    return astra::platform::RunKeyForgeWindow();
}

#else

int main(int argc, char** argv) {
    const int cliResult = RunCli(argc, argv);
    if (cliResult >= 0) return cliResult;
    return astra::platform::RunKeyForgeWindow();
}

#endif
