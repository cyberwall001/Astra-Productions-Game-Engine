// app/KeyForgeApp.h - Astra KeyForge: the owner-only access-code issuer.
//
// Runs as its own executable (AstraKeyForge.exe) so the engine never ships a code
// generator in the same process as the client registry. Owner passphrase gates the UI.
#pragma once

#include <string>
#include <vector>

#include "astra/licensing/AccessCode.h"
#include "astra/licensing/CodeRegistry.h"

namespace astra::app {

struct IssuedRow {
    license::AccessCode code;
    std::string note;
    std::string batch;
};

struct KeyForgeApp {
    enum class Stage { CreatePassphrase, Unlock, Ready };
    enum class Tab { Generate = 0, Ledger = 1, Settings = 2 };

    Stage stage = Stage::CreatePassphrase;
    Tab tab = Tab::Generate;

    // Passphrase gate
    std::string passphrase;
    std::string passphraseConfirm;
    std::string gateError;
    std::string gateHint;
    bool revealPassphrase = false;
    bool hasOwnerRecord = false;
    std::string ownerPath;

    // Generator form
    int roleIndex = 0;         // 0 user, 1 staff, 2 owner
    int durationIndex = 4;     // 30 days
    int quantity = 1;
    std::string note;
    std::string batchName;
    std::vector<IssuedRow> lastBatch;

    // Ledger
    int ledgerFilter = 0;      // 0 all, 1 active, 2 expired, 3 revoked
    std::string ledgerSearch;

    // Chrome
    float time = 0.0f;
    float dt = 1.0f / 60.0f;
    bool wantsQuit = false;
    bool initialized = false;
    std::string status;
    float statusTimer = 0.0f;

    license::IssuedLedger ledger;

    void Init();
    void Tick(float deltaTime);
    void Draw();

    bool HasOwnerPassphrase() const;
    bool CreateOwnerPassphrase();
    bool Unlock();
    void SetStatus(std::string text);

    void GenerateCodes();
    void CopyToClipboard(const std::string& text, const std::string& what);
    bool ExportBatch(bool asJson);
    void Revoke(const std::string& hash);
};

void DrawKeyForgeApp(KeyForgeApp& app);

}  // namespace astra::app
