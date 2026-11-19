#include "astra/app/KeyForgeApp.h"

#include "astra/app/screens/Common.h"

#include <algorithm>
#include <cmath>

#include "astra/auth/Account.h"
#include "astra/core/FileIO.h"
#include "astra/core/Log.h"
#include "astra/core/Paths.h"
#include "astra/core/Strings.h"
#include "astra/core/Time.h"
#include "astra/core/Version.h"
#include "astra/crypto/Encoding.h"
#include "astra/crypto/Random.h"
#include "astra/data/Json.h"
#include "astra/licensing/MasterKey.h"
#include "astra/ui/Branding.h"
#include "astra/ui/Theme.h"
#include "astra/ui/Widgets.h"
#include "imgui.h"

namespace astra::app {
namespace {

using ui::Colors;
using ui::Icon;
using ui::Text;
using ui::U;

constexpr int kOwnerIterations = 200000;

std::string OwnerRecordPath() {
    return paths::Join(paths::KeyForgeDataDir(), "owner.json");
}

}  // namespace

void KeyForgeApp::Init() {
    if (initialized) return;
    initialized = true;
    ownerPath = OwnerRecordPath();
    hasOwnerRecord = HasOwnerPassphrase();
    stage = hasOwnerRecord ? Stage::Unlock : Stage::CreatePassphrase;
    ledger = license::IssuedLedger::ForKeyForge();
    ledger.Load();
    log::Info("keyforge", str::Format("ledger loaded: %zu codes", ledger.Codes().size()));
    log::Info("keyforge", "master key " + license::MasterKeyFingerprint());
}

bool KeyForgeApp::HasOwnerPassphrase() const {
    std::string text;
    return fs::ReadTextFile(OwnerRecordPath(), text);
}

bool KeyForgeApp::CreateOwnerPassphrase() {
    if (passphrase.size() < 12) {
        gateError = "Owner passphrases need at least 12 characters";
        gateHint = "This protects every code you ever issue.";
        return false;
    }
    if (passphrase != passphraseConfirm) {
        gateError = "Those passphrases do not match";
        return false;
    }

    json::Value record = json::Value::MakeObject();
    const std::string salt = auth::NewSaltHex();
    record.Set("schema", json::Value(1));
    record.Set("salt", json::Value(salt));
    record.Set("hash", json::Value(auth::HashPassword(passphrase, salt, kOwnerIterations)));
    record.Set("iterations", json::Value(kOwnerIterations));
    record.Set("created", json::Value(time::NowUtc()));
    record.Set("fingerprint", json::Value(license::MasterKeyFingerprint()));

    if (!fs::WriteTextFileAtomic(OwnerRecordPath(), record.Dump(true))) {
        gateError = "Could not write the owner record";
        return false;
    }
    hasOwnerRecord = true;
    passphrase.clear();
    passphraseConfirm.clear();
    gateError.clear();
    stage = Stage::Ready;
    SetStatus("Owner passphrase created");
    log::Info("keyforge", "owner passphrase created");
    return true;
}

bool KeyForgeApp::Unlock() {
    std::string text;
    if (!fs::ReadTextFile(OwnerRecordPath(), text)) {
        gateError = "Owner record is missing";
        stage = Stage::CreatePassphrase;
        return false;
    }
    json::Value root;
    if (!json::Value::Parse(text, root)) {
        gateError = "Owner record is corrupt";
        return false;
    }
    const std::string salt = root["salt"].AsString();
    const std::string hash = root["hash"].AsString();
    const int iterations = static_cast<int>(root["iterations"].AsInt(kOwnerIterations));

    if (!auth::VerifyPasswordHash(passphrase, salt, hash, iterations)) {
        gateError = "That passphrase is not correct";
        gateHint = "KeyForge locks after repeated failures - restart the app if needed.";
        passphrase.clear();
        return false;
    }

    passphrase.clear();
    gateError.clear();
    gateHint.clear();
    stage = Stage::Ready;
    SetStatus("Unlocked - owner tools ready");
    log::Info("keyforge", "owner unlocked");
    return true;
}

void KeyForgeApp::SetStatus(std::string text) {
    status = std::move(text);
    statusTimer = 4.0f;
}

void KeyForgeApp::CopyToClipboard(const std::string& text, const std::string& what) {
    ImGui::SetClipboardText(text.c_str());
    SetStatus(what + " copied to clipboard");
}

void KeyForgeApp::GenerateCodes() {
    if (stage != Stage::Ready) return;
    const auto& durations = license::DurationOptions();
    const int durationDays = license::DurationDaysFromIndex(durationIndex);
    const license::Role role = static_cast<license::Role>(ImClamp(roleIndex, 0, 2));

    Bytes secret = license::MasterKeyBytes();
    const std::int64_t nowUtc = time::NowUtc();
    const std::int64_t issuedDay = license::DayFromUtc(nowUtc);

    lastBatch.clear();
    const std::string batch = batchName.empty()
                                  ? "batch-" + std::to_string(time::NowUtc())
                                  : batchName;

    for (int i = 0; i < quantity; ++i) {
        license::CodePayload payload;
        payload.version = kAccessCodeVersion;
        payload.role = role;
        payload.durationDays = durationDays;
        payload.issuedDay = issuedDay;
        payload.nonce = static_cast<std::uint32_t>(crypto::SecureRandomBytes(2)[0] |
                                                   (crypto::SecureRandomBytes(2)[0] << 8)) &
                        0x7FFF;

        IssuedRow row;
        row.code = license::IssueCode(payload, secret);
        row.note = note;
        row.batch = batch;

        license::IssuedCode record;
        record.hash = license::CodeHash(row.code.text);
        record.masked = license::MaskCode(crypto::NormalizeCode(row.code.text));
        record.code = row.code.text;
        record.role = license::RoleName(role);
        record.duration = durations[static_cast<std::size_t>(durationIndex)].label;
        record.note = note;
        record.batch = batch;
        record.issuedUtc = nowUtc;
        record.expiryUtc = row.code.expiryUtc;
        record.neverExpires = row.code.neverExpires;
        ledger.Add(record);

        lastBatch.push_back(std::move(row));
    }
    ledger.Save();
    Wipe(secret.data(), secret.size());
    SetStatus(str::Format("Issued %d %s code%s", quantity, license::RoleName(role),
                          quantity == 1 ? "" : "s"));
    log::Info("keyforge", str::Format("issued %d code(s) for %s", quantity,
                                      license::RoleName(role)));
}

bool KeyForgeApp::ExportBatch(bool asJson) {
    if (lastBatch.empty()) {
        SetStatus("Nothing to export yet");
        return false;
    }
    const std::string dir = paths::EnsureDataDir("keyforge/exports");
    const std::string stamp = time::FormatUtc(time::NowUtc());
    std::string safeStamp = str::ReplaceAll(stamp, ":", "-");
    safeStamp = str::ReplaceAll(safeStamp, " ", "_");

    std::string output;
    if (asJson) {
        json::Value root = json::Value::MakeObject();
        root.Set("issuer", json::Value(kCompany));
        root.Set("generated", json::Value(time::ToIso8601(time::NowUtc())));
        root.Set("fingerprint", json::Value(license::MasterKeyFingerprint()));
        json::Value codes = json::Value::MakeArray();
        for (const auto& row : lastBatch) {
            json::Value item = json::Value::MakeObject();
            item.Set("code", json::Value(row.code.text));
            item.Set("role", json::Value(license::RoleName(row.code.payload.role)));
            item.Set("duration", json::Value(license::DurationLabel(row.code.payload.durationDays)));
            item.Set("expires", json::Value(row.code.neverExpires ? std::string("never")
                                                                  : time::ToIso8601(row.code.expiryUtc)));
            item.Set("note", json::Value(row.note));
            codes.Push(std::move(item));
        }
        root.Set("codes", std::move(codes));
        output = root.Dump(true);
    } else {
        output += "Astra Productions - engine access codes\n";
        output += "Generated " + time::ToIso8601(time::NowUtc()) + "\n";
        output += "Issuer key " + license::MasterKeyFingerprint() + "\n";
        output += "------------------------------------------\n";
        for (const auto& row : lastBatch) {
            output += row.code.text + "   " + license::RoleName(row.code.payload.role) + "   " +
                      license::DurationLabel(row.code.payload.durationDays) + "   " +
                      (row.note.empty() ? "-" : row.note) + "\n";
        }
        output += "------------------------------------------\n";
        output += "Each code registers exactly one Astra Studio account.\n";
    }

    const std::string path = paths::Join(dir, std::string(asJson ? "codes-" : "codes-") +
                                                  safeStamp + (asJson ? ".json" : ".txt"));
    if (!fs::WriteTextFileAtomic(path, output)) {
        SetStatus("Export failed");
        return false;
    }
    SetStatus("Exported to " + path);
    return true;
}

void KeyForgeApp::Revoke(const std::string& hash) {
    if (ledger.Revoke(hash)) {
        ledger.Save();
        SetStatus("Code revoked");
    }
}

void KeyForgeApp::Tick(float deltaTime) {
    dt = ImClamp(deltaTime, 0.0f, 0.1f);
    time += dt;
    if (statusTimer > 0.0f) statusTimer -= dt;
}

void KeyForgeApp::Draw() { DrawKeyForgeApp(*this); }

// ---------------------------------------------------------------------------
// Rendering
// ---------------------------------------------------------------------------

namespace {

void DrawGate(KeyForgeApp& app) {
    ImGuiWindow* window = ImGui::GetCurrentWindow();
    ImDrawList* dl = window->DrawList;
    const ImVec2 min = window->Pos;
    const ImVec2 max = ImVec2(min.x + window->Size.x, min.y + window->Size.y);
    const float width = window->Size.x;
    const float height = window->Size.y;

    ui::Backdrop(dl, min, max, app.time, true);
    ui::RadialGlow(dl, ImVec2(min.x + width * 0.5f, min.y + height * 0.28f), width * 0.55f,
                   Colors().brand, 0.75f);

    const float cardWidth = ImMin(U(430.0f), width - U(60.0f));
    const ImVec2 cardMin(min.x + (width - cardWidth) * 0.5f, min.y + U(70.0f));
    const float cardHeight = height - U(120.0f);

    dl->AddRectFilled(cardMin, ImVec2(cardMin.x + cardWidth, cardMin.y + cardHeight),
                      ui::Fade(Colors().surface, 0.96f), U(16.0f));
    dl->AddRect(cardMin, ImVec2(cardMin.x + cardWidth, cardMin.y + cardHeight), Colors().stroke,
                U(16.0f), 0, U(1.0f));

    ImGui::SetCursorScreenPos(ImVec2(cardMin.x, cardMin.y + U(26.0f)));
    ui::LogoImage(ImVec2(U(58.0f), U(58.0f)), 0.5f);

    ImGui::SetCursorScreenPos(ImVec2(cardMin.x + U(28.0f), cardMin.y + U(96.0f)));
    ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0, 0, 0, 0));
    ImGui::BeginChild("##gate_body", ImVec2(cardWidth - U(56.0f), cardHeight - U(120.0f)),
                      ImGuiChildFlags_None, ImGuiWindowFlags_NoScrollbar);

