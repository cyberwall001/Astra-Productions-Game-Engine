#include "astra/render/ViewportRenderer.h"

#include <algorithm>
#include <cstdint>
#include <chrono>
#include <cmath>
#include <vector>

#include "imgui_internal.h"

namespace astra::render {
namespace {

using scene::Component;
using scene::Entity;
using scene::EntityId;
using scene::MeshShape;
using scene::Vec3;

// ---------------------------------------------------------------------------
// Palette. Kept local so the viewport can shift with the theme without the renderer
// depending on the widget layer.
// ---------------------------------------------------------------------------
constexpr ImU32 kSkyTop = IM_COL32(16, 20, 30, 255);
constexpr ImU32 kSkyHorizon = IM_COL32(52, 58, 74, 255);
constexpr ImU32 kGroundFog = IM_COL32(38, 42, 54, 255);
constexpr ImU32 kAxisX = IM_COL32(242, 88, 91, 255);
constexpr ImU32 kAxisY = IM_COL32(95, 207, 158, 255);
constexpr ImU32 kAxisZ = IM_COL32(111, 168, 220, 255);

ImU32 ToColor(const Vec3& c, float alpha = 1.0f) {
    const auto channel = [](float v) {
        const float clamped = v < 0.0f ? 0.0f : (v > 1.0f ? 1.0f : v);
        return static_cast<int>(clamped * 255.0f + 0.5f);
    };
    return IM_COL32(channel(c.x), channel(c.y), channel(c.z), channel(alpha));
}

ImU32 Mix(ImU32 a, ImU32 b, float t) {
    const float inv = 1.0f - t;
    const auto part = [&](int shift) {
        const float ca = static_cast<float>((a >> shift) & 0xFF);
        const float cb = static_cast<float>((b >> shift) & 0xFF);
        return static_cast<int>(ca * inv + cb * t + 0.5f);
    };
    return IM_COL32(part(IM_COL32_R_SHIFT), part(IM_COL32_G_SHIFT), part(IM_COL32_B_SHIFT),
                    part(IM_COL32_A_SHIFT));
}

ImU32 WithAlpha(ImU32 color, float alpha) {
    const int a = static_cast<int>(alpha * 255.0f + 0.5f);
    return (color & ~IM_COL32_A_MASK) | (static_cast<ImU32>(a) << IM_COL32_A_SHIFT);
}

// ---------------------------------------------------------------------------
// Mesh building: every primitive is a unit shape the actor's size scales.
// ---------------------------------------------------------------------------
struct MeshVertex {
    Vec3 position;
    Vec3 normal;
};

struct MeshData {
    std::vector<MeshVertex> vertices;
    std::vector<unsigned int> indices;
};

void AddQuad(MeshData& mesh, const MeshVertex& a, const MeshVertex& b, const MeshVertex& c,
             const MeshVertex& d) {
    const auto base = static_cast<unsigned int>(mesh.vertices.size());
    mesh.vertices.push_back(a);
    mesh.vertices.push_back(b);
    mesh.vertices.push_back(c);
    mesh.vertices.push_back(d);
    mesh.indices.insert(mesh.indices.end(), {base, base + 1, base + 2, base, base + 2, base + 3});
}

MeshData BuildCube() {
    MeshData mesh;
    const Vec3 n[6] = {{1, 0, 0}, {-1, 0, 0}, {0, 1, 0}, {0, -1, 0}, {0, 0, 1}, {0, 0, -1}};
    for (int face = 0; face < 6; ++face) {
        const Vec3 normal = n[face];
        Vec3 u = std::fabs(normal.y) > 0.5f ? Vec3(1, 0, 0) : Vec3(0, 1, 0);
        const Vec3 v = Cross(normal, u);
        const Vec3 center = normal * 0.5f;
        const MeshVertex a{center - u * 0.5f - v * 0.5f, normal};
        const MeshVertex b{center + u * 0.5f - v * 0.5f, normal};
        const MeshVertex c{center + u * 0.5f + v * 0.5f, normal};
        const MeshVertex d{center - u * 0.5f + v * 0.5f, normal};
        AddQuad(mesh, a, b, c, d);
    }
    return mesh;
}

MeshData BuildSphere(int segments = 24, int rings = 14) {
    MeshData mesh;
    for (int ring = 0; ring <= rings; ++ring) {
        const float v = static_cast<float>(ring) / static_cast<float>(rings);
        const float phi = v * 3.14159265f;
        for (int segment = 0; segment <= segments; ++segment) {
            const float u = static_cast<float>(segment) / static_cast<float>(segments);
            const float theta = u * 6.28318531f;
            const Vec3 normal(std::sin(phi) * std::cos(theta), std::cos(phi),
                              std::sin(phi) * std::sin(theta));
            mesh.vertices.push_back({normal * 0.5f, normal});
        }
    }
    const int stride = segments + 1;
    for (int ring = 0; ring < rings; ++ring) {
        for (int segment = 0; segment < segments; ++segment) {
            const auto a = static_cast<unsigned int>(ring * stride + segment);
            const auto b = a + 1;
            const auto c = a + static_cast<unsigned int>(stride) + 1;
            const auto d = a + static_cast<unsigned int>(stride);
            mesh.indices.insert(mesh.indices.end(), {a, d, c, a, c, b});
        }
    }
    return mesh;
}

MeshData BuildCylinder(int segments = 24) {
    MeshData mesh;
    for (int segment = 0; segment <= segments; ++segment) {
        const float u = static_cast<float>(segment) / static_cast<float>(segments);
        const float theta = u * 6.28318531f;
        const Vec3 normal(std::cos(theta), 0.0f, std::sin(theta));
        mesh.vertices.push_back({Vec3(normal.x * 0.5f, -0.5f, normal.z * 0.5f), normal});
        mesh.vertices.push_back({Vec3(normal.x * 0.5f, 0.5f, normal.z * 0.5f), normal});
    }
    for (int segment = 0; segment < segments; ++segment) {
        const auto base = static_cast<unsigned int>(segment * 2);
        mesh.indices.insert(mesh.indices.end(),
                            {base, base + 1, base + 3, base, base + 3, base + 2});
    }
    // Caps, as triangle fans around the centre vertex.
    for (int cap = 0; cap < 2; ++cap) {
        const float y = cap == 0 ? -0.5f : 0.5f;
        const Vec3 normal(0.0f, cap == 0 ? -1.0f : 1.0f, 0.0f);
        const auto center = static_cast<unsigned int>(mesh.vertices.size());
        mesh.vertices.push_back({Vec3(0.0f, y, 0.0f), normal});
        for (int segment = 0; segment <= segments; ++segment) {
            const float theta = static_cast<float>(segment) / static_cast<float>(segments) *
                                6.28318531f;
            mesh.vertices.push_back(
                {Vec3(std::cos(theta) * 0.5f, y, std::sin(theta) * 0.5f), normal});
        }
        for (int segment = 0; segment < segments; ++segment) {
            const auto a = center + 1 + static_cast<unsigned int>(segment);
            const auto b = a + 1;
            if (cap == 0) {
                mesh.indices.insert(mesh.indices.end(), {center, b, a});
            } else {
                mesh.indices.insert(mesh.indices.end(), {center, a, b});
            }
        }
    }
    return mesh;
}

MeshData BuildCone(int segments = 24) {
    MeshData mesh;
    const Vec3 apex(0.0f, 0.5f, 0.0f);
    for (int segment = 0; segment < segments; ++segment) {
        const float theta0 =
            static_cast<float>(segment) / static_cast<float>(segments) * 6.28318531f;
        const float theta1 =
            static_cast<float>(segment + 1) / static_cast<float>(segments) * 6.28318531f;
        const Vec3 p0(std::cos(theta0) * 0.5f, -0.5f, std::sin(theta0) * 0.5f);
        const Vec3 p1(std::cos(theta1) * 0.5f, -0.5f, std::sin(theta1) * 0.5f);
        const Vec3 sideNormal = Normalize(Cross(p1 - p0, apex - p0));
        const Vec3 mid = (p0 + p1) * 0.5f;
        const Vec3 outward = Normalize(Vec3(mid.x, 0.0f, mid.z));
        const Vec3 normal = Dot(sideNormal, outward) < 0.0f ? -sideNormal : sideNormal;
        const auto base = static_cast<unsigned int>(mesh.vertices.size());
        mesh.vertices.push_back({apex, Normalize(normal + Vec3(0.0f, 0.4f, 0.0f))});
        mesh.vertices.push_back({p0, normal});
        mesh.vertices.push_back({p1, normal});
        mesh.indices.insert(mesh.indices.end(), {base, base + 2, base + 1});
    }
    const Vec3 down(0.0f, -1.0f, 0.0f);
    const auto center = static_cast<unsigned int>(mesh.vertices.size());
    mesh.vertices.push_back({Vec3(0.0f, -0.5f, 0.0f), down});
    for (int segment = 0; segment <= segments; ++segment) {
        const float theta = static_cast<float>(segment) / static_cast<float>(segments) *
                            6.28318531f;
        mesh.vertices.push_back({Vec3(std::cos(theta) * 0.5f, -0.5f, std::sin(theta) * 0.5f), down});
    }
    for (int segment = 0; segment < segments; ++segment) {
        const auto a = center + 1 + static_cast<unsigned int>(segment);
        mesh.indices.insert(mesh.indices.end(), {center, a, a + 1});
    }
    return mesh;
}

MeshData BuildPlane(int divisions = 8) {
    MeshData mesh;
    const Vec3 up(0.0f, 1.0f, 0.0f);
    for (int z = 0; z <= divisions; ++z) {
        for (int x = 0; x <= divisions; ++x) {
            const float fx = static_cast<float>(x) / static_cast<float>(divisions) - 0.5f;
            const float fz = static_cast<float>(z) / static_cast<float>(divisions) - 0.5f;
            mesh.vertices.push_back({Vec3(fx, 0.0f, fz), up});
        }
    }
    const int stride = divisions + 1;
    for (int z = 0; z < divisions; ++z) {
        for (int x = 0; x < divisions; ++x) {
            const auto a = static_cast<unsigned int>(z * stride + x);
            const auto b = a + 1;
            const auto c = a + static_cast<unsigned int>(stride) + 1;
            const auto d = a + static_cast<unsigned int>(stride);
            mesh.indices.insert(mesh.indices.end(), {a, c, b, a, d, c});
        }
    }
    return mesh;
}

const MeshData& MeshFor(MeshShape shape) {
    static const MeshData cube = BuildCube();
    static const MeshData sphere = BuildSphere();
    static const MeshData cylinder = BuildCylinder();
    static const MeshData cone = BuildCone();
    static const MeshData plane = BuildPlane();
    switch (shape) {
        case MeshShape::Sphere:
            return sphere;
        case MeshShape::Cylinder:
            return cylinder;
        case MeshShape::Cone:
            return cone;
        case MeshShape::Plane:
            return plane;
        case MeshShape::Cube:
        default:
            return cube;
    }
}

// ---------------------------------------------------------------------------
// Camera space + projection.
// ---------------------------------------------------------------------------
struct Projector {
    Vec3 eye;
    Vec3 forward;
    Vec3 right;
    Vec3 up;
    ImVec2 center;
    float focal = 1.0f;
    float nearZ = 0.08f;

