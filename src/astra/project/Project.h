// project/Project.h - the on-disk project descriptor (astra.project.json) and helpers
// for creating/opening/discovering projects.
#pragma once

#include <string>
#include <vector>
#include <cstdint>
#include <string_view>

#include "astra/scene/Scene.h"

namespace astra::project {

inline constexpr const char* kProjectFileName = "astra.project.json";

struct ProjectInfo {
    std::string name;
    std::string rootPath;         // absolute folder
    std::string projectFilePath;  // <root>/astra.project.json
    std::string engineVersion;
    std::string defaultScene = "scenes/main.astra.scene";
    std::string repoFullName;     // when cloned from GitHub
    std::string description;
    std::int64_t lastOpenedUtc = 0;

    bool isValid() const { return !name.empty() && !rootPath.empty(); }
    std::string ScenePath(std::string_view sceneName = {}) const;
};

// Reads <dir>/astra.project.json.
bool Read(const std::string& dir, ProjectInfo& out, std::string* error = nullptr);

// Writes the descriptor, creating the folder layout (scenes/, assets/, content/).
bool Write(const ProjectInfo& info);

// Creates a new project folder with a starter scene.
bool Create(const std::string& parentDirectory, const std::string& name, ProjectInfo& out,
            std::string* error = nullptr);

// True when `dir` contains astra.project.json.
bool IsProjectDirectory(const std::string& dir);

// Loads / saves scenes inside a project.
bool SaveScene(const ProjectInfo& info, const scene::Scene& scene, std::string* error = nullptr);
bool LoadScene(const ProjectInfo& info, scene::Scene& scene, std::string* error = nullptr);

// Recent project list persisted next to the launcher settings.
class RecentProjects {
public:
    RecentProjects();  // defaults to <engine data>/recent_projects.json
    explicit RecentProjects(std::string path);
    static RecentProjects ForEngine();

    bool Load();
    bool Save() const;

    void Touch(const ProjectInfo& info);  // insert/move to front
    void Remove(const std::string& rootPath);
    std::vector<ProjectInfo> Entries() const { return entries_; }

private:
    std::string path_;
    std::vector<ProjectInfo> entries_;
};

}  // namespace astra::project