    ui::Overline("OWNER TOOL  ·  ASTRA PRODUCTIONS", Colors().brand);
    ui::VSpace(8.0f);
    if (app.stage == KeyForgeApp::Stage::CreatePassphrase) {
        ui::Text(Text().h2, Colors().text, "Create the owner passphrase");
        ui::VSpace(6.0f);
        ui::TextWrapped(
            "KeyForge mints the one-time codes that unlock Astra Studio accounts. This passphrase "
            "protects the issue ledger on this machine - store it somewhere safe.",
            Colors().textDim, cardWidth - U(56.0f));
        ui::VSpace(18.0f);

        ui::FieldLabel("OWNER PASSPHRASE", "12+ characters");
        ui::PasswordField("##owner_pass", app.passphrase, app.revealPassphrase, "Choose a passphrase");
        ui::VSpace(10.0f);
        ui::FieldLabel("CONFIRM PASSPHRASE");
        ui::PasswordField("##owner_pass2", app.passphraseConfirm, app.revealPassphrase,
                          "Repeat the passphrase");
    } else {
        ui::Text(Text().h2, Colors().text, "Unlock Astra KeyForge");
        ui::VSpace(6.0f);
        ui::TextWrapped("Enter the owner passphrase to issue or revoke engine access codes.",
                        Colors().textDim, cardWidth - U(56.0f));
        ui::VSpace(18.0f);
        ui::FieldLabel("OWNER PASSPHRASE");
        ui::PasswordField("##owner_unlock", app.passphrase, app.revealPassphrase, "Passphrase");
    }