    Vec3 ToView(const Vec3& world) const {
        const Vec3 rel = world - eye;
        return Vec3(Dot(rel, right), Dot(rel, up), Dot(rel, forward));
    }

    ImVec2 ProjectView(const Vec3& view) const {
        const float invZ = 1.0f / std::max(view.z, 0.0001f);
        return ImVec2(center.x + view.x * focal * invZ, center.y - view.y * focal * invZ);
    }
};

Projector MakeProjector(const Rect& rect, const Camera& camera) {
    Projector projector;
    projector.eye = camera.Position();
    camera.Basis(projector.forward, projector.right, projector.up);
    projector.center = ImVec2(rect.min.x + rect.Width() * 0.5f,
                              rect.min.y + rect.Height() * 0.5f);
    const float fov = std::max(15.0f, std::min(110.0f, camera.fovDeg));
    projector.focal = (rect.Height() * 0.5f) / std::tan(Radians(fov) * 0.5f);
    projector.nearZ = 0.08f;
    return projector;
}

struct VertexOut {
    ImVec2 screen;
    float depth = 0.0f;
    ImU32 color = 0;
};

struct PendingTri {
    ImVec2 points[3];
    ImU32 colors[3];
    float depth = 0.0f;
};

void PushTri(std::vector<PendingTri>& out, const ImVec2& a, const ImVec2& b, const ImVec2& c,
             ImU32 ca, ImU32 cb, ImU32 cc, float depth) {
    PendingTri tri;
    tri.points[0] = a;
    tri.points[1] = b;
    tri.points[2] = c;
    tri.colors[0] = ca;
    tri.colors[1] = cb;
    tri.colors[2] = cc;
    tri.depth = depth;
    out.push_back(tri);
}

// Sutherland-Hodgman clip of a camera-space polygon against z >= near.
void ClipNear(const std::vector<VertexOut>& input, const Projector& projector,
              std::vector<VertexOut>& output) {
    output.clear();
    if (input.empty()) return;
    const std::size_t count = input.size();
    for (std::size_t i = 0; i < count; ++i) {
        const VertexOut& current = input[i];
        const VertexOut& next = input[(i + 1) % count];
        const bool currentInside = current.depth >= projector.nearZ;
        const bool nextInside = next.depth >= projector.nearZ;
        if (currentInside) output.push_back(current);
        if (currentInside != nextInside) {
            const float t = (projector.nearZ - current.depth) / (next.depth - current.depth);
            VertexOut clipped;
            clipped.screen = ImVec2(current.screen.x + (next.screen.x - current.screen.x) * t,
                                    current.screen.y + (next.screen.y - current.screen.y) * t);
            clipped.depth = projector.nearZ;
            clipped.color = current.color;
            output.push_back(clipped);
        }
    }
}

// ---------------------------------------------------------------------------
// Lighting environment pulled out of the scene.
// ---------------------------------------------------------------------------
struct Environment {
    Vec3 sunDirection{-0.42f, -0.78f, 0.46f};
    Vec3 sunColor{1.0f, 0.94f, 0.86f};
    float sunIntensity = 2.4f;
    Vec3 ambientSky{0.30f, 0.34f, 0.44f};
    Vec3 ambientGround{0.10f, 0.11f, 0.14f};
    float fog = 0.18f;
    float exposure = 1.0f;
    Vec3 fogColor{0.13f, 0.15f, 0.20f};
};

Environment ReadEnvironment(const scene::Scene& scene) {
    Environment env;
    bool sunFound = false;
    for (const Entity& entity : scene.Entities()) {
        if (entity.Has(Component::Atmosphere)) {
            env.sunDirection = Normalize(Vec3(entity.atmosphere.sunDirection.x,
                                              -std::fabs(entity.atmosphere.sunDirection.y),
                                              entity.atmosphere.sunDirection.z));
            env.fog = entity.atmosphere.fogDensity;
            env.exposure = std::max(0.2f, entity.atmosphere.exposure);
            env.ambientSky = env.ambientSky * (0.6f + entity.atmosphere.cloudCover * 0.8f);
            env.fogColor = Lerp(Vec3(0.09f, 0.10f, 0.14f), Vec3(0.20f, 0.22f, 0.29f),
                                1.0f - std::min(1.0f, entity.atmosphere.fogDensity * 2.2f));
        }
        if (entity.Has(Component::Light) && entity.light.type == scene::LightType::Directional &&
            !sunFound) {
            env.sunColor = entity.light.color;
            env.sunIntensity = entity.light.intensity;
            sunFound = true;
            if (!entity.Has(Component::Atmosphere)) {
                const Vec3 rot = entity.transform.rotation;
                const Vec3 dir = RotateEuler(Vec3(0.0f, -1.0f, 0.0f), rot);
                env.sunDirection = Normalize(dir);
            }
        }
    }
    return env;
}

Vec3 ShadeColor(const Vec3& albedo, const Vec3& normal, const Vec3& viewDirection,
                const Environment& env, float distance) {
    const float lambert = std::max(0.0f, Dot(normal, -env.sunDirection));
    const float hemi = 0.5f + 0.5f * normal.y;
    const Vec3 ambient = Lerp(env.ambientGround, env.ambientSky, hemi) * 0.55f;
    const float skyFill = 0.14f * (0.5f + 0.5f * normal.y);
    Vec3 lit = albedo * (ambient + env.sunColor * (lambert * env.sunIntensity * 0.62f + skyFill));

    // A touch of rim light keeps silhouettes readable against the sky.
    const float rim = std::pow(1.0f - std::fabs(Dot(normal, viewDirection)), 3.0f);
    lit = lit + env.ambientSky * (rim * 0.10f);

    const float fogAmount = std::min(1.0f, (distance * env.fog) / 90.0f);
    lit = Lerp(lit, env.fogColor, fogAmount * 0.85f);
    return lit * env.exposure;
}

// ---------------------------------------------------------------------------
// Small viewport gizmos drawn in screen space.
// ---------------------------------------------------------------------------
void DrawActorIcon(ImDrawList* dl, ImVec2 center, float radius, ImU32 color, int kind) {
    dl->AddCircleFilled(center, radius, WithAlpha(color, 0.10f), 20);
    dl->AddCircle(center, radius, WithAlpha(color, 0.6f), 20, 1.1f);
    switch (kind) {
        case 0:  // light: sun
            dl->AddCircleFilled(center, radius * 0.34f, color, 12);
            for (int i = 0; i < 8; ++i) {
                const float angle = static_cast<float>(i) * 0.7853981f;
                const ImVec2 dir(std::cos(angle), std::sin(angle));
                dl->AddLine(ImVec2(center.x + dir.x * radius * 0.5f,
                                   center.y + dir.y * radius * 0.5f),
                            ImVec2(center.x + dir.x * radius * 0.82f,
                                   center.y + dir.y * radius * 0.82f),
                            color, 1.3f);
            }
            break;
        case 1:  // camera: lens barrel
            dl->AddRect(ImVec2(center.x - radius * 0.5f, center.y - radius * 0.3f),
                        ImVec2(center.x + radius * 0.16f, center.y + radius * 0.3f), color, 2.0f,
                        0, 1.3f);
            dl->AddTriangleFilled(ImVec2(center.x + radius * 0.18f, center.y - radius * 0.28f),
                                  ImVec2(center.x + radius * 0.18f, center.y + radius * 0.28f),
                                  ImVec2(center.x + radius * 0.6f, center.y), color);
            break;
        case 2:  // weather volume: cloud
            dl->AddCircleFilled(ImVec2(center.x - radius * 0.3f, center.y + radius * 0.1f),
                                radius * 0.32f, WithAlpha(color, 0.85f), 12);
            dl->AddCircleFilled(ImVec2(center.x + radius * 0.02f, center.y - radius * 0.12f),
                                radius * 0.42f, WithAlpha(color, 0.85f), 14);
            dl->AddCircleFilled(ImVec2(center.x + radius * 0.34f, center.y + radius * 0.1f),
                                radius * 0.30f, WithAlpha(color, 0.85f), 12);
            break;
        default:  // player start: flag
            dl->AddLine(ImVec2(center.x - radius * 0.25f, center.y + radius * 0.55f),
                        ImVec2(center.x - radius * 0.25f, center.y - radius * 0.55f), color, 1.6f);
            dl->AddTriangleFilled(ImVec2(center.x - radius * 0.25f, center.y - radius * 0.55f),
                                  ImVec2(center.x + radius * 0.45f, center.y - radius * 0.3f),
                                  ImVec2(center.x - radius * 0.25f, center.y - radius * 0.05f),
                                  color);
            break;
    }
}

void DrawAxisGizmo(ImDrawList* dl, const Projector& projector, const Vec3& origin, float scale) {
    const Vec3 axes[3] = {Vec3(scale, 0, 0), Vec3(0, scale, 0), Vec3(0, 0, scale)};
    const ImU32 colors[3] = {kAxisX, kAxisY, kAxisZ};
    const Vec3 viewOrigin = projector.ToView(origin);
    if (viewOrigin.z < projector.nearZ) return;
    const ImVec2 screenOrigin = projector.ProjectView(viewOrigin);

    for (int axis = 0; axis < 3; ++axis) {
        const Vec3 tipWorld = origin + axes[axis];
        const Vec3 viewTip = projector.ToView(tipWorld);
        if (viewTip.z < projector.nearZ) continue;
        const ImVec2 tip = projector.ProjectView(viewTip);
        dl->AddLine(screenOrigin, tip, colors[axis], 2.0f);

        const ImVec2 dir(tip.x - screenOrigin.x, tip.y - screenOrigin.y);
        const float length = std::sqrt(dir.x * dir.x + dir.y * dir.y);
        if (length < 4.0f) continue;
        const ImVec2 unit(dir.x / length, dir.y / length);
        const ImVec2 side(-unit.y, unit.x);
        dl->AddTriangleFilled(
            tip, ImVec2(tip.x - unit.x * 8.0f + side.x * 3.5f, tip.y - unit.y * 8.0f + side.y * 3.5f),
            ImVec2(tip.x - unit.x * 8.0f - side.x * 3.5f, tip.y - unit.y * 8.0f - side.y * 3.5f),
            colors[axis]);
    }
}

void DrawBoxWire(ImDrawList* dl, const Projector& projector, const Vec3& center,
                 const Vec3& extents, ImU32 color, float thickness) {
    Vec3 corners[8];
    int index = 0;
    for (int x = -1; x <= 1; x += 2) {
        for (int y = -1; y <= 1; y += 2) {
            for (int z = -1; z <= 1; z += 2) {
                corners[index++] = center + Vec3(extents.x * x, extents.y * y, extents.z * z);
            }
        }
    }
    Vec3 view[8];
    ImVec2 screen[8];
    bool behind[8];
    for (int i = 0; i < 8; ++i) {
        view[i] = projector.ToView(corners[i]);
        behind[i] = view[i].z < projector.nearZ;
        screen[i] = behind[i] ? ImVec2(0, 0) : projector.ProjectView(view[i]);
    }
    static const int edges[12][2] = {{0, 1}, {2, 3}, {4, 5}, {6, 7}, {0, 2}, {1, 3},
                                     {4, 6}, {5, 7}, {0, 4}, {1, 5}, {2, 6}, {3, 7}};
    for (const auto& edge : edges) {
        if (behind[edge[0]] || behind[edge[1]]) continue;
        dl->AddLine(screen[edge[0]], screen[edge[1]], color, thickness);
    }
}

}  // namespace

bool ActorBounds(const Entity& entity, Vec3& center, Vec3& extents) {
    center = entity.transform.position;
    extents = Vec3(0.5f, 0.5f, 0.5f);
    if (entity.Has(Component::Mesh)) {
        extents = entity.transform.scale * (entity.mesh.size * 0.5f);
        if (entity.mesh.shape == MeshShape::Plane) {
            extents.y = std::max(0.02f, entity.transform.scale.y * 0.01f);
        }
        return true;
    }
    if (entity.Has(Component::Light) || entity.Has(Component::Camera) ||
        entity.Has(Component::Weather)) {
        return false;
    }
    return false;
}

Stats Draw(ImDrawList* dl, const Rect& rect, const scene::Scene& scene, const Camera& camera,
           const Options& options, EntityId selected, float time) {
    const auto start = std::chrono::steady_clock::now();
    Stats stats;

    const float width = rect.Width();
    const float height = rect.Height();
    if (width < 8.0f || height < 8.0f) return stats;

    const Projector projector = MakeProjector(rect, camera);
    const Environment env = ReadEnvironment(scene);
    const Vec3 eye = projector.eye;

    // ---- sky + infinite ground ---------------------------------------------
    // The horizon is where a ray parallel to the ground would project, so the ground can
    // fade into the sky instead of ending at a hard band.
    float horizonY = rect.min.y + height * 0.5f;
    {
        const Vec3 forward = projector.forward;
        Vec3 flat(forward.x, 0.0f, forward.z);
        if (Length(flat) < 0.001f) flat = Vec3(0.0f, 0.0f, 1.0f);
        flat = Normalize(flat);
        const Vec3 farPoint = projector.eye + flat * 4000.0f;
        const Vec3 view = projector.ToView(farPoint);
        if (view.z > projector.nearZ) horizonY = projector.ProjectView(view).y;
    }
    horizonY = std::max(rect.min.y - height, std::min(rect.max.y + height, horizonY));

    const int bands = 32;
    for (int i = 0; i < bands; ++i) {
        const float t0 = static_cast<float>(i) / static_cast<float>(bands);
        const float t1 = static_cast<float>(i + 1) / static_cast<float>(bands);
        const float y0 = rect.min.y + height * t0;
        const float y1 = rect.min.y + height * t1 + 1.0f;

        if (y1 <= horizonY) {
            const float t = Clamp01((y1 - rect.min.y) / std::max(1.0f, horizonY - rect.min.y));
            dl->AddRectFilled(ImVec2(rect.min.x, y0), ImVec2(rect.max.x, y1),
                              Mix(kSkyTop, kSkyHorizon, std::pow(t, 1.6f)));
        } else if (y0 >= horizonY) {
            const float t = Clamp01((y0 - horizonY) / std::max(1.0f, rect.max.y - horizonY));
            dl->AddRectFilled(ImVec2(rect.min.x, y0), ImVec2(rect.max.x, y1),
                              Mix(kSkyHorizon, kGroundFog, std::pow(t, 0.75f)));
        } else {
            dl->AddRectFilled(ImVec2(rect.min.x, y0), ImVec2(rect.max.x, horizonY), kSkyHorizon);
            dl->AddRectFilled(ImVec2(rect.min.x, horizonY), ImVec2(rect.max.x, y1), kGroundFog);
        }
    }
    // A faint glow just above the horizon keeps the ground from looking pasted on.
    dl->AddRectFilledMultiColor(ImVec2(rect.min.x, horizonY - height * 0.16f),
                                ImVec2(rect.max.x, horizonY), IM_COL32(120, 132, 168, 0),
                                IM_COL32(120, 132, 168, 0), IM_COL32(120, 132, 168, 26),
                                IM_COL32(120, 132, 168, 26));

    // ---- ground grid -------------------------------------------------------
    if (options.showGrid) {
        const int radius = 20;
        const float y = 0.0f;
        for (int i = -radius; i <= radius; ++i) {
            const bool major = (i % 5) == 0;
            const float fade = 1.0f - static_cast<float>(std::fabs(i)) / static_cast<float>(radius + 2);
            const bool axisLine = i == 0;
            const ImU32 base = axisLine ? IM_COL32(150, 160, 180, 255)
                                        : (major ? IM_COL32(126, 136, 158, 255)
                                                 : IM_COL32(104, 112, 130, 255));
            const ImU32 color = WithAlpha(base, (major ? 0.46f : 0.24f) * fade);
            const Vec3 a(static_cast<float>(i), y, -static_cast<float>(radius));
            const Vec3 b(static_cast<float>(i), y, static_cast<float>(radius));
            const Vec3 c(-static_cast<float>(radius), y, static_cast<float>(i));
            const Vec3 d(static_cast<float>(radius), y, static_cast<float>(i));
            int segmentIndex = 0;
            for (const auto& segment : {std::pair<Vec3, Vec3>{a, b}, std::pair<Vec3, Vec3>{c, d}}) {
                ++segmentIndex;
                const ImU32 lineColor =
                    i == 0 ? WithAlpha(segmentIndex == 1 ? kAxisX : kAxisZ, 0.5f * fade) : color;
                const Vec3 va = projector.ToView(segment.first);
                const Vec3 vb = projector.ToView(segment.second);
                if (va.z < projector.nearZ && vb.z < projector.nearZ) continue;
                // Cheap near clip: pull the endpoint that is behind the camera to the plane.
                auto clip = [&](Vec3& v) {
                    if (v.z >= projector.nearZ) return;
                    const float t = (projector.nearZ - va.z) / (vb.z - va.z);
                    v = va + (vb - va) * std::max(0.0f, std::min(1.0f, t));
                };
                Vec3 clippedA = va;
                Vec3 clippedB = vb;
                if (clippedA.z < projector.nearZ) clip(clippedA);
                else if (clippedB.z < projector.nearZ) {
                    const float t = (projector.nearZ - vb.z) / (va.z - vb.z);
                    clippedB = vb + (va - vb) * std::max(0.0f, std::min(1.0f, t));
                }
                dl->AddLine(projector.ProjectView(clippedA), projector.ProjectView(clippedB),
                            lineColor, 1.0f);
                ++stats.gridLines;
            }
        }
    }

    // ---- soft ground shadows ----------------------------------------------
    if (options.showShadows) {
        for (const Entity& entity : scene.Entities()) {
            if (!entity.active || !entity.Has(Component::Mesh) || !entity.mesh.castShadows) continue;
            if (entity.mesh.shape == MeshShape::Plane) continue;
            Vec3 center, extents;
            ActorBounds(entity, center, extents);
            const float lift = std::max(0.0f, center.y - extents.y);
            const Vec3 ground = center + Vec3(0.0f, -lift, 0.0f);
            const float reach = 0.55f + lift * 0.10f;
            const Vec3 view = projector.ToView(ground);
            if (view.z < projector.nearZ) continue;
            const ImVec2 position = projector.ProjectView(view);
            const float radiusPx = projector.focal * (std::max(extents.x, extents.z) * 1.9f + reach) /
                                   std::max(view.z, 0.1f);
            if (radiusPx < 1.5f || radiusPx > width * 0.9f) continue;
            const float fade = std::max(0.0f, 0.34f - lift * 0.02f);
            dl->AddEllipseFilled(position, ImVec2(radiusPx, radiusPx * 0.42f),
                                 IM_COL32(0, 0, 0, static_cast<int>(fade * 255.0f)), 0.0f, 24);
        }
    }

    // ---- geometry ----------------------------------------------------------
    std::vector<PendingTri> triangles;
    triangles.reserve(4096);

    for (const Entity& entity : scene.Entities()) {
        if (!entity.active) continue;
        if (entity.Has(Component::Mesh)) ++stats.actors;
        else if (entity.Has(Component::Light) || entity.Has(Component::Camera) ||
                 entity.Has(Component::Weather))
            ++stats.actors;
    }

    for (const Entity& entity : scene.Entities()) {
        if (!entity.active || !entity.Has(Component::Mesh) || !entity.mesh.visible) continue;
        ++stats.drawnActors;

        const MeshData& mesh = MeshFor(entity.mesh.shape);
        const Vec3 rotation = entity.transform.rotation;
        const Vec3 scale = entity.transform.scale * entity.mesh.size;
        const Vec3 position = entity.transform.position;

        std::vector<VertexOut> world;
        world.reserve(mesh.vertices.size());
        std::vector<Vec3> normals(mesh.vertices.size());

        for (std::size_t i = 0; i < mesh.vertices.size(); ++i) {
            const MeshVertex& vertex = mesh.vertices[i];
            const Vec3 local = Vec3(vertex.position.x * scale.x, vertex.position.y * scale.y,
                                    vertex.position.z * scale.z);
            const Vec3 worldPos = RotateEuler(local, rotation) + position;
            const Vec3 normal = Normalize(RotateEuler(vertex.normal, rotation));
            normals[i] = normal;

            const Vec3 view = projector.ToView(worldPos);
            VertexOut out;
            out.depth = view.z;
            out.screen = view.z >= projector.nearZ ? projector.ProjectView(view) : ImVec2(0, 0);

            const Vec3 viewDirection = Normalize(worldPos - eye);
            const float distance = Length(worldPos - eye);
            Vec3 color;
            if (options.mode == ViewMode::Unlit) {
                color = entity.mesh.baseColor;
            } else if (options.mode == ViewMode::Normals) {
                color = Vec3(normal.x * 0.5f + 0.5f, normal.y * 0.5f + 0.5f, normal.z * 0.5f + 0.5f);
            } else {
                color = ShadeColor(entity.mesh.baseColor, normal, viewDirection, env, distance);
            }
            out.color = ToColor(color);
            world.push_back(out);
        }

        if (options.mode == ViewMode::Wireframe) {
            // Unique edges only. Drawing a triangle soup would stroke every shared edge
            // twice and turn a cone's fan into a starburst.
            std::vector<std::uint64_t> edges;
            edges.reserve(mesh.indices.size());
            for (std::size_t i = 0; i + 2 < mesh.indices.size(); i += 3) {
                const unsigned int tri[3] = {mesh.indices[i], mesh.indices[i + 1],
                                             mesh.indices[i + 2]};
                if (world[tri[0]].depth < projector.nearZ ||
                    world[tri[1]].depth < projector.nearZ ||
                    world[tri[2]].depth < projector.nearZ) {
                    continue;
                }
                for (unsigned int e = 0; e < 3; ++e) {
                    const std::uint64_t lo = std::min(tri[e], tri[(e + 1) % 3]);
                    const std::uint64_t hi = std::max(tri[e], tri[(e + 1) % 3]);
                    edges.push_back((lo << 32) | hi);
                }
                ++stats.triangles;
            }
            std::sort(edges.begin(), edges.end());
            edges.erase(std::unique(edges.begin(), edges.end()), edges.end());
            const ImU32 wire =
                WithAlpha(ToColor(Lerp(entity.mesh.baseColor, Vec3(1.0f, 1.0f, 1.0f), 0.35f)), 0.70f);
            for (std::uint64_t key : edges) {
                const unsigned int lo = static_cast<unsigned int>(key >> 32);
                const unsigned int hi = static_cast<unsigned int>(key & 0xffffffffull);
                dl->AddLine(world[lo].screen, world[hi].screen, wire, 1.0f);
            }
            continue;
        }

        for (std::size_t i = 0; i + 2 < mesh.indices.size(); i += 3) {
            const unsigned int i0 = mesh.indices[i];
            const unsigned int i1 = mesh.indices[i + 1];
            const unsigned int i2 = mesh.indices[i + 2];
            const VertexOut& a = world[i0];
            const VertexOut& b = world[i1];
            const VertexOut& c = world[i2];
            if (a.depth < projector.nearZ && b.depth < projector.nearZ &&
                c.depth < projector.nearZ) {
                continue;
            }
            std::vector<VertexOut> polygon = {a, b, c};
            if (a.depth < projector.nearZ || b.depth < projector.nearZ ||
                c.depth < projector.nearZ) {
                std::vector<VertexOut> clipped;
                ClipNear(polygon, projector, clipped);
                polygon.swap(clipped);
            }
            if (polygon.size() < 3) continue;

            // Back-face culling on the projected winding.
            const ImVec2& p0 = polygon[0].screen;
            const ImVec2& p1 = polygon[1].screen;
            const ImVec2& p2 = polygon[2].screen;
            const float area = (p1.x - p0.x) * (p2.y - p0.y) - (p2.x - p0.x) * (p1.y - p0.y);
            if (!entity.mesh.twoSided && area >= 0.0f) continue;

            for (std::size_t v = 1; v + 1 < polygon.size(); ++v) {
                PushTri(triangles, polygon[0].screen, polygon[v].screen, polygon[v + 1].screen,
                        polygon[0].color, polygon[v].color, polygon[v + 1].color,
                        (polygon[0].depth + polygon[v].depth + polygon[v + 1].depth) / 3.0f);
            }
        }
    }

    std::sort(triangles.begin(), triangles.end(),
              [](const PendingTri& a, const PendingTri& b) { return a.depth > b.depth; });

    if (!triangles.empty()) {
        const ImVec2 whiteUv = ImGui::GetDrawListSharedData()->TexUvWhitePixel;
        dl->PrimReserve(static_cast<int>(triangles.size() * 3),
                        static_cast<int>(triangles.size() * 3));
        for (const PendingTri& tri : triangles) {
            dl->PrimVtx(tri.points[0], whiteUv, tri.colors[0]);
            dl->PrimVtx(tri.points[1], whiteUv, tri.colors[1]);
            dl->PrimVtx(tri.points[2], whiteUv, tri.colors[2]);
        }
        stats.triangles += static_cast<int>(triangles.size());
    }

    // ---- selection ---------------------------------------------------------
    if (options.showGizmos) {
        if (const Entity* selectedEntity = scene.Find(selected)) {
            Vec3 center, extents;
            ActorBounds(*selectedEntity, center, extents);
            if (selectedEntity->Has(Component::Mesh)) {
                DrawBoxWire(dl, projector, center, extents, IM_COL32(255, 122, 108, 200), 1.6f);
            }
            DrawAxisGizmo(dl, projector, center, 1.6f);
        }
    }

    // ---- actor icons -------------------------------------------------------
    if (options.showActorIcons) {
        for (const Entity& entity : scene.Entities()) {
            if (!entity.active) continue;
            int kind = -1;
            ImU32 color = IM_COL32(160, 166, 180, 255);
            if (entity.Has(Component::Light)) {
                kind = 0;
                color = ToColor(entity.light.color);
            } else if (entity.Has(Component::Camera)) {
                kind = 1;
                color = IM_COL32(150, 170, 210, 255);
            } else if (entity.Has(Component::Weather)) {
                kind = 2;
                color = IM_COL32(130, 176, 220, 255);
            } else if (entity.tag == "gameplay") {
                kind = 3;
                color = IM_COL32(95, 207, 158, 255);
            }
            if (kind < 0) continue;

            const Vec3 view = projector.ToView(entity.transform.position);
            if (view.z < projector.nearZ) continue;
            const ImVec2 position = projector.ProjectView(view);
            if (!rect.Contains(position)) continue;
            const float radius = std::clamp(projector.focal * 0.30f / std::max(view.z, 0.5f), 5.0f,
                                            9.0f);
            DrawActorIcon(dl, position, radius, color, kind);

            if (entity.Has(Component::Weather)) {
                const float pulse = 0.30f + 0.10f * std::sin(time * 1.6f);
                DrawBoxWire(dl, projector, entity.transform.position,
                            entity.transform.scale * 0.5f, WithAlpha(color, pulse), 1.0f);
            }
            if (entity.id == selected) {
                dl->AddCircle(position, radius + 3.0f, IM_COL32(255, 122, 108, 220), 24, 1.4f);
            }
        }
    }

    // ---- vignette ----------------------------------------------------------
    dl->AddRectFilled(rect.min, ImVec2(rect.max.x, rect.min.y + height * 0.06f),
                      IM_COL32(0, 0, 0, 40));
    dl->AddRectFilled(ImVec2(rect.min.x, rect.max.y - height * 0.10f), rect.max,
                      IM_COL32(0, 0, 0, 55));

    const auto end = std::chrono::steady_clock::now();
    stats.milliseconds =
        std::chrono::duration_cast<std::chrono::duration<float, std::milli>>(end - start).count();
    return stats;
}

EntityId Pick(const Rect& rect, const scene::Scene& scene, const Camera& camera, ImVec2 mouse) {
    if (!rect.Contains(mouse)) return scene::kInvalidEntity;
    const Projector projector = MakeProjector(rect, camera);

    EntityId best = scene::kInvalidEntity;
    float bestScore = 1e9f;

    for (const Entity& entity : scene.Entities()) {
        if (!entity.active) continue;
        bool pickable = entity.Has(Component::Mesh) || entity.Has(Component::Light) ||
                        entity.Has(Component::Camera) || entity.Has(Component::Weather);
        if (!pickable) continue;

        Vec3 center, extents;
        ActorBounds(entity, center, extents);
        const Vec3 view = projector.ToView(center);
        if (view.z < projector.nearZ) continue;
        const ImVec2 screen = projector.ProjectView(view);

        // Approximate the actor with its screen-space bounding circle.
        const float radius = entity.Has(Component::Mesh)
                                 ? std::max(6.0f, projector.focal *
                                                      std::max({extents.x, extents.y, extents.z}) /
                                                      std::max(view.z, 0.1f))
                                 : 12.0f;
        const float dx = mouse.x - screen.x;
        const float dy = mouse.y - screen.y;
        const float distance = std::sqrt(dx * dx + dy * dy);
        if (distance > radius) continue;
        if (distance < bestScore) {
            bestScore = distance;
            best = entity.id;
        }
    }
    return best;
}

void FrameSelection(Camera& camera, const scene::Scene& scene, EntityId selected) {
    if (const Entity* entity = scene.Find(selected)) {
        Vec3 center, extents;
        ActorBounds(*entity, center, extents);
        camera.Frame(center, std::max({extents.x, extents.y, extents.z}));
        return;
    }
    if (scene.Count() == 0) {
        camera.target = Vec3(0.0f, 1.0f, 0.0f);
        camera.distance = 16.0f;
        return;
    }
    Vec3 min(1e9f, 1e9f, 1e9f);
    Vec3 max(-1e9f, -1e9f, -1e9f);
    for (const Entity& entity : scene.Entities()) {
        const Vec3 p = entity.transform.position;
        min = Vec3(std::min(min.x, p.x), std::min(min.y, p.y), std::min(min.z, p.z));
        max = Vec3(std::max(max.x, p.x), std::max(max.y, p.y), std::max(max.z, p.z));
    }
    const Vec3 center = (min + max) * 0.5f;
    const float radius = std::max(2.0f, Length(max - min) * 0.5f);
    camera.Frame(center, radius);
}

}  // namespace astra::render
