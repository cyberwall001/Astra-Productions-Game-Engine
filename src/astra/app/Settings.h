// app/Settings.h - persistent launcher/editor preferences.
#pragma once

#include <string>
#include <vector>
#include <string_view>

namespace astra::app {

enum class UiDensity { Compact = 0, Comfortable = 1, Spacious = 2 };

struct AppSettings {
    // Appearance
    float uiScale = 1.0f;
    UiDensity density = UiDensity::Comfortable;
    bool reducedMotion = false;
    bool showTooltips = true;

    // Launcher
    bool rememberAccount = true;
    bool skipBootScreen = false;   // developer convenience
    bool allowDemoWorkspace = true;
    bool signInWithGithub = false;

    // GitHub
    std::string githubTokenEncoded;  // DPAPI-protected hex on Windows
    std::string githubLogin;
    std::string githubDefaultOrg;
    bool preferGithubProjects = true;

    // Editor
    std::string activeProjectPath;
    std::vector<std::string> recentProjects;
    bool showViewportPlaceholder = true;

    // Diagnostics
    bool verboseLogging = false;

    std::string Serialize() const;
    static AppSettings Deserialize(std::string_view json);

    void PushRecentProject(const std::string& path, std::size_t maxEntries = 8);
};

class SettingsStore {
public:
    SettingsStore();

    static SettingsStore ForEngine();

    bool Load();
    bool Save() const;

    AppSettings& Get() { return settings_; }
    const AppSettings& Get() const { return settings_; }

    const std::string& Path() const { return path_; }

private:
    std::string path_;
    AppSettings settings_;
};

}  // namespace astra::app