    if (!app.gateError.empty()) {
        ui::VSpace(12.0f);
        screens::InfoBanner(app.gateError, Icon::Warning, Colors().danger);
    }

    ui::VSpace(18.0f);
    const char* label = app.stage == KeyForgeApp::Stage::CreatePassphrase ? "Create and unlock"
                                                                         : "Unlock KeyForge";
    if (ui::PrimaryButton(label, ImVec2(cardWidth - U(56.0f), U(44.0f)), true, Icon::Key)) {
        if (app.stage == KeyForgeApp::Stage::CreatePassphrase) {
            app.CreateOwnerPassphrase();
        } else {
            app.Unlock();
        }
    }
    ui::VSpace(14.0f);
    ui::TextFaint("Master key %s", license::MasterKeyFingerprint().c_str());
    ui::VSpace(6.0f);
    ui::TextWrapped("Owner-only. Never distribute this executable with the engine build.",
                    ui::Colors().warn, cardWidth - U(56.0f));

    ImGui::EndChild();
    ImGui::PopStyleColor();
}

void DrawGenerate(KeyForgeApp& app) {
    ui::Text(Text().h1, Colors().text, "Access codes");
    ui::VSpace(2.0f);
    ui::TextWrapped("Issue single-use codes that register an Astra Studio account.",
                    Colors().textDim, U(560.0f));
    ui::VSpace(18.0f);

    const float formWidth = U(330.0f);
    const float kFormHeight = U(540.0f);
    ui::BeginCard("##generate_form", ImVec2(formWidth, kFormHeight), Colors().surface);
    ui::Overline("NEW CODE BATCH", Colors().textFaint);
    ui::VSpace(10.0f);

    ui::FieldLabel("ROLE");
    static const char* kRoles[] = {"User", "Staff", "Owner"};
    ui::Segmented("##role", &app.roleIndex, kRoles, 3, formWidth - U(28.0f));
    ui::VSpace(12.0f);

    ui::FieldLabel("VALID FOR");
    static std::vector<const char*> durationLabels;
    durationLabels.clear();
    for (const auto& option : license::DurationOptions()) durationLabels.push_back(option.label);
    ui::Select("##duration", &app.durationIndex, durationLabels.data(),
               static_cast<int>(durationLabels.size()), formWidth - U(28.0f));
    ui::VSpace(12.0f);

    ui::FieldLabel("QUANTITY", "max 10");
    ui::Stepper("##quantity", &app.quantity, 1, 10, formWidth - U(28.0f));
    ui::VSpace(12.0f);

    ui::FieldLabel("NOTE", "optional");
    ui::TextField("##note", app.note, "e.g. Anna - QA build");
    ui::VSpace(16.0f);

    if (ui::PrimaryButton("Generate codes", ImVec2(formWidth - U(28.0f), U(42.0f)), true,
                          Icon::Sparkle)) {
        app.GenerateCodes();
    }
    ui::EndCard();

    ImGui::SameLine(0.0f, U(18.0f));

    // Results.
    const float resultsWidth = ImGui::GetContentRegionAvail().x;
    ui::BeginCard("##results", ImVec2(resultsWidth, kFormHeight), Colors().surface);
    ui::Overline("LATEST BATCH", Colors().textFaint);
    ui::VSpace(8.0f);

    if (app.lastBatch.empty()) {
        ui::VSpace(20.0f);
        ui::EmptyState(Icon::Key, "No codes yet",
                       "Generated codes appear here, ready to copy or export.");
    } else {
        for (const auto& row : app.lastBatch) {
            ImGui::PushID(row.code.text.c_str());
            const ImVec2 pos = ImGui::GetCursorScreenPos();
            const float rowWidth = ImGui::GetContentRegionAvail().x;
            ImDrawList* dl = ImGui::GetWindowDrawList();
            dl->AddRectFilled(pos, ImVec2(pos.x + rowWidth, pos.y + U(46.0f)),
                              Colors().surfaceSunken, U(10.0f));

            dl->AddText(Text().mono, Text().mono->FontSize, ImVec2(pos.x + U(12.0f), pos.y + U(6.0f)),
                        Colors().text, row.code.text.c_str());
            dl->AddText(Text().tiny, Text().tiny->FontSize,
                        ImVec2(pos.x + U(12.0f), pos.y + U(26.0f)), Colors().textFaint,
                        row.code.Description().c_str());

            ImGui::SetCursorScreenPos(ImVec2(pos.x + rowWidth - U(72.0f), pos.y + U(9.0f)));
            if (ui::IconButton("##copy", Icon::Copy, "Copy code", false, 26.0f)) {
                app.CopyToClipboard(row.code.text, "Code");
            }
            ImGui::SetCursorScreenPos(ImVec2(pos.x + rowWidth - U(38.0f), pos.y + U(9.0f)));
            if (ui::IconButton("##revoke", Icon::Trash, "Revoke", false, 26.0f)) {
                app.Revoke(license::CodeHash(row.code.text));
            }
            ImGui::SetCursorScreenPos(ImVec2(pos.x, pos.y + U(50.0f)));
            ImGui::Dummy(ImVec2(rowWidth, U(1.0f)));
            ImGui::PopID();
        }

        ui::VSpace(6.0f);
        if (ui::SecondaryButton("Copy all", ImVec2(U(120.0f), U(32.0f)), true, Icon::Copy)) {
            std::string all;
            for (const auto& row : app.lastBatch) all += row.code.text + "\n";
            app.CopyToClipboard(all, "Batch");
        }
        ImGui::SameLine(0.0f, U(8.0f));
        if (ui::SecondaryButton("Export .txt", ImVec2(U(130.0f), U(32.0f)), true, Icon::Download)) {
            app.ExportBatch(false);
        }
        ImGui::SameLine(0.0f, U(8.0f));
        if (ui::GhostButton("Export .json", ImVec2(U(130.0f), U(32.0f)), true, Icon::Download)) {
            app.ExportBatch(true);
        }
    }
    ui::EndCard();
}

