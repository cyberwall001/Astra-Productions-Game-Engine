#include "astra/script/Host.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <random>

#include "astra/asset/MeshBuilder.h"
#include "astra/core/FileIO.h"
#include "astra/core/Strings.h"

namespace astra::script {
namespace {

using scene::Entity;
using scene::EntityId;
using scene::MeshShape;
using scene::Vec3;

Value Error(const std::string& message) {
    throw Interpreter::Error{message, 1, 1};
}

double Number(const Value& value, const char* what) {
    if (!value.IsNumber()) {
        Error(std::string(what) + " expects a number, got " + value.TypeName());
    }
    return value.number;
}

int Integer(const Value& value, const char* what) {
    return static_cast<int>(Number(value, what));
}

// Returns by value: callers bind `Text(Arg(a, 0), ...)` to a reference to a temporary
// Value, so handing back a reference would dangle.
std::string Text(const Value& value, const char* what) {
    if (!value.IsString()) {
        Error(std::string(what) + " expects a string, got " + value.TypeName());
    }
    return value.string;
}

Value Arg(const std::vector<Value>& arguments, std::size_t index) {
    return index < arguments.size() ? arguments[index] : Value(nullptr);
}

Vec3 Vector(const Value& value, const char* what) {
    if (value.IsArray() && value.array->items.size() >= 3) {
        return Vec3(static_cast<float>(Number(value.array->items[0], what)),
                    static_cast<float>(Number(value.array->items[1], what)),
                    static_cast<float>(Number(value.array->items[2], what)));
    }
    return Vec3(static_cast<float>(Number(value, what)), 0.0f, 0.0f);
}

Value MakeVector(const Vec3& v) {
    Value array = Value::MakeArray();
    array.array->items.push_back(Value(v.x));
    array.array->items.push_back(Value(v.y));
    array.array->items.push_back(Value(v.z));
    return array;
}

Value MakeColor(const Vec3& c) {
    Value array = Value::MakeArray();
    array.array->items.push_back(Value(c.x));
    array.array->items.push_back(Value(c.y));
    array.array->items.push_back(Value(c.z));
    return array;
}

image::Color ToImageColor(const std::vector<Value>& arguments, std::size_t start,
                          float defaultAlpha) {
    const float r = static_cast<float>(Number(Arg(arguments, start), "color"));
    const float g = static_cast<float>(Number(Arg(arguments, start + 1), "color"));
    const float b = static_cast<float>(Number(Arg(arguments, start + 2), "color"));
    const float a = arguments.size() > start + 3
                        ? static_cast<float>(Number(Arg(arguments, start + 3), "alpha"))
                        : defaultAlpha;
    return image::Color(r, g, b, a);
}

}  // namespace

const std::vector<std::string>& ScriptableShapes() {
    static const std::vector<std::string> shapes = {"cube",  "sphere",  "cylinder",
                                                    "cone",  "plane",   "custom",  "torus"};
    return shapes;
}

const std::vector<std::string>& ScriptableShaders() {
    static const std::vector<std::string> shaders = {"lit",      "unlit",   "checker", "stripes",
                                                     "noise",    "gradient", "hologram", "emissive"};
    return shaders;
}

const std::vector<std::string>& ScriptableParticles() {
    static const std::vector<std::string> presets = {"smoke", "sparks", "rain", "dust", "fire"};
    return presets;
}

Host::Host(scene::Scene& scene) : scene_(scene) {}

void Host::SetOutputSink(OutputSink sink) { sink_ = std::move(sink); }

void Host::SetWorkingDirectory(std::string directory) {
    workingDirectory_ = std::move(directory);
}

std::string Host::ResolvePath(const std::string& path) const {
    if (path.empty()) return path;
    if (path[0] == '/' || (path.size() > 1 && path[1] == ':')) return path;
    if (workingDirectory_.empty()) return path;
    std::string joined = workingDirectory_;
    if (joined.back() != '/' && joined.back() != '\\') joined += '/';
    joined += path;
    return joined;
}

void Host::Reset() {
    meshes_.clear();
    images_.clear();
    imageNames_.clear();
    output_.clear();
}

ScriptRun Host::RunFile(const std::string& path) {
    const std::string resolved = ResolvePath(path);
    std::string source;
    if (!fs::ReadTextFile(resolved, source)) {
        ScriptRun failed;
        failed.name = path;
        failed.result.error = "Could not read script: " + resolved;
        return failed;
    }
    return Run(source, fs::FileName(resolved));
}

// ---------------------------------------------------------------------------
// The API surface
// ---------------------------------------------------------------------------
void Host::Install(Interpreter& interpreter) {
    interpreter.SetPrintSink([this](const std::string& line) {
        output_.push_back(line);
        if (sink_) sink_(line);
    });

    // ---- print / conversions ----
    interpreter.Define("print", Value::MakeNative("print", [this](Interpreter&, const std::vector<Value>& args) {
        std::string line;
        for (std::size_t i = 0; i < args.size(); ++i) {
            if (i > 0) line += " ";
            line += args[i].ToString();
        }
        output_.push_back(line);
        if (sink_) sink_(line);
        return Value(nullptr);
    }));
    interpreter.Define("str", Value::MakeNative("str", [](Interpreter&, const std::vector<Value>& args) {
        return Value(Arg(args, 0).ToString());
    }));
    interpreter.Define("num", Value::MakeNative("num", [](Interpreter&, const std::vector<Value>& args) {
        const Value value = Arg(args, 0);
        if (value.IsNumber()) return value;
        if (value.IsBool()) return Value(value.boolValue ? 1.0 : 0.0);
        if (value.IsString()) return Value(std::atof(value.string.c_str()));
        return Value(0.0);
    }));
    interpreter.Define("type", Value::MakeNative("type", [](Interpreter&, const std::vector<Value>& args) {
        return Value(Arg(args, 0).TypeName());
    }));
    interpreter.Define("len", Value::MakeNative("len", [](Interpreter&, const std::vector<Value>& args) {
        const Value value = Arg(args, 0);
        if (value.IsArray()) return Value(static_cast<double>(value.array->items.size()));
        if (value.IsString()) return Value(static_cast<double>(value.string.size()));
        return Value(0.0);
    }));
    interpreter.Define("range", Value::MakeNative("range", [](Interpreter&, const std::vector<Value>& args) {
        const int count = std::max(0, Integer(Arg(args, 0), "range"));
        Value array = Value::MakeArray();
        for (int i = 0; i < count; ++i) array.array->items.push_back(Value(static_cast<double>(i)));
        return array;
    }));
    interpreter.Define("assert", Value::MakeNative("assert", [](Interpreter&, const std::vector<Value>& args) {
        if (!Arg(args, 0).Truthy()) Error("assertion failed: " + Arg(args, 1).ToString());
        return Value(true);
    }));

    // ---- math ----
    Value math = Value::MakeArray();  // placeholder replaced below
    (void)math;
    const auto define = [&interpreter](const char* name, NativeFunction fn) {
        interpreter.Define(name, Value::MakeNative(name, std::move(fn)));
    };
    define("abs", [](Interpreter&, const std::vector<Value>& a) { return Value(std::fabs(Number(Arg(a, 0), "abs"))); });
    define("floor", [](Interpreter&, const std::vector<Value>& a) { return Value(std::floor(Number(Arg(a, 0), "floor"))); });
    define("ceil", [](Interpreter&, const std::vector<Value>& a) { return Value(std::ceil(Number(Arg(a, 0), "ceil"))); });
    define("round", [](Interpreter&, const std::vector<Value>& a) { return Value(std::round(Number(Arg(a, 0), "round"))); });
    define("sqrt", [](Interpreter&, const std::vector<Value>& a) {
        const double v = Number(Arg(a, 0), "sqrt");
        if (v < 0.0) Error("sqrt of a negative number");
        return Value(std::sqrt(v));
    });
    define("pow", [](Interpreter&, const std::vector<Value>& a) {
        return Value(std::pow(Number(Arg(a, 0), "pow"), Number(Arg(a, 1), "pow")));
    });
    define("sin", [](Interpreter&, const std::vector<Value>& a) { return Value(std::sin(Number(Arg(a, 0), "sin"))); });
    define("cos", [](Interpreter&, const std::vector<Value>& a) { return Value(std::cos(Number(Arg(a, 0), "cos"))); });
    define("tan", [](Interpreter&, const std::vector<Value>& a) { return Value(std::tan(Number(Arg(a, 0), "tan"))); });
    define("atan2", [](Interpreter&, const std::vector<Value>& a) {
        return Value(std::atan2(Number(Arg(a, 0), "atan2"), Number(Arg(a, 1), "atan2")));
    });
    define("min", [](Interpreter&, const std::vector<Value>& a) {
        return Value(std::min(Number(Arg(a, 0), "min"), Number(Arg(a, 1), "min")));
    });
    define("max", [](Interpreter&, const std::vector<Value>& a) {
        return Value(std::max(Number(Arg(a, 0), "max"), Number(Arg(a, 1), "max")));
    });
    define("clamp", [](Interpreter&, const std::vector<Value>& a) {
        const double v = Number(Arg(a, 0), "clamp");
        const double lo = Number(Arg(a, 1), "clamp");
        const double hi = Number(Arg(a, 2), "clamp");
        return Value(std::min(std::max(v, lo), hi));
    });
    define("lerp", [](Interpreter&, const std::vector<Value>& a) {
        const double t = Number(Arg(a, 0), "lerp");
        return Value(Number(Arg(a, 1), "lerp") + (Number(Arg(a, 2), "lerp") - Number(Arg(a, 1), "lerp")) * t);
    });
    define("noise", [](Interpreter&, const std::vector<Value>& a) {
        return Value(static_cast<double>(
            image::ValueNoise2D(static_cast<float>(Number(Arg(a, 0), "noise")),
                                static_cast<float>(Number(Arg(a, 1), "noise")))));
    });
    define("fbm", [](Interpreter&, const std::vector<Value>& a) {
        const int octaves = a.size() > 2 ? Integer(Arg(a, 2), "fbm") : 4;
        return Value(static_cast<double>(image::FractalNoise2D(
            static_cast<float>(Number(Arg(a, 0), "fbm")),
            static_cast<float>(Number(Arg(a, 1), "fbm")), octaves)));
    });
    {
        auto rng = std::make_shared<std::mt19937>(1337u);
        std::uniform_real_distribution<double> distribution(0.0, 1.0);
        interpreter.Define("random", Value::MakeNative("random", [rng, distribution](Interpreter&, const std::vector<Value>& a) mutable {
            const double r = distribution(*rng);
            if (a.empty()) return Value(r);
            if (a.size() == 1) return Value(r * Number(Arg(a, 0), "random"));
            const double lo = Number(Arg(a, 0), "random");
            const double hi = Number(Arg(a, 1), "random");
            return Value(lo + r * (hi - lo));
        }));
    }
    interpreter.Define("PI", Value(3.14159265358979323846));
    interpreter.Define("TAU", Value(6.28318530717958647692));

    // ---- strings ----
    define("upper", [](Interpreter&, const std::vector<Value>& a) {
        std::string text = Text(Arg(a, 0), "upper");
        for (char& c : text) c = static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
        return Value(text);
    });
    define("lower", [](Interpreter&, const std::vector<Value>& a) {
        std::string text = Text(Arg(a, 0), "lower");
        for (char& c : text) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
        return Value(text);
    });
    define("substr", [](Interpreter&, const std::vector<Value>& a) {
        const std::string text = Text(Arg(a, 0), "substr");
        const int start = std::max(0, Integer(Arg(a, 1), "substr"));
        const int length = a.size() > 2 ? Integer(Arg(a, 2), "substr")
                                        : static_cast<int>(text.size()) - start;
        if (start >= static_cast<int>(text.size())) return Value("");
        return Value(text.substr(static_cast<std::size_t>(start),
                                 static_cast<std::size_t>(std::max(0, length))));
    });
    define("split", [](Interpreter&, const std::vector<Value>& a) {
        const std::string text = Text(Arg(a, 0), "split");
        const std::string separator = Text(Arg(a, 1), "split");
        Value array = Value::MakeArray();
        if (separator.empty()) {
            for (char c : text) array.array->items.push_back(Value(std::string(1, c)));
            return array;
        }
        std::size_t start = 0;
        while (true) {
            const std::size_t found = text.find(separator, start);
            if (found == std::string::npos) {
                array.array->items.push_back(Value(text.substr(start)));
                break;
            }
            array.array->items.push_back(Value(text.substr(start, found - start)));
            start = found + separator.size();
        }
        return array;
    });
    define("join", [](Interpreter&, const std::vector<Value>& a) {
        const Value array = Arg(a, 0);
        const std::string separator = a.size() > 1 ? Text(Arg(a, 1), "join") : "";
        if (!array.IsArray()) Error("join expects an array");
        std::string out;
        for (std::size_t i = 0; i < array.array->items.size(); ++i) {
            if (i > 0) out += separator;
            out += array.array->items[i].ToString();
        }
        return Value(out);
    });
    define("format", [](Interpreter&, const std::vector<Value>& a) {
        // Very small subset: {} placeholders replaced in order.
        const std::string pattern = Text(Arg(a, 0), "format");
        std::string out;
        std::size_t argIndex = 1;
        for (std::size_t i = 0; i < pattern.size(); ++i) {
            if (pattern[i] == '{' && i + 1 < pattern.size() && pattern[i + 1] == '}') {
                out += Arg(a, argIndex++).ToString();
                ++i;
            } else {
                out.push_back(pattern[i]);
            }
        }
        return Value(out);
    });

    // ---- io ----
    define("readText", [this](Interpreter&, const std::vector<Value>& a) {
        std::string text;
        const std::string path = ResolvePath(Text(Arg(a, 0), "readText"));
        if (!fs::ReadTextFile(path, text)) Error("readText could not open " + path);
        return Value(text);
    });
    define("writeText", [this](Interpreter&, const std::vector<Value>& a) {
        const std::string path = ResolvePath(Text(Arg(a, 0), "writeText"));
        const std::string content = Text(Arg(a, 1), "writeText");
        if (!fs::WriteTextFileAtomic(path, content)) Error("writeText could not write " + path);
        return Value(true);
    });
    define("listDir", [this](Interpreter&, const std::vector<Value>& a) {
        const std::string path = ResolvePath(Text(Arg(a, 0), "listDir"));
        Value array = Value::MakeArray();
        for (const auto& name : fs::ListDirectory(path)) array.array->items.push_back(Value(name));
        return array;
    });

    // ---- scene ----
    define("findActor", [this](Interpreter&, const std::vector<Value>& a) {
        Entity* entity = scene_.FindByName(Text(Arg(a, 0), "findActor"));
        return Value(static_cast<double>(entity ? entity->id : 0));
    });
    define("actorCount", [this](Interpreter&, const std::vector<Value>&) {
        return Value(static_cast<double>(scene_.Count()));
    });
    define("actors", [this](Interpreter&, const std::vector<Value>&) {
        Value array = Value::MakeArray();
        for (const auto& entity : scene_.Entities()) {
            array.array->items.push_back(Value(static_cast<double>(entity.id)));
        }
        return array;
    });
    define("spawnActor", [this](Interpreter&, const std::vector<Value>& a) {
        const std::string name = Text(Arg(a, 0), "spawnActor");
        const EntityId id = scene_.CreateEntity(name);
        Entity* entity = scene_.Find(id);
        if (!entity) Error("could not create actor");
        if (a.size() > 1 && a[1].IsString() && !a[1].string.empty()) {
            const std::string shape = a[1].string;
            if (shape == "custom") {
                // Geometry comes later through attachMesh.
                entity->Add(scene::Component::Mesh);
                entity->mesh.shape = MeshShape::Custom;
            } else {
                bool matched = false;
                int count = 0;
                const char* const* names = scene::MeshShapeNames(count);
                for (int i = 0; i < count; ++i) {
                    std::string candidate = names[i];
                    for (char& c : candidate) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
                    if (candidate == shape) {
                        entity->Add(scene::Component::Mesh);
                        entity->mesh.shape = static_cast<MeshShape>(i);
                        entity->mesh.baseColor = Vec3(0.72f, 0.74f, 0.82f);
                        entity->Add(scene::Component::Material);
                        matched = true;
                        break;
                    }
                }
                if (!matched) {
                    std::string valid;
                    for (const auto& candidate : ScriptableShapes()) {
                        if (!valid.empty()) valid += ", ";
                        valid += candidate;
                    }
                    Error("unknown shape '" + shape + "' (use one of: " + valid + ")");
                }
            }
        }
        if (a.size() > 4) {
            entity->transform.position = Vec3(static_cast<float>(Number(Arg(a, 2), "spawnActor")),
                                              static_cast<float>(Number(Arg(a, 3), "spawnActor")),
                                              static_cast<float>(Number(Arg(a, 4), "spawnActor")));
        }
        scene_.MarkDirty();
        return Value(static_cast<double>(id));
    });
    define("destroyActor", [this](Interpreter&, const std::vector<Value>& a) {
        const bool ok = scene_.DestroyEntity(static_cast<EntityId>(Integer(Arg(a, 0), "destroyActor")));
        scene_.MarkDirty();
        return Value(ok);
    });
    define("actorName", [this](Interpreter&, const std::vector<Value>& a) {
        const Entity* entity = scene_.Find(static_cast<EntityId>(Integer(Arg(a, 0), "actorName")));
        return Value(entity ? entity->name : std::string());
    });
    define("setPosition", [this](Interpreter&, const std::vector<Value>& a) {
        Entity* entity = scene_.Find(static_cast<EntityId>(Integer(Arg(a, 0), "setPosition")));
        if (!entity) Error("setPosition: no such actor");
        entity->transform.position = Vec3(static_cast<float>(Number(Arg(a, 1), "setPosition")),
                                          static_cast<float>(Number(Arg(a, 2), "setPosition")),
                                          static_cast<float>(Number(Arg(a, 3), "setPosition")));
        scene_.MarkDirty();
        return Value(true);
    });
    define("position", [this](Interpreter&, const std::vector<Value>& a) {
        const Entity* entity = scene_.Find(static_cast<EntityId>(Integer(Arg(a, 0), "position")));
        return MakeVector(entity ? entity->transform.position : Vec3());
    });
    define("setRotation", [this](Interpreter&, const std::vector<Value>& a) {
        Entity* entity = scene_.Find(static_cast<EntityId>(Integer(Arg(a, 0), "setRotation")));
        if (!entity) Error("setRotation: no such actor");
        entity->transform.rotation = Vec3(static_cast<float>(Number(Arg(a, 1), "setRotation")),
                                          static_cast<float>(Number(Arg(a, 2), "setRotation")),
                                          static_cast<float>(Number(Arg(a, 3), "setRotation")));
        scene_.MarkDirty();
        return Value(true);
    });
    define("rotation", [this](Interpreter&, const std::vector<Value>& a) {
        const Entity* entity = scene_.Find(static_cast<EntityId>(Integer(Arg(a, 0), "rotation")));
        return MakeVector(entity ? entity->transform.rotation : Vec3());
    });
    define("setScale", [this](Interpreter&, const std::vector<Value>& a) {
        Entity* entity = scene_.Find(static_cast<EntityId>(Integer(Arg(a, 0), "setScale")));
        if (!entity) Error("setScale: no such actor");
        entity->transform.scale = Vec3(static_cast<float>(Number(Arg(a, 1), "setScale")),
                                       static_cast<float>(Number(Arg(a, 2), "setScale")),
                                       static_cast<float>(Number(Arg(a, 3), "setScale")));
        scene_.MarkDirty();
        return Value(true);
    });
    define("scale", [this](Interpreter&, const std::vector<Value>& a) {
        const Entity* entity = scene_.Find(static_cast<EntityId>(Integer(Arg(a, 0), "scale")));
        return MakeVector(entity ? entity->transform.scale : Vec3(1.0f, 1.0f, 1.0f));
    });
    define("setColor", [this](Interpreter&, const std::vector<Value>& a) {
        Entity* entity = scene_.Find(static_cast<EntityId>(Integer(Arg(a, 0), "setColor")));
        if (!entity) Error("setColor: no such actor");
        const Vec3 color(static_cast<float>(Number(Arg(a, 1), "setColor")),
                         static_cast<float>(Number(Arg(a, 2), "setColor")),
                         static_cast<float>(Number(Arg(a, 3), "setColor")));
        entity->mesh.baseColor = color;
        entity->material.baseColor = color;
        scene_.MarkDirty();
        return Value(true);
    });
    define("color", [this](Interpreter&, const std::vector<Value>& a) {
        const Entity* entity = scene_.Find(static_cast<EntityId>(Integer(Arg(a, 0), "color")));
        return MakeColor(entity ? entity->mesh.baseColor : Vec3());
    });
    define("setMaterial", [this](Interpreter&, const std::vector<Value>& a) {
        Entity* entity = scene_.Find(static_cast<EntityId>(Integer(Arg(a, 0), "setMaterial")));
        if (!entity) Error("setMaterial: no such actor");
        if (!entity->Has(scene::Component::Material)) scene_.AddComponent(entity->id, scene::Component::Material);
        const std::string name = Text(Arg(a, 1), "setMaterial");
        const auto& shaders = ScriptableShaders();
        for (std::size_t i = 0; i < shaders.size(); ++i) {
            if (shaders[i] == name) {
                entity->material.shader = static_cast<scene::MaterialShader>(i);
                if (a.size() > 2) entity->material.roughness = static_cast<float>(Number(Arg(a, 2), "setMaterial"));
                if (a.size() > 3) entity->material.metallic = static_cast<float>(Number(Arg(a, 3), "setMaterial"));
                if (a.size() > 4) entity->material.emissive = static_cast<float>(Number(Arg(a, 4), "setMaterial"));
                scene_.MarkDirty();
                return Value(true);
            }
        }
        std::string valid;
        for (const auto& shader : shaders) {
            if (!valid.empty()) valid += ", ";
            valid += shader;
        }
        Error("unknown shader '" + name + "' (use one of: " + valid + ")");
        return Value(false);
    });
    define("setAccent", [this](Interpreter&, const std::vector<Value>& a) {
        Entity* entity = scene_.Find(static_cast<EntityId>(Integer(Arg(a, 0), "setAccent")));
        if (!entity) Error("setAccent: no such actor");
        if (!entity->Has(scene::Component::Material)) scene_.AddComponent(entity->id, scene::Component::Material);
        entity->material.accentColor = Vec3(static_cast<float>(Number(Arg(a, 1), "setAccent")),
                                            static_cast<float>(Number(Arg(a, 2), "setAccent")),
                                            static_cast<float>(Number(Arg(a, 3), "setAccent")));
        scene_.MarkDirty();
        return Value(true);
    });
    define("setParticles", [this](Interpreter&, const std::vector<Value>& a) {
        Entity* entity = scene_.Find(static_cast<EntityId>(Integer(Arg(a, 0), "setParticles")));
        if (!entity) Error("setParticles: no such actor");
        if (!entity->Has(scene::Component::Particles)) scene_.AddComponent(entity->id, scene::Component::Particles);
        const std::string name = Text(Arg(a, 1), "setParticles");
        const auto& presets = ScriptableParticles();
        for (std::size_t i = 0; i < presets.size(); ++i) {
            if (presets[i] == name) {
                entity->particles.preset = static_cast<scene::ParticlePreset>(i);
                scene::ApplyParticlePreset(entity->particles);
                if (a.size() > 2) entity->particles.rate = static_cast<float>(Number(Arg(a, 2), "setParticles"));
                scene_.MarkDirty();
                return Value(true);
            }
        }
        Error("unknown particle preset '" + name + "'");
        return Value(false);
    });
    define("attachMesh", [this](Interpreter&, const std::vector<Value>& a) {
        Entity* entity = scene_.Find(static_cast<EntityId>(Integer(Arg(a, 0), "attachMesh")));
        if (!entity) Error("attachMesh: no such actor");
        const u32 meshId = static_cast<u32>(Integer(Arg(a, 1), "attachMesh"));
        if (!scene_.Mesh(meshId)) Error("attachMesh: unknown mesh id");
        if (!entity->Has(scene::Component::Mesh)) scene_.AddComponent(entity->id, scene::Component::Mesh);
        entity->mesh.shape = MeshShape::Custom;
        entity->mesh.meshId = meshId;
        scene_.MarkDirty();
        return Value(true);
    });

    // ---- mesh building ----
    const auto pushMesh = [this](scene::MeshAsset mesh) {
        meshes_.push_back(std::move(mesh));
        return Value(static_cast<double>(meshes_.size()));
    };
    const auto meshAt = [this](const Value& value, const char* what) -> scene::MeshAsset& {
        const int index = Integer(value, what);
        if (index < 1 || index > static_cast<int>(meshes_.size())) {
            Error(std::string(what) + ": unknown mesh buffer " + std::to_string(index));
        }
        return meshes_[static_cast<std::size_t>(index - 1)];
    };

    define("meshCube", [pushMesh](Interpreter&, const std::vector<Value>&) {
        return pushMesh(asset::MakeCube());
    });
    define("meshSphere", [pushMesh](Interpreter&, const std::vector<Value>& a) {
        const int segments = a.size() > 0 ? Integer(Arg(a, 0), "meshSphere") : 24;
        const int rings = a.size() > 1 ? Integer(Arg(a, 1), "meshSphere") : 14;
        return pushMesh(asset::MakeSphere(segments, rings));
    });
    define("meshCylinder", [pushMesh](Interpreter&, const std::vector<Value>& a) {
        return pushMesh(asset::MakeCylinder(a.size() > 0 ? Integer(Arg(a, 0), "meshCylinder") : 24));
    });
    define("meshCone", [pushMesh](Interpreter&, const std::vector<Value>& a) {
        return pushMesh(asset::MakeCone(a.size() > 0 ? Integer(Arg(a, 0), "meshCone") : 24));
    });
    define("meshPlane", [pushMesh](Interpreter&, const std::vector<Value>& a) {
        return pushMesh(asset::MakePlane(a.size() > 0 ? Integer(Arg(a, 0), "meshPlane") : 8));
    });
    define("meshTorus", [pushMesh](Interpreter&, const std::vector<Value>& a) {
        return pushMesh(asset::MakeTorus(a.size() > 0 ? Integer(Arg(a, 0), "meshTorus") : 28,
                                         a.size() > 1 ? Integer(Arg(a, 1), "meshTorus") : 14,
                                         a.size() > 2 ? static_cast<float>(Number(Arg(a, 2), "meshTorus")) : 0.28f));
    });
    define("meshNew", [pushMesh](Interpreter&, const std::vector<Value>& a) {
        scene::MeshAsset mesh;
        mesh.name = a.empty() ? std::string("Script mesh") : Text(Arg(a, 0), "meshNew");
        return pushMesh(std::move(mesh));
    });
    define("meshVertexCount", [meshAt](Interpreter&, const std::vector<Value>& a) {
        return Value(static_cast<double>(meshAt(Arg(a, 0), "meshVertexCount").vertices.size()));
    });
    define("meshTriangleCount", [meshAt](Interpreter&, const std::vector<Value>& a) {
        return Value(static_cast<double>(meshAt(Arg(a, 0), "meshTriangleCount").TriangleCount()));
    });
    define("meshVertex", [meshAt](Interpreter&, const std::vector<Value>& a) {
        scene::MeshAsset& mesh = meshAt(Arg(a, 0), "meshVertex");
        const int index = Integer(Arg(a, 1), "meshVertex");
        if (index < 0 || index >= static_cast<int>(mesh.vertices.size())) Error("meshVertex: out of range");
        const auto& vertex = mesh.vertices[static_cast<std::size_t>(index)];
        Value array = Value::MakeArray();
        array.array->items.push_back(Value(vertex.position.x));
        array.array->items.push_back(Value(vertex.position.y));
        array.array->items.push_back(Value(vertex.position.z));
        array.array->items.push_back(Value(vertex.normal.x));
        array.array->items.push_back(Value(vertex.normal.y));
        array.array->items.push_back(Value(vertex.normal.z));
        array.array->items.push_back(Value(vertex.u));
        array.array->items.push_back(Value(vertex.v));
        return array;
    });
    define("meshSetVertex", [meshAt](Interpreter&, const std::vector<Value>& a) {
        scene::MeshAsset& mesh = meshAt(Arg(a, 0), "meshSetVertex");
        const int index = Integer(Arg(a, 1), "meshSetVertex");
        if (index < 0 || index >= static_cast<int>(mesh.vertices.size())) Error("meshSetVertex: out of range");
        mesh.vertices[static_cast<std::size_t>(index)].position =
            Vec3(static_cast<float>(Number(Arg(a, 2), "meshSetVertex")),
                 static_cast<float>(Number(Arg(a, 3), "meshSetVertex")),
                 static_cast<float>(Number(Arg(a, 4), "meshSetVertex")));
        return Value(true);
    });
    define("meshSetNormal", [meshAt](Interpreter&, const std::vector<Value>& a) {
        scene::MeshAsset& mesh = meshAt(Arg(a, 0), "meshSetNormal");
        const int index = Integer(Arg(a, 1), "meshSetNormal");
        if (index < 0 || index >= static_cast<int>(mesh.vertices.size())) Error("meshSetNormal: out of range");
        mesh.vertices[static_cast<std::size_t>(index)].normal =
            Vec3(static_cast<float>(Number(Arg(a, 2), "meshSetNormal")),
                 static_cast<float>(Number(Arg(a, 3), "meshSetNormal")),
                 static_cast<float>(Number(Arg(a, 4), "meshSetNormal")));
        return Value(true);
    });
    define("meshAddVertex", [meshAt](Interpreter&, const std::vector<Value>& a) {
        scene::MeshAsset& mesh = meshAt(Arg(a, 0), "meshAddVertex");
        scene::MeshVertex vertex;
        vertex.position = Vec3(static_cast<float>(Number(Arg(a, 1), "meshAddVertex")),
                               static_cast<float>(Number(Arg(a, 2), "meshAddVertex")),
                               static_cast<float>(Number(Arg(a, 3), "meshAddVertex")));
        vertex.normal = Vec3(0.0f, 1.0f, 0.0f);
        if (a.size() > 6) {
            vertex.normal = Vec3(static_cast<float>(Number(Arg(a, 4), "meshAddVertex")),
                                 static_cast<float>(Number(Arg(a, 5), "meshAddVertex")),
                                 static_cast<float>(Number(Arg(a, 6), "meshAddVertex")));
        }
        if (a.size() > 8) {
            vertex.u = static_cast<float>(Number(Arg(a, 7), "meshAddVertex"));
            vertex.v = static_cast<float>(Number(Arg(a, 8), "meshAddVertex"));
        }
        mesh.vertices.push_back(vertex);
        return Value(static_cast<double>(mesh.vertices.size() - 1));
    });
    define("meshAddTriangle", [meshAt](Interpreter&, const std::vector<Value>& a) {
        scene::MeshAsset& mesh = meshAt(Arg(a, 0), "meshAddTriangle");
        const int i0 = Integer(Arg(a, 1), "meshAddTriangle");
        const int i1 = Integer(Arg(a, 2), "meshAddTriangle");
        const int i2 = Integer(Arg(a, 3), "meshAddTriangle");
        const int count = static_cast<int>(mesh.vertices.size());
        if (i0 < 0 || i1 < 0 || i2 < 0 || i0 >= count || i1 >= count || i2 >= count) {
            Error("meshAddTriangle: vertex index out of range");
        }
        mesh.indices.push_back(static_cast<u32>(i0));
        mesh.indices.push_back(static_cast<u32>(i1));
        mesh.indices.push_back(static_cast<u32>(i2));
        return Value(true);
    });
    define("meshRecenter", [meshAt](Interpreter&, const std::vector<Value>& a) {
        asset::Recenter(meshAt(Arg(a, 0), "meshRecenter"));
        return Value(true);
    });
    define("meshFitUnit", [meshAt](Interpreter&, const std::vector<Value>& a) {
        asset::FitToUnitBox(meshAt(Arg(a, 0), "meshFitUnit"));
        return Value(true);
    });
    define("meshRecomputeNormals", [meshAt](Interpreter&, const std::vector<Value>& a) {
        meshAt(Arg(a, 0), "meshRecomputeNormals").RecomputeNormals();
        return Value(true);
    });
    define("meshSetColor", [meshAt, this](Interpreter&, const std::vector<Value>& a) {
        meshAt(Arg(a, 0), "meshSetColor").albedo =
            Vec3(static_cast<float>(Number(Arg(a, 1), "meshSetColor")),
                 static_cast<float>(Number(Arg(a, 2), "meshSetColor")),
                 static_cast<float>(Number(Arg(a, 3), "meshSetColor")));
        return Value(true);
    });
    define("meshRegister", [meshAt, this](Interpreter&, const std::vector<Value>& a) {
        scene::MeshAsset copy = meshAt(Arg(a, 0), "meshRegister");
        if (a.size() > 1) copy.name = Text(Arg(a, 1), "meshRegister");
        if (copy.Empty()) Error("meshRegister: mesh has no triangles");
        if (copy.name.empty()) copy.name = "Script mesh";
        const u32 id = scene_.AddMesh(copy);
        scene_.MarkDirty();
        return Value(static_cast<double>(id));
    });

    // ---- image editing ----
    const auto pushImage = [this](int width, int height, std::string name) {
        images_.emplace_back(std::max(1, width), std::max(1, height));
        images_.back().Clear(image::Color(0.0f, 0.0f, 0.0f, 1.0f));
        imageNames_.push_back(std::move(name));
        return Value(static_cast<double>(images_.size()));
    };
    const auto imageAt = [this](const Value& value, const char* what) -> image::Buffer& {
        const int index = Integer(value, what);
        if (index < 1 || index > static_cast<int>(images_.size())) {
            Error(std::string(what) + ": unknown image buffer " + std::to_string(index));
        }
        return images_[static_cast<std::size_t>(index - 1)];
    };

    define("imageNew", [pushImage](Interpreter&, const std::vector<Value>& a) {
        const int width = std::clamp(a.size() > 0 ? Integer(Arg(a, 0), "imageNew") : 256, 1, 2048);
        const int height = std::clamp(a.size() > 1 ? Integer(Arg(a, 1), "imageNew") : 256, 1, 2048);
        return pushImage(width, height, "image " + std::to_string(width) + "x" + std::to_string(height));
    });
    define("imageWidth", [imageAt](Interpreter&, const std::vector<Value>& a) {
        return Value(static_cast<double>(imageAt(Arg(a, 0), "imageWidth").width));
    });
    define("imageHeight", [imageAt](Interpreter&, const std::vector<Value>& a) {
        return Value(static_cast<double>(imageAt(Arg(a, 0), "imageHeight").height));
    });
    define("imageFill", [imageAt](Interpreter&, const std::vector<Value>& a) {
        imageAt(Arg(a, 0), "imageFill").Clear(ToImageColor(a, 1, 1.0f));
        return Value(true);
    });
    define("imageSet", [imageAt](Interpreter&, const std::vector<Value>& a) {
        image::Buffer& buffer = imageAt(Arg(a, 0), "imageSet");
        buffer.SetPixel(Integer(Arg(a, 1), "imageSet"), Integer(Arg(a, 2), "imageSet"),
                        ToImageColor(a, 3, 1.0f));
        return Value(true);
    });
    define("imageGet", [imageAt](Interpreter&, const std::vector<Value>& a) {
        image::Buffer& buffer = imageAt(Arg(a, 0), "imageGet");
        const image::Color color =
            buffer.GetPixel(Integer(Arg(a, 1), "imageGet"), Integer(Arg(a, 2), "imageGet"));
        Value array = Value::MakeArray();
        array.array->items.push_back(Value(color.r));
        array.array->items.push_back(Value(color.g));
        array.array->items.push_back(Value(color.b));
        array.array->items.push_back(Value(color.a));
        return array;
    });
    define("imageRect", [imageAt](Interpreter&, const std::vector<Value>& a) {
        imageAt(Arg(a, 0), "imageRect")
            .FillRect(Integer(Arg(a, 1), "imageRect"), Integer(Arg(a, 2), "imageRect"),
                      Integer(Arg(a, 3), "imageRect"), Integer(Arg(a, 4), "imageRect"),
                      ToImageColor(a, 5, 1.0f));
        return Value(true);
    });
    define("imageCircle", [imageAt](Interpreter&, const std::vector<Value>& a) {
        imageAt(Arg(a, 0), "imageCircle")
            .FillCircle(static_cast<int>(Number(Arg(a, 1), "imageCircle")),
                        static_cast<int>(Number(Arg(a, 2), "imageCircle")),
                        static_cast<float>(Number(Arg(a, 3), "imageCircle")),
                        ToImageColor(a, 4, 1.0f));
        return Value(true);
    });
    define("imageLine", [imageAt](Interpreter&, const std::vector<Value>& a) {
        imageAt(Arg(a, 0), "imageLine")
            .DrawLine(Integer(Arg(a, 1), "imageLine"), Integer(Arg(a, 2), "imageLine"),
                      Integer(Arg(a, 3), "imageLine"), Integer(Arg(a, 4), "imageLine"),
                      ToImageColor(a, 5, 1.0f));
        return Value(true);
    });
    define("imageGradient", [imageAt](Interpreter&, const std::vector<Value>& a) {
        imageAt(Arg(a, 0), "imageGradient")
            .GradientVertical(ToImageColor(a, 1, 1.0f), ToImageColor(a, 4, 1.0f));
        return Value(true);
    });
    define("imageChecker", [imageAt](Interpreter&, const std::vector<Value>& a) {
        imageAt(Arg(a, 0), "imageChecker")
            .Checkerboard(Integer(Arg(a, 1), "imageChecker"), ToImageColor(a, 2, 1.0f),
                          ToImageColor(a, 6, 1.0f));
        return Value(true);
    });
    define("imageNoise", [imageAt](Interpreter&, const std::vector<Value>& a) {
        imageAt(Arg(a, 0), "imageNoise")
            .AddNoise(static_cast<float>(Number(Arg(a, 1), "imageNoise")),
                      static_cast<unsigned int>(a.size() > 2 ? Integer(Arg(a, 2), "imageNoise") : 1337));
        return Value(true);
    });
    define("imageBlur", [imageAt](Interpreter&, const std::vector<Value>& a) {
        imageAt(Arg(a, 0), "imageBlur").BoxBlur(Integer(Arg(a, 1), "imageBlur"));
        return Value(true);
    });
    define("imageGamma", [imageAt](Interpreter&, const std::vector<Value>& a) {
        imageAt(Arg(a, 0), "imageGamma").GammaCorrect(static_cast<float>(Number(Arg(a, 1), "imageGamma")));
        return Value(true);
    });
    define("imageSave", [imageAt, this](Interpreter&, const std::vector<Value>& a) {
        image::Buffer& buffer = imageAt(Arg(a, 0), "imageSave");
        const std::string path = ResolvePath(Text(Arg(a, 1), "imageSave"));
        if (const std::size_t slash = path.find_last_of("/\\"); slash != std::string::npos) {
            fs::CreateDirectories(path.substr(0, slash));
        }
        if (!image::WritePng(buffer, path)) Error("imageSave could not write " + path);
        output_.push_back("saved image " + path);
        if (sink_) sink_("saved image " + path);
        return Value(true);
    });
    define("imageLoad", [pushImage, this](Interpreter&, const std::vector<Value>& a) {
        image::Buffer buffer;
        const std::string path = ResolvePath(Text(Arg(a, 0), "imageLoad"));
        if (!image::ReadPng(path, buffer)) Error("imageLoad could not read " + path);
        images_.push_back(std::move(buffer));
        imageNames_.push_back(fs::FileName(path));
        return Value(static_cast<double>(images_.size()));
    });
}

ScriptRun Host::Run(std::string_view source, std::string name) {
    ScriptRun run;
    run.name = std::move(name);
    output_.clear();

    Diagnostic diagnostic;
    Program program = Program::Parse(source, &diagnostic);
    if (!diagnostic.message.empty()) {
        run.result.ok = false;
        run.result.error = diagnostic.message;
        run.result.line = diagnostic.line;
        run.result.column = diagnostic.column;
        run.output = output_;
        return run;
    }

    Interpreter interpreter;
    interpreter.SetStepLimit(stepLimit_);
    Install(interpreter);
    run.result = interpreter.Run(program);
    run.output = output_;
    return run;
}

}  // namespace astra::script
