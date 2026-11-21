// script/Host.h - the engine API AstraScript scripts run against.
//
// The host owns the interpreter for a run, installs the `scene`, `mesh`, `image`, `math`,
// `string` and `io` namespaces, and keeps the in-memory meshes/images the script builds.
// `mesh.register()` copies a working mesh into the scene's mesh library so actors can use it,
// which is how a script turns code into geometry that survives a save.
#pragma once

#include <functional>
#include <string>
#include <string_view>
#include <vector>

#include "astra/image/Image.h"
#include "astra/scene/Scene.h"
#include "astra/script/Interpreter.h"

namespace astra::script {

struct ScriptRun {
    RunResult result;
    std::vector<std::string> output;
    std::string name;
};

class Host {
public:
    using OutputSink = std::function<void(const std::string&)>;

    explicit Host(scene::Scene& scene);

    // Directory relative paths resolve against (usually the project root).
    void SetWorkingDirectory(std::string directory);
    const std::string& WorkingDirectory() const { return workingDirectory_; }

    void SetOutputSink(OutputSink sink);
    void SetStepLimit(long long limit) { stepLimit_ = limit; }

    // Runs source at global scope. The scene is edited in place.
    ScriptRun Run(std::string_view source, std::string name = "script");
    ScriptRun RunFile(const std::string& path);

    // Drops generated meshes/images (the scene keeps whatever was registered).
    void Reset();

    const std::vector<image::Buffer>& Images() const { return images_; }
    const std::vector<std::string>& ImageNames() const { return imageNames_; }
    std::size_t MeshObjectCount() const { return meshes_.size(); }
    const std::vector<scene::MeshAsset>& MeshObjects() const { return meshes_; }

    // Resolves a relative path against the working directory.
    std::string ResolvePath(const std::string& path) const;

private:
    void Install(Interpreter& interpreter);

    scene::Scene& scene_;
    std::string workingDirectory_;
    OutputSink sink_;
    long long stepLimit_ = 4000000;
    std::vector<scene::MeshAsset> meshes_;
    std::vector<image::Buffer> images_;
    std::vector<std::string> imageNames_;
    std::vector<std::string> output_;
};

// Names of the shapes/ shaders/ presets scripts can pass as strings.
const std::vector<std::string>& ScriptableShapes();
const std::vector<std::string>& ScriptableShaders();
const std::vector<std::string>& ScriptableParticles();

}  // namespace astra::script