void DrawLedger(KeyForgeApp& app) {
    ui::Text(Text().h1, Colors().text, "Issue ledger");
    ui::VSpace(2.0f);
    ui::TextFaint("Every code this machine has minted. Revoked codes stay visible for audit.");
    ui::VSpace(16.0f);

    static const char* kFilters[] = {"All", "Active", "Expired", "Revoked"};
    ui::Segmented("##ledger_filter", &app.ledgerFilter, kFilters, 4, U(300.0f));
    ImGui::SameLine(0.0f, U(12.0f));
    ui::SearchField("##ledger_search", app.ledgerSearch, U(240.0f), "Search notes, codes...");
    ui::VSpace(14.0f);

    const auto& codes = app.ledger.Codes();
    const std::int64_t now = time::NowUtc();

    if (ImGui::BeginTable("##ledger_table", 6,
                          ImGuiTableFlags_RowBg | ImGuiTableFlags_BordersInnerH |
                              ImGuiTableFlags_SizingStretchProp)) {
        ImGui::TableSetupColumn("Code", ImGuiTableColumnFlags_WidthStretch, 1.4f);
        ImGui::TableSetupColumn("Role", ImGuiTableColumnFlags_WidthStretch, 0.7f);
        ImGui::TableSetupColumn("Duration", ImGuiTableColumnFlags_WidthStretch, 0.9f);
        ImGui::TableSetupColumn("Issued", ImGuiTableColumnFlags_WidthStretch, 1.0f);
        ImGui::TableSetupColumn("Status", ImGuiTableColumnFlags_WidthStretch, 0.8f);
        ImGui::TableSetupColumn("", ImGuiTableColumnFlags_WidthFixed, U(64.0f));
        ImGui::TableHeadersRow();

        for (const auto& record : codes) {
            const bool expired = !record.neverExpires && record.expiryUtc > 0 &&
                                 now > record.expiryUtc;
            const char* status = record.revoked ? "Revoked" : (expired ? "Expired" : "Active");
            const ImU32 statusColor = record.revoked ? Colors().textFaint
                                       : (expired ? Colors().warn : Colors().success);
            if (app.ledgerFilter == 1 && (record.revoked || expired)) continue;
            if (app.ledgerFilter == 2 && (!expired || record.revoked)) continue;
            if (app.ledgerFilter == 3 && !record.revoked) continue;
            if (!app.ledgerSearch.empty()) {
                const std::string needle = str::ToLower(app.ledgerSearch);
                if (!str::Contains(str::ToLower(record.masked), needle) &&
                    !str::Contains(str::ToLower(record.note), needle) &&
                    !str::Contains(str::ToLower(record.code), needle)) {
                    continue;
                }
            }

            ImGui::TableNextRow();
            ImGui::TableSetColumnIndex(0);
            ImGui::PushStyleColor(ImGuiCol_Text, ui::ToVec4(Colors().text));
            ui::PushFont(Text().monoSmall);
            ImGui::TextUnformatted(record.code.empty() ? record.masked.c_str() : record.code.c_str());
            ui::PopFont();
            ImGui::PopStyleColor();
            if (!record.note.empty()) {
                ImGui::PushStyleColor(ImGuiCol_Text, ui::ToVec4(ui::Fade(Colors().textDim, 0.82f)));
                ui::PushFont(Text().tiny);
                ImGui::TextUnformatted(record.note.c_str());
                ui::PopFont();
                ImGui::PopStyleColor();
            }

            ImGui::TableSetColumnIndex(1);
            ui::Badge(record.role.c_str(), record.role == "Staff" ? Colors().brand : Colors().info);
            ImGui::TableSetColumnIndex(2);
            ui::TextFaint("%s", record.duration.c_str());
            ImGui::TableSetColumnIndex(3);
            ui::TextFaint("%s", time::FormatUtc(record.issuedUtc).c_str());
            ImGui::TableSetColumnIndex(4);
            ui::Badge(status, statusColor);
            ImGui::TableSetColumnIndex(5);
            ImGui::PushID(record.hash.c_str());
            if (!record.revoked && ui::IconButton("##row_revoke", Icon::Trash, "Revoke", false, 24.0f)) {
                app.Revoke(record.hash);
            }
            ImGui::PopID();
        }
        ImGui::EndTable();
    }
    if (codes.empty()) {
        ui::VSpace(20.0f);
        ui::EmptyState(Icon::Database, "Ledger is empty",
                       "Codes you generate will be recorded here with their batch and note.");
    }
}

