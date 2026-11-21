// render/Pathtracer.h - offline ray tracing / path tracing of the level on the CPU.
//
// The interactive viewport is a rasteriser, so lighting is approximated. This module does the
// opposite: it builds an acceleration structure from the same MeshAsset data the viewport
// draws, then traces primary rays with shadow rays, ambient occlusion, reflections and
// (optionally) full path-traced multi-bounce transport. It runs on every core and reports
// its own progress, which is what the Render panel shows.
//
// The GPU backend plugs into the same `Settings`/`Image` contract once the D3D12 device lands
// (see docs/ROADMAP.md); `Backend::Gpu` is reported as unavailable rather than faked.
#pragma once

#include <atomic>
#include <cstdint>
#include <functional>
#include <string>
#include <vector>

#include "astra/image/Image.h"
#include "astra/render/Camera.h"
#include "astra/scene/Scene.h"

namespace astra::render {

enum class TraceBackend { Cpu = 0, Gpu };

struct TraceSettings {
    int width = 480;
    int height = 270;
    int samples = 24;          // paths per pixel
    int maxBounces = 3;        // 0 = direct light only
    float exposure = 1.05f;
    float sunAngularRadiusDeg = 1.2f;  // soft sun disk for soft shadows
    bool ambientOcclusion = true;
    bool reflections = true;
    bool denoise = true;
    TraceBackend backend = TraceBackend::Cpu;
};

struct TraceStats {
    int triangles = 0;
    int nodes = 0;
    int threads = 0;
    double milliseconds = 0.0;
    std::uint64_t rays = 0;
};

// Bakes the scene into a top-level acceleration structure. Cheap enough to rebuild per render
// (levels here are thousands of triangles), and it keeps the tracer independent of the scene.
class TraceScene {
public:
    void Build(const scene::Scene& scene);
    bool Empty() const { return triangles_.empty(); }
    int TriangleCount() const { return static_cast<int>(triangles_.size()); }

    struct Hit {
        float t = 0.0f;
        scene::Vec3 position;
        scene::Vec3 normal;
        scene::Vec3 albedo;
        scene::Vec3 emission;
        float roughness = 0.5f;
        float metallic = 0.0f;
        int primitive = -1;
    };
    bool Intersect(const scene::Vec3& origin, const scene::Vec3& direction, float maxDistance,
                   Hit& hit, std::uint64_t* rayCounter) const;
    bool Occluded(const scene::Vec3& origin, const scene::Vec3& direction, float maxDistance) const;

    struct Triangle {
        scene::Vec3 a, b, c;
        scene::Vec3 na, nb, nc;
        scene::Vec3 albedo;
        scene::Vec3 emission;
        float roughness = 0.5f;
        float metallic = 0.0f;
    };
    const std::vector<Triangle>& Triangles() const { return triangles_; }
    const scene::Vec3& SkyTop() const { return skyTop_; }
    const scene::Vec3& SkyHorizon() const { return skyHorizon_; }
    const scene::Vec3& SunDirection() const { return sunDirection_; }
    const scene::Vec3& SunColor() const { return sunColor_; }
    float SunIntensity() const { return sunIntensity_; }
    int NodeCount() const { return static_cast<int>(nodes_.size()); }

private:
    struct Node {
        scene::Vec3 min;
        scene::Vec3 max;
        int left = -1;    // -1 for leaves
        int right = -1;
        int start = 0;
        int count = 0;
    };
    int BuildNode(int start, int count, int depth);

    std::vector<Triangle> triangles_;
    std::vector<int> order_;
    std::vector<Node> nodes_;
    scene::Vec3 skyTop_{0.05f, 0.07f, 0.11f};
    scene::Vec3 skyHorizon_{0.24f, 0.27f, 0.35f};
    scene::Vec3 sunDirection_{0.0f, -1.0f, 0.0f};
    scene::Vec3 sunColor_{1.0f, 0.95f, 0.88f};
    float sunIntensity_ = 2.4f;
};

// Traces a still. `progress` is called from the worker threads with 0..1 and may be null.
image::Buffer Render(const TraceScene& scene, const Camera& camera, const TraceSettings& settings,
                     TraceStats* stats = nullptr,
                     const std::function<void(float)>& progress = nullptr);

// Convenience: build + trace + save as PNG.
bool RenderToFile(const scene::Scene& scene, const Camera& camera, const TraceSettings& settings,
                  const std::string& path, TraceStats* stats = nullptr);

const char* TraceBackendName(TraceBackend backend);
bool TraceBackendAvailable(TraceBackend backend);

}  // namespace astra::render
