#include "astra/project/Project.h"

#include <algorithm>

#include "astra/core/FileIO.h"
#include "astra/core/Log.h"
#include "astra/core/Paths.h"
#include "astra/core/Strings.h"
#include "astra/core/Time.h"
#include "astra/core/Version.h"
#include "astra/data/Json.h"

namespace astra::project {
namespace {

constexpr int kProjectSchema = 1;

json::Value InfoToJson(const ProjectInfo& info) {
    json::Value root = json::Value::MakeObject();
    root.Set("schema", json::Value(kProjectSchema));
    root.Set("name", json::Value(info.name));
    root.Set("engineVersion", json::Value(kEngineVersion));
    root.Set("defaultScene", json::Value(info.defaultScene));
    root.Set("repo", json::Value(info.repoFullName));
    root.Set("description", json::Value(info.description));
    root.Set("renderer", json::Value("pending"));  // milestone 1: no viewport yet
    json::Value layers = json::Value::MakeArray();
    layers.Push(json::Value("default"));
    layers.Push(json::Value("weather"));
    layers.Push(json::Value("gameplay"));
    root.Set("layers", std::move(layers));
    return root;
}

ProjectInfo InfoFromJson(const json::Value& root, const std::string& dir) {
    ProjectInfo info;
    info.name = root["name"].AsString(fs::FileName(dir));
    info.rootPath = dir;
    info.projectFilePath = paths::Join(dir, kProjectFileName);
    info.engineVersion = root["engineVersion"].AsString(kEngineVersion);
    info.defaultScene = root["defaultScene"].AsString("scenes/main.astra.scene");
    info.repoFullName = root["repo"].AsString();
    info.description = root["description"].AsString();
    return info;
}

}  // namespace

std::string ProjectInfo::ScenePath(std::string_view sceneName) const {
    const std::string relative =
        sceneName.empty() ? defaultScene : ("scenes/" + std::string(sceneName) + ".astra.scene");
    return paths::Join(rootPath, relative);
}

bool Read(const std::string& dir, ProjectInfo& out, std::string* error) {
    const std::string file = paths::Join(dir, kProjectFileName);
    std::string text;
    if (!fs::ReadTextFile(file, text)) {
        if (error) *error = "No " + std::string(kProjectFileName) + " in " + dir;
        return false;
    }
    json::Value root;
    std::string parseError;
    if (!json::Value::Parse(text, root, &parseError)) {
        if (error) *error = "Project file is not valid JSON: " + parseError;
        return false;
    }
    out = InfoFromJson(root, dir);
    return true;
}

bool Write(const ProjectInfo& info) {
    if (info.rootPath.empty()) return false;
    fs::CreateDirectories(info.rootPath);
    fs::CreateDirectories(paths::Join(info.rootPath, "scenes"));
    fs::CreateDirectories(paths::Join(info.rootPath, "assets"));
    fs::CreateDirectories(paths::Join(info.rootPath, "content"));

    ProjectInfo copy = info;
    copy.projectFilePath = paths::Join(info.rootPath, kProjectFileName);
    return fs::WriteTextFileAtomic(copy.projectFilePath, InfoToJson(copy).Dump(true));
}

bool Create(const std::string& parentDirectory, const std::string& name, ProjectInfo& out,
            std::string* error) {
    const std::string safe = str::SanitizeFileName(name);
    const std::string dir = paths::Join(parentDirectory, safe);
    if (fs::Exists(dir)) {
        if (error) *error = "A folder named " + safe + " already exists here";
        return false;
    }

    ProjectInfo info;
    info.name = str::Trim(name).empty() ? safe : str::Trim(name);
    info.rootPath = dir;
    info.projectFilePath = paths::Join(dir, kProjectFileName);
    info.engineVersion = kEngineVersion;
    info.defaultScene = "scenes/main.astra.scene";
    info.description = "New Astra project";

    if (!Write(info)) {
        if (error) *error = "Could not write the project folder";
        return false;
    }

    // New projects open on the weather template: the same storm rig the hub uses
    // for repositories, so a project is never an empty viewport on first launch.
    scene::Scene starter = scene::Scene::CreateStormScene(info.name);
    if (!SaveScene(info, starter, error)) return false;

    out = info;
    log::Info("project", "created project at " + dir);
    return true;
}

bool IsProjectDirectory(const std::string& dir) {
    return fs::Exists(paths::Join(dir, kProjectFileName));
}

bool SaveScene(const ProjectInfo& info, const scene::Scene& scene, std::string* error) {
    const std::string path = info.ScenePath();
    if (!fs::CreateDirectories(fs::ParentPath(path))) {
        if (error) *error = "Could not create the scenes folder";
        return false;
    }
    if (!fs::WriteTextFileAtomic(path, scene.ToJson())) {
        if (error) *error = "Could not write " + path;
        return false;
    }
    return true;
}

bool LoadScene(const ProjectInfo& info, scene::Scene& scene, std::string* error) {
    const std::string path = info.ScenePath();
    std::string text;
    if (!fs::ReadTextFile(path, text)) {
        if (error) *error = "Scene file is missing: " + path;
        return false;
    }
    if (!scene.FromJson(text, error)) return false;
    scene.SetSourcePath(path);
    return true;
}

// ---------------------------------------------------------------------------
// RecentProjects
// ---------------------------------------------------------------------------

RecentProjects::RecentProjects()
    : path_(paths::Join(paths::EngineDataDir(), "recent_projects.json")) {}

RecentProjects::RecentProjects(std::string path) : path_(std::move(path)) {}

RecentProjects RecentProjects::ForEngine() {
    return RecentProjects(paths::Join(paths::EngineDataDir(), "recent_projects.json"));
}

bool RecentProjects::Load() {
    entries_.clear();
    std::string text;
    if (!fs::ReadTextFile(path_, text)) return false;
    json::Value root;
    if (!json::Value::Parse(text, root)) return false;
    for (const auto& item : root["projects"].Elements()) {
        const std::string dir = item["path"].AsString();
        if (dir.empty()) continue;
        ProjectInfo info;
        if (Read(dir, info)) {
            info.lastOpenedUtc = item["opened"].AsInt();
            entries_.push_back(std::move(info));
        }
    }
    return true;
}

bool RecentProjects::Save() const {
    json::Value root = json::Value::MakeObject();
    json::Value list = json::Value::MakeArray();
    for (const auto& info : entries_) {
        json::Value item = json::Value::MakeObject();
        item.Set("path", json::Value(info.rootPath));
        item.Set("name", json::Value(info.name));
        item.Set("opened", json::Value(info.lastOpenedUtc));
        list.Push(std::move(item));
    }
    root.Set("projects", std::move(list));
    return fs::WriteTextFileAtomic(path_, root.Dump(true));
}

void RecentProjects::Touch(const ProjectInfo& info) {
    entries_.erase(std::remove_if(entries_.begin(), entries_.end(),
                                  [&](const ProjectInfo& e) { return e.rootPath == info.rootPath; }),
                   entries_.end());
    ProjectInfo copy = info;
    copy.lastOpenedUtc = time::NowUtc();
    entries_.insert(entries_.begin(), std::move(copy));
    if (entries_.size() > 10) entries_.resize(10);
    Save();
}

void RecentProjects::Remove(const std::string& rootPath) {
    entries_.erase(std::remove_if(entries_.begin(), entries_.end(),
                                  [&](const ProjectInfo& e) { return e.rootPath == rootPath; }),
                   entries_.end());
    Save();
}

}  // namespace astra::project