void DrawSettings(KeyForgeApp& app) {
    ui::Text(Text().h1, Colors().text, "KeyForge settings");
    ui::VSpace(2.0f);
    ui::TextFaint("Owner tool configuration and licensing diagnostics.");
    ui::VSpace(18.0f);

    ui::BeginCard("##kf_about", ImVec2(0.0f, U(210.0f)), Colors().surface);
    ui::Text(Text().h3, Colors().text, "Signing identity");
    ui::VSpace(8.0f);
    ui::KeyValue("Master key", license::MasterKeyFingerprint().c_str());
    ui::KeyValue("Code version", str::Format("%d", kAccessCodeVersion).c_str());
    ui::KeyValue("Codes issued", str::Format("%zu", app.ledger.Codes().size()).c_str());
    ui::KeyValue("Owner record", app.ownerPath.c_str());
    ui::VSpace(12.0f);
    screens::InfoBanner(
        "This build signs codes with a shared HMAC key so Studio can verify them offline. "
        "Engine 0.2 moves to Ed25519: Studio keeps only the public half.",
        Icon::Shield, Colors().warn);
    ui::EndCard();

    ui::VSpace(14.0f);
    ui::BeginCard("##kf_maintenance", ImVec2(0.0f, U(150.0f)), Colors().surface);
    ui::Text(Text().h3, Colors().text, "Maintenance");
    ui::VSpace(10.0f);
    if (ui::SecondaryButton("Reload ledger", ImVec2(U(180.0f), U(34.0f)), true, Icon::Refresh)) {
        app.ledger.Load();
        app.SetStatus("Ledger reloaded");
    }
    ImGui::SameLine(0.0f, U(10.0f));
    if (ui::GhostButton("Open data folder", ImVec2(U(190.0f), U(34.0f)), true, Icon::Folder)) {
        app.SetStatus(paths::KeyForgeDataDir());
    }
    ImGui::SameLine(0.0f, U(10.0f));
    if (ui::GhostButton("Copy fingerprint", ImVec2(U(190.0f), U(34.0f)), true, Icon::Copy)) {
        app.CopyToClipboard(license::MasterKeyFingerprint(), "Fingerprint");
    }
    ui::EndCard();
}

}  // namespace

