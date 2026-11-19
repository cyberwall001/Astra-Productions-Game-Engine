#include "astra/app/Settings.h"

#include <algorithm>

#include "astra/core/FileIO.h"
#include "astra/core/Paths.h"
#include "astra/data/Json.h"

namespace astra::app {

std::string AppSettings::Serialize() const {
    json::Value root = json::Value::MakeObject();
    root.Set("schema", json::Value(1));

    json::Value appearance = json::Value::MakeObject();
    appearance.Set("uiScale", json::Value(static_cast<double>(uiScale)));
    appearance.Set("density", json::Value(static_cast<int>(density)));
    appearance.Set("reducedMotion", json::Value(reducedMotion));
    appearance.Set("tooltips", json::Value(showTooltips));
    root.Set("appearance", std::move(appearance));

    json::Value launcher = json::Value::MakeObject();
    launcher.Set("rememberAccount", json::Value(rememberAccount));
    launcher.Set("skipBoot", json::Value(skipBootScreen));
    launcher.Set("demoWorkspace", json::Value(allowDemoWorkspace));
    launcher.Set("githubSignIn", json::Value(signInWithGithub));
    root.Set("launcher", std::move(launcher));

    json::Value github = json::Value::MakeObject();
    github.Set("token", json::Value(githubTokenEncoded));
    github.Set("login", json::Value(githubLogin));
    github.Set("org", json::Value(githubDefaultOrg));
    github.Set("preferProjects", json::Value(preferGithubProjects));
    root.Set("github", std::move(github));

    json::Value editor = json::Value::MakeObject();
    editor.Set("activeProject", json::Value(activeProjectPath));
    editor.Set("viewportPlaceholder", json::Value(showViewportPlaceholder));
    json::Value recent = json::Value::MakeArray();
    for (const auto& path : recentProjects) recent.Push(json::Value(path));
    editor.Set("recent", std::move(recent));
    root.Set("editor", std::move(editor));

    root.Set("verboseLogging", json::Value(verboseLogging));
    return root.Dump(true);
}

AppSettings AppSettings::Deserialize(std::string_view jsonText) {
    AppSettings settings;
    json::Value root;
    if (!json::Value::Parse(jsonText, root)) return settings;

    const json::Value& appearance = root["appearance"];
    settings.uiScale = static_cast<float>(appearance["uiScale"].AsNumber(1.0));
    settings.density = static_cast<UiDensity>(appearance["density"].AsInt(1));
    settings.reducedMotion = appearance["reducedMotion"].AsBool(false);
    settings.showTooltips = appearance["tooltips"].AsBool(true);

    const json::Value& launcher = root["launcher"];
    settings.rememberAccount = launcher["rememberAccount"].AsBool(true);
    settings.skipBootScreen = launcher["skipBoot"].AsBool(false);
    settings.allowDemoWorkspace = launcher["demoWorkspace"].AsBool(true);
    settings.signInWithGithub = launcher["githubSignIn"].AsBool(false);

    const json::Value& github = root["github"];
    settings.githubTokenEncoded = github["token"].AsString();
    settings.githubLogin = github["login"].AsString();
    settings.githubDefaultOrg = github["org"].AsString();
    settings.preferGithubProjects = github["preferProjects"].AsBool(true);

    const json::Value& editor = root["editor"];
    settings.activeProjectPath = editor["activeProject"].AsString();
    settings.showViewportPlaceholder = editor["viewportPlaceholder"].AsBool(true);
    for (const auto& item : editor["recent"].Elements()) {
        const std::string path = item.AsString();
        if (!path.empty()) settings.recentProjects.push_back(path);
    }

    settings.verboseLogging = root["verboseLogging"].AsBool(false);
    return settings;
}

void AppSettings::PushRecentProject(const std::string& path, std::size_t maxEntries) {
    recentProjects.erase(std::remove(recentProjects.begin(), recentProjects.end(), path),
                         recentProjects.end());
    recentProjects.insert(recentProjects.begin(), path);
    if (recentProjects.size() > maxEntries) recentProjects.resize(maxEntries);
}

SettingsStore::SettingsStore() : path_(paths::Join(paths::EngineDataDir(), "settings.json")) {}

SettingsStore SettingsStore::ForEngine() { return SettingsStore(); }

bool SettingsStore::Load() {
    std::string text;
    if (!fs::ReadTextFile(path_, text)) return false;
    settings_ = AppSettings::Deserialize(text);
    return true;
}

bool SettingsStore::Save() const { return fs::WriteTextFileAtomic(path_, settings_.Serialize()); }

}  // namespace astra::app
