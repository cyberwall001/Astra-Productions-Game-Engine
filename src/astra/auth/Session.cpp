#include "astra/auth/Session.h"

#include "astra/core/FileIO.h"
#include "astra/core/Paths.h"
#include "astra/data/Json.h"

namespace astra::auth {

SessionStore::SessionStore(std::string path) : path_(std::move(path)) {}

SessionStore SessionStore::ForEngine() {
    return SessionStore(paths::Join(paths::EngineDataDir(), "session.json"));
}

bool SessionStore::Load() {
    data_.Clear();
    std::string text;
    if (!fs::ReadTextFile(path_, text)) return false;

    json::Value root;
    if (!json::Value::Parse(text, root)) return false;

    data_.remember = root["remember"].AsBool();
    data_.accountId = root["account"].AsString();
    data_.token = root["token"].AsString();
    data_.issuedUtc = root["issued"].AsInt();
    data_.expiresUtc = root["expires"].AsInt();
    return true;
}

bool SessionStore::Save() const {
    json::Value root = json::Value::MakeObject();
    root.Set("remember", json::Value(data_.remember));
    root.Set("account", json::Value(data_.accountId));
    root.Set("token", json::Value(data_.token));
    root.Set("issued", json::Value(data_.issuedUtc));
    root.Set("expires", json::Value(data_.expiresUtc));
    return fs::WriteTextFileAtomic(path_, root.Dump(true));
}

bool SessionStore::Clear() {
    data_.Clear();
    // An empty file is cheaper than teaching every reader about a deleted session.
    return fs::WriteTextFileAtomic(path_, "{}\n");
}

}  // namespace astra::auth
