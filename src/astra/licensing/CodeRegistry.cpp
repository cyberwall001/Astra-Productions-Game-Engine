#include "astra/licensing/CodeRegistry.h"

#include "astra/core/FileIO.h"
#include "astra/core/Log.h"
#include "astra/core/Paths.h"
#include "astra/data/Json.h"
#include "astra/crypto/Encoding.h"
#include "astra/crypto/Sha256.h"
#include "astra/licensing/MasterKey.h"

namespace astra::license {
namespace {

constexpr int kRegistrySchema = 1;

std::string HashOf(std::string_view normalizedCode) {
    return crypto::Sha256::HexDigest(normalizedCode);
}

}  // namespace

std::string CodeHash(std::string_view normalizedCode) {
    return HashOf(crypto::NormalizeCode(normalizedCode));
}

std::string MaskCode(std::string_view normalizedCode) {
    const std::string normalized = crypto::NormalizeCode(normalizedCode);
    if (normalized.size() < 8) return std::string(normalized);
    return normalized.substr(0, 4) + "-****-****-" + normalized.substr(normalized.size() - 4);
}

// ---------------------------------------------------------------------------
// CodeRegistry
// ---------------------------------------------------------------------------

CodeRegistry::CodeRegistry(std::string path) : path_(std::move(path)) {}

CodeRegistry CodeRegistry::ForEngine() {
    return CodeRegistry(paths::Join(paths::EngineDataDir(), "used_codes.json"));
}

bool CodeRegistry::Load() {
    used_.clear();
    std::string text;
    if (!fs::ReadTextFile(path_, text)) return false;

    json::Value root;
    std::string error;
    if (!json::Value::Parse(text, root, &error)) {
        log::Error("license", "used_codes.json is corrupt: " + error);
        return false;
    }
    for (const auto& item : root["used"].Elements()) {
        UsedCode record;
        record.hash = item["hash"].AsString();
        record.masked = item["masked"].AsString();
        record.accountId = item["account"].AsString();
        record.usedUtc = item["used"].AsInt();
        if (!record.hash.empty()) used_.push_back(std::move(record));
    }
    return true;
}

bool CodeRegistry::Save() const {
    json::Value root = json::Value::MakeObject();
    root.Set("schema", json::Value(kRegistrySchema));
    json::Value list = json::Value::MakeArray();
    for (const auto& record : used_) {
        json::Value item = json::Value::MakeObject();
        item.Set("hash", json::Value(record.hash));
        item.Set("masked", json::Value(record.masked));
        item.Set("account", json::Value(record.accountId));
        item.Set("used", json::Value(record.usedUtc));
        list.Push(std::move(item));
    }
    root.Set("used", std::move(list));
    return fs::WriteTextFileAtomic(path_, root.Dump(true));
}

bool CodeRegistry::IsUsed(std::string_view normalizedCode) const {
    const std::string hash = HashOf(normalizedCode);
    for (const auto& record : used_) {
        if (record.hash == hash) return true;
    }
    return false;
}

bool CodeRegistry::MarkUsed(std::string_view normalizedCode, std::string_view accountId,
                            std::int64_t nowUtc) {
    const std::string normalized = crypto::NormalizeCode(normalizedCode);
    if (normalized.empty()) return false;
    if (IsUsed(normalized)) return false;

    UsedCode record;
    record.hash = HashOf(normalized);
    record.masked = MaskCode(normalized);
    record.accountId = std::string(accountId);
    record.usedUtc = nowUtc;
    used_.push_back(std::move(record));
    return Save();
}

// ---------------------------------------------------------------------------
// IssuedLedger
// ---------------------------------------------------------------------------

IssuedLedger::IssuedLedger()
    : path_(paths::Join(paths::KeyForgeDataDir(), "issued_codes.json")) {}

IssuedLedger::IssuedLedger(std::string path) : path_(std::move(path)) {}

IssuedLedger IssuedLedger::ForKeyForge() {
    return IssuedLedger(paths::Join(paths::KeyForgeDataDir(), "issued_codes.json"));
}

bool IssuedLedger::Load() {
    codes_.clear();
    std::string text;
    if (!fs::ReadTextFile(path_, text)) return false;

    json::Value root;
    std::string error;
    if (!json::Value::Parse(text, root, &error)) {
        log::Error("keyforge", "issued_codes.json is corrupt: " + error);
        return false;
    }
    for (const auto& item : root["codes"].Elements()) {
        IssuedCode record;
        record.hash = item["hash"].AsString();
        record.masked = item["masked"].AsString();
        record.code = item["code"].AsString();
        record.role = item["role"].AsString("User");
        record.duration = item["duration"].AsString("30 days");
        record.note = item["note"].AsString();
        record.batch = item["batch"].AsString();
        record.issuedUtc = item["issued"].AsInt();
        record.expiryUtc = item["expiry"].AsInt();
        record.neverExpires = item["never"].AsBool();
        record.revoked = item["revoked"].AsBool();
        if (!record.hash.empty()) codes_.push_back(std::move(record));
    }
    return true;
}

bool IssuedLedger::Save() const {
    json::Value root = json::Value::MakeObject();
    root.Set("schema", json::Value(kRegistrySchema));
    root.Set("fingerprint", json::Value(MasterKeyFingerprint()));
    json::Value list = json::Value::MakeArray();
    for (const auto& record : codes_) {
        json::Value item = json::Value::MakeObject();
        item.Set("hash", json::Value(record.hash));
        item.Set("masked", json::Value(record.masked));
        item.Set("code", json::Value(record.code));
        item.Set("role", json::Value(record.role));
        item.Set("duration", json::Value(record.duration));
        item.Set("note", json::Value(record.note));
        item.Set("batch", json::Value(record.batch));
        item.Set("issued", json::Value(record.issuedUtc));
        item.Set("expiry", json::Value(record.expiryUtc));
        item.Set("never", json::Value(record.neverExpires));
        item.Set("revoked", json::Value(record.revoked));
        list.Push(std::move(item));
    }
    root.Set("codes", std::move(list));
    return fs::WriteTextFileAtomic(path_, root.Dump(true));
}

void IssuedLedger::Add(const IssuedCode& record) {
    codes_.insert(codes_.begin(), record);
}

bool IssuedLedger::Revoke(std::string_view hash) {
    for (auto& record : codes_) {
        if (record.hash == hash) {
            record.revoked = true;
            return true;
        }
    }
    return false;
}

std::vector<IssuedCode> IssuedLedger::Active() const {
    std::vector<IssuedCode> out;
    for (const auto& record : codes_) {
        if (!record.revoked) out.push_back(record);
    }
    return out;
}

}  // namespace astra::license