void DrawKeyForgeApp(KeyForgeApp& app) {
    ImGuiIO& io = ImGui::GetIO();
    if (!app.initialized) app.Init();
    app.Tick(io.DeltaTime);

    const ImGuiViewport* viewport = ImGui::GetMainViewport();
    ImGui::SetNextWindowPos(viewport->Pos);
    ImGui::SetNextWindowSize(viewport->Size);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
    ImGui::PushStyleColor(ImGuiCol_WindowBg, ui::ToVec4(Colors().bg));
    ImGui::Begin("##keyforge_root", nullptr,
                 ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove |
                     ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoSavedSettings |
                     ImGuiWindowFlags_NoBringToFrontOnFocus);

    if (app.stage != KeyForgeApp::Stage::Ready) {
        DrawGate(app);
        ImGui::End();
        ImGui::PopStyleColor();
        ImGui::PopStyleVar();
        return;
    }

    ImGuiWindow* window = ImGui::GetCurrentWindow();
    ImDrawList* dl = window->DrawList;
    const ImVec2 min = window->Pos;
    const ImVec2 max = ImVec2(min.x + window->Size.x, min.y + window->Size.y);
    const float width = window->Size.x;
    const float height = window->Size.y;

    dl->AddRectFilled(min, max, Colors().bg);
    ui::RadialGlow(dl, ImVec2(min.x + width * 0.7f, min.y), width * 0.6f, Colors().brand, 0.22f);

    // Top bar.
    const float barHeight = U(58.0f);
    dl->AddRectFilled(min, ImVec2(min.x + width, min.y + barHeight), Colors().editorBar);
    dl->AddLine(ImVec2(min.x, min.y + barHeight), ImVec2(min.x + width, min.y + barHeight),
                Colors().stroke, U(1.0f));
    const float logoSize = U(30.0f);
    const ImVec2 logoPos(min.x + U(22.0f), min.y + (barHeight - logoSize) * 0.5f);
    if (const ui::branding::Image* logo = ui::branding::Logo(); logo && logo->texture != 0) {
        dl->AddImage(logo->texture, logoPos, ImVec2(logoPos.x + logoSize, logoPos.y + logoSize));
    }
    dl->AddText(Text().medium, Text().medium->FontSize, ImVec2(logoPos.x + logoSize + U(12.0f), min.y + U(12.0f)),
                Colors().text, "ASTRA KEYFORGE");
    dl->AddText(Text().tiny, Text().tiny->FontSize,
                ImVec2(logoPos.x + logoSize + U(12.0f), min.y + U(32.0f)), Colors().warn,
                "OWNER-ONLY TOOL");
    const std::string versionText = std::string("v") + kEngineVersion;
    const float versionWidth = Text().tiny->CalcTextSizeA(Text().tiny->FontSize, FLT_MAX, 0.0f,
                                                          versionText.c_str()).x;
    dl->AddText(Text().tiny, Text().tiny->FontSize,
                ImVec2(min.x + width - U(22.0f) - versionWidth, min.y + U(23.0f)),
                Colors().textFaint, versionText.c_str());

    // Sidebar tabs.
    const float sidebarWidth = U(196.0f);
    dl->AddRectFilled(ImVec2(min.x, min.y + barHeight), ImVec2(min.x + sidebarWidth, max.y),
                      Colors().surfaceSunken);
    dl->AddLine(ImVec2(min.x + sidebarWidth, min.y + barHeight),
                ImVec2(min.x + sidebarWidth, max.y), Colors().stroke, U(1.0f));

    static const char* kTabs[] = {"Generate", "Issue ledger", "Settings"};
    static const Icon kTabIcons[] = {Icon::Key, Icon::Database, Icon::Gear};
    float tabY = min.y + barHeight + U(16.0f);
    for (int i = 0; i < 3; ++i) {
        const ImRect bb(ImVec2(min.x + U(10.0f), tabY),
                        ImVec2(min.x + sidebarWidth - U(10.0f), tabY + U(36.0f)));
        ImGui::SetCursorScreenPos(bb.Min);
        ImGui::PushID(i);
        ImGui::InvisibleButton("##kf_tab", bb.GetSize());
        const bool hovered = ImGui::IsItemHovered();
        if (ImGui::IsItemClicked()) app.tab = static_cast<KeyForgeApp::Tab>(i);
        ImGui::PopID();
        const bool active = static_cast<int>(app.tab) == i;
        if (active) {
            dl->AddRectFilled(bb.Min, bb.Max, ui::Fade(Colors().brand, 0.15f), U(8.0f));
            dl->AddRectFilled(bb.Min, ImVec2(bb.Min.x + U(2.5f), bb.Max.y), Colors().brand, U(2.0f));
        } else if (hovered) {
            dl->AddRectFilled(bb.Min, bb.Max, Colors().surfaceHover, U(8.0f));
        }
        ui::DrawIcon(dl, kTabIcons[i], ImVec2(bb.Min.x + U(20.0f), bb.Min.y + U(18.0f)), U(14.0f),
                     active ? Colors().brand : Colors().textFaint, U(1.6f));
        dl->AddText(Text().small, Text().small->FontSize,
                    ImVec2(bb.Min.x + U(38.0f), bb.Min.y + U(10.0f)),
                    active ? Colors().text : Colors().textDim, kTabs[i]);
        tabY += U(42.0f);
    }

    if (!app.status.empty() && app.statusTimer > 0.0f) {
        ImGui::SetCursorScreenPos(ImVec2(min.x + U(12.0f), max.y - U(46.0f)));
        ImGui::PushStyleColor(ImGuiCol_Text, ui::ToVec4(Colors().success));
        ui::PushFont(Text().tiny);
        ImGui::PushTextWrapPos(min.x + sidebarWidth - U(16.0f));
        ImGui::TextUnformatted(app.status.c_str());
        ImGui::PopTextWrapPos();
        ui::PopFont();
        ImGui::PopStyleColor();
    }

    // Content.
    ImGui::SetCursorScreenPos(ImVec2(min.x + sidebarWidth + U(26.0f), min.y + barHeight + U(24.0f)));
    ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0, 0, 0, 0));
    ImGui::BeginChild("##kf_content",
                      ImVec2(width - sidebarWidth - U(52.0f), height - barHeight - U(48.0f)),
                      ImGuiChildFlags_None, ImGuiWindowFlags_None);
    switch (app.tab) {
        case KeyForgeApp::Tab::Generate: DrawGenerate(app); break;
        case KeyForgeApp::Tab::Ledger: DrawLedger(app); break;
        case KeyForgeApp::Tab::Settings: DrawSettings(app); break;
    }
    ImGui::EndChild();
    ImGui::PopStyleColor();

    ImGui::End();
    ImGui::PopStyleColor();
    ImGui::PopStyleVar();
}

}  // namespace astra::app
