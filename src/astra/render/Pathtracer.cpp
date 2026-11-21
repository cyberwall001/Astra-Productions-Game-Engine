#include "astra/render/Pathtracer.h"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <thread>

#include "astra/asset/MeshBuilder.h"

namespace astra::render {
namespace {

using scene::Vec3;

constexpr float kEpsilon = 1e-3f;
constexpr float kPi = 3.14159265358979f;

// The tracer keeps its own tiny vector helpers: the renderer's maths header is included for the
// camera, and these overloads would collide with it.
inline Vec3 VAdd(const Vec3& a, const Vec3& b) { return Vec3(a.x + b.x, a.y + b.y, a.z + b.z); }
inline Vec3 VSub(const Vec3& a, const Vec3& b) { return Vec3(a.x - b.x, a.y - b.y, a.z - b.z); }
inline Vec3 VMul(const Vec3& a, float s) { return Vec3(a.x * s, a.y * s, a.z * s); }
inline Vec3 VMulVec(const Vec3& a, const Vec3& b) {
    return Vec3(a.x * b.x, a.y * b.y, a.z * b.z);
}
inline Vec3 VLerp(const Vec3& a, const Vec3& b, float t) {
    return Vec3(a.x + (b.x - a.x) * t, a.y + (b.y - a.y) * t, a.z + (b.z - a.z) * t);
}
inline float VDot(const Vec3& a, const Vec3& b) { return a.x * b.x + a.y * b.y + a.z * b.z; }
inline Vec3 VCross(const Vec3& a, const Vec3& b) {
    return Vec3(a.y * b.z - a.z * b.y, a.z * b.x - a.x * b.z, a.x * b.y - a.y * b.x);
}
inline float VLength(const Vec3& a) { return std::sqrt(VDot(a, a)); }
inline Vec3 VNormalize(const Vec3& a) {
    const float length = VLength(a);
    return length > 1e-7f ? VMul(a, 1.0f / length) : Vec3(0.0f, 1.0f, 0.0f);
}
inline Vec3 VMin3(const Vec3& a, const Vec3& b) {
    return Vec3(std::min(a.x, b.x), std::min(a.y, b.y), std::min(a.z, b.z));
}
inline Vec3 VMax3(const Vec3& a, const Vec3& b) {
    return Vec3(std::max(a.x, b.x), std::max(a.y, b.y), std::max(a.z, b.z));
}

// A tiny RNG so the tracer is deterministic with a fixed seed.
struct Rng {
    std::uint32_t state = 0x9E3779B9u;
    explicit Rng(std::uint32_t seed) : state(seed * 747796405u + 2891336453u) {}
    std::uint32_t Next() {
        state ^= state << 13;
        state ^= state >> 17;
        state ^= state << 5;
        return state;
    }
    float Float() { return static_cast<float>(Next() & 0xFFFFFF) / 16777216.0f; }
    Vec3 UnitSphere() {
        const float z = Float() * 2.0f - 1.0f;
        const float a = Float() * 2.0f * kPi;
        const float r = std::sqrt(std::max(0.0f, 1.0f - z * z));
        return Vec3(r * std::cos(a), r * std::sin(a), z);
    }
};

bool IntersectTriangle(const TraceScene::Triangle& triangle, const Vec3& origin,
                       const Vec3& direction, float maxDistance, float& outT, float& outU,
                       float& outV) {
    const Vec3 edge1 = VSub(triangle.b, triangle.a);
    const Vec3 edge2 = VSub(triangle.c, triangle.a);
    const Vec3 p = VCross(direction, edge2);
    const float determinant = VDot(edge1, p);
    if (std::fabs(determinant) < 1e-8f) return false;
    const float inverse = 1.0f / determinant;
    const Vec3 t = VSub(origin, triangle.a);
    const float u = VDot(t, p) * inverse;
    if (u < 0.0f || u > 1.0f) return false;
    const Vec3 q = VCross(t, edge1);
    const float v = VDot(direction, q) * inverse;
    if (v < 0.0f || u + v > 1.0f) return false;
    const float distance = VDot(edge2, q) * inverse;
    if (distance < kEpsilon || distance > maxDistance) return false;
    outT = distance;
    outU = u;
    outV = v;
    return true;
}

bool Slab(const Vec3& min, const Vec3& max, const Vec3& origin, const Vec3& inverse,
          float maxDistance) {
    const float t0x = (min.x - origin.x) * inverse.x;
    const float t1x = (max.x - origin.x) * inverse.x;
    const float t0y = (min.y - origin.y) * inverse.y;
    const float t1y = (max.y - origin.y) * inverse.y;
    const float t0z = (min.z - origin.z) * inverse.z;
    const float t1z = (max.z - origin.z) * inverse.z;
    const float tmin = std::max({std::min(t0x, t1x), std::min(t0y, t1y), std::min(t0z, t1z)});
    const float tmax = std::min({std::max(t0x, t1x), std::max(t0y, t1y), std::max(t0z, t1z)});
    return tmax >= std::max(0.0f, tmin) && tmin < maxDistance;
}

}  // namespace

void TraceScene::Build(const scene::Scene& scene) {
    triangles_.clear();
    order_.clear();
    nodes_.clear();

    // Environment: reuse the same atmosphere/light actors the viewport reads.
    for (const auto& entity : scene.Entities()) {
        if (!entity.active) continue;
        if (entity.Has(scene::Component::Atmosphere)) {
            skyTop_ = Vec3(0.035f + 0.02f, 0.05f, 0.085f);
            skyHorizon_ = VLerp(Vec3(0.20f, 0.24f, 0.33f), Vec3(0.32f, 0.30f, 0.34f),
                               entity.atmosphere.cloudCover);
            sunDirection_ = VNormalize(Vec3(entity.atmosphere.sunDirection.x,
                                           -std::fabs(entity.atmosphere.sunDirection.y),
                                           entity.atmosphere.sunDirection.z));
        }
        if (entity.Has(scene::Component::Light) &&
            entity.light.type == scene::LightType::Directional) {
            sunColor_ = entity.light.color;
            sunIntensity_ = entity.light.intensity;
        }
    }

    const auto worldMatrix = [](const scene::Entity& entity, const Vec3& local) {
        // Same convention as the viewport: scale, then Euler XYZ rotation, then translate.
        Vec3 p = Vec3(local.x * entity.transform.scale.x * entity.mesh.size.x,
                      local.y * entity.transform.scale.y * entity.mesh.size.y,
                      local.z * entity.transform.scale.z * entity.mesh.size.z);
        const Vec3 r = entity.transform.rotation;
        const auto rotate = [](const Vec3& v, const Vec3& euler) {
            const float rx = euler.x * kPi / 180.0f;
            const float ry = euler.y * kPi / 180.0f;
            const float rz = euler.z * kPi / 180.0f;
            // Rz * Ry * Rx
            Vec3 out = v;
            {
                const float c = std::cos(rx), s = std::sin(rx);
                out = Vec3(out.x, out.y * c - out.z * s, out.y * s + out.z * c);
            }
            {
                const float c = std::cos(ry), s = std::sin(ry);
                out = Vec3(out.x * c + out.z * s, out.y, -out.x * s + out.z * c);
            }
            {
                const float c = std::cos(rz), s = std::sin(rz);
                out = Vec3(out.x * c - out.y * s, out.x * s + out.y * c, out.z);
            }
            return out;
        };
        p = rotate(p, r);
        return VAdd(p, entity.transform.position);
    };

    for (const auto& entity : scene.Entities()) {
        if (!entity.active || !entity.Has(scene::Component::Mesh) || !entity.mesh.visible) continue;
        const scene::MeshAsset* mesh = entity.mesh.shape == scene::MeshShape::Custom
                                           ? scene.Mesh(entity.mesh.meshId)
                                           : &asset::Primitive(entity.mesh.shape);
        if (!mesh || mesh->Empty()) continue;

        const bool hasMaterial = entity.Has(scene::Component::Material);
        Vec3 albedo = hasMaterial ? entity.material.baseColor : entity.mesh.baseColor;
        if (entity.Has(scene::Component::Material)) {
            switch (entity.material.shader) {
                case scene::MaterialShader::Emissive:
                case scene::MaterialShader::Hologram:
                    albedo = entity.material.baseColor;
                    break;
                default:
                    albedo = VLerp(entity.material.baseColor, entity.material.accentColor, 0.25f);
                    break;
            }
        }
        Vec3 emission(0.0f, 0.0f, 0.0f);
        if (hasMaterial && (entity.material.shader == scene::MaterialShader::Emissive ||
                            entity.material.shader == scene::MaterialShader::Hologram)) {
            const float strength = 0.6f + entity.material.emissive * 3.0f;
            emission = VMul(entity.material.baseColor, strength);
        }

        const Vec3 rotation = entity.transform.rotation;
        const auto rotateNormal = [&rotation](Vec3 n) {
            const float rx = rotation.x * kPi / 180.0f;
            const float ry = rotation.y * kPi / 180.0f;
            const float rz = rotation.z * kPi / 180.0f;
            {
                const float c = std::cos(rx), s = std::sin(rx);
                n = Vec3(n.x, n.y * c - n.z * s, n.y * s + n.z * c);
            }
            {
                const float c = std::cos(ry), s = std::sin(ry);
                n = Vec3(n.x * c + n.z * s, n.y, -n.x * s + n.z * c);
            }
            {
                const float c = std::cos(rz), s = std::sin(rz);
                n = Vec3(n.x * c - n.y * s, n.x * s + n.y * c, n.z);
            }
            return VNormalize(n);
        };

        std::vector<Vec3> positions(mesh->vertices.size());
        std::vector<Vec3> normals(mesh->vertices.size());
        for (std::size_t i = 0; i < mesh->vertices.size(); ++i) {
            positions[i] = worldMatrix(entity, mesh->vertices[i].position);
            normals[i] = rotateNormal(mesh->vertices[i].normal);
        }
        for (std::size_t i = 0; i + 2 < mesh->indices.size(); i += 3) {
            const unsigned int i0 = mesh->indices[i];
            const unsigned int i1 = mesh->indices[i + 1];
            const unsigned int i2 = mesh->indices[i + 2];
            if (i0 >= positions.size() || i1 >= positions.size() || i2 >= positions.size()) continue;
            Triangle triangle;
            triangle.a = positions[i0];
            triangle.b = positions[i1];
            triangle.c = positions[i2];
            triangle.na = normals[i0];
            triangle.nb = normals[i1];
            triangle.nc = normals[i2];
            triangle.albedo = albedo;
            triangle.emission = emission;
            triangle.roughness = hasMaterial ? entity.material.roughness : 0.55f;
            triangle.metallic = hasMaterial ? entity.material.metallic : 0.0f;
            triangles_.push_back(triangle);
        }
    }

    // The editor draws its ground as a grid rather than geometry, so the tracer adds a
    // matte floor plane. Without it, renders read as objects floating in the sky.
    bool hasFloor = false;
    for (const auto& entity : scene.Entities()) {
        if (entity.active && entity.Has(scene::Component::Mesh) &&
            entity.mesh.shape == scene::MeshShape::Plane) {
            hasFloor = true;
            break;
        }
    }
    if (!hasFloor) {
        const float extent = 260.0f;
        const Vec3 corners[4] = {Vec3(-extent, 0.0f, -extent), Vec3(extent, 0.0f, -extent),
                                 Vec3(extent, 0.0f, extent), Vec3(-extent, 0.0f, extent)};
        const Vec3 up(0.0f, 1.0f, 0.0f);
        const int indices[6] = {0, 3, 2, 0, 2, 1};
        for (int index : indices) {
            Triangle triangle;
            triangle.a = corners[index];
            triangle.b = corners[(index + 1) % 4];
            triangle.c = corners[(index + 2) % 4];
            triangle.na = triangle.nb = triangle.nc = up;
            triangle.albedo = Vec3(0.17f, 0.18f, 0.21f);
            triangle.roughness = 0.9f;
            triangles_.push_back(triangle);
        }
    }

    order_.resize(triangles_.size());
    for (std::size_t i = 0; i < order_.size(); ++i) order_[i] = static_cast<int>(i);
    if (!triangles_.empty()) BuildNode(0, static_cast<int>(order_.size()), 0);
}

int TraceScene::BuildNode(int start, int count, int depth) {
    const int index = static_cast<int>(nodes_.size());
    nodes_.push_back(Node{});

    Vec3 min(1e30f, 1e30f, 1e30f);
    Vec3 max(-1e30f, -1e30f, -1e30f);
    for (int i = 0; i < count; ++i) {
        const Triangle& triangle = triangles_[static_cast<std::size_t>(order_[static_cast<std::size_t>(start + i)])];
        min = VMin3(min, VMin3(triangle.a, VMin3(triangle.b, triangle.c)));
        max = VMax3(max, VMax3(triangle.a, VMax3(triangle.b, triangle.c)));
    }
    nodes_[static_cast<std::size_t>(index)].min = min;
    nodes_[static_cast<std::size_t>(index)].max = max;

    constexpr int kLeafSize = 8;
    if (count <= kLeafSize || depth > 24) {
        nodes_[static_cast<std::size_t>(index)].start = start;
        nodes_[static_cast<std::size_t>(index)].count = count;
        return index;
    }

    // Split at the median along the longest axis: no SAH, but linear time and good enough for
    // the level sizes the editor deals with.
    const Vec3 extent = VSub(max, min);
    const int axis = extent.x > extent.y ? (extent.x > extent.z ? 0 : 2)
                                         : (extent.y > extent.z ? 1 : 2);
    const int mid = start + count / 2;
    std::nth_element(order_.begin() + start, order_.begin() + mid, order_.begin() + start + count,
                     [&](int lhs, int rhs) {
                         const Triangle& a = triangles_[static_cast<std::size_t>(lhs)];
                         const Triangle& b = triangles_[static_cast<std::size_t>(rhs)];
                         const float ca = (a.a.x + a.b.x + a.c.x + a.a.y + a.b.y + a.c.y +
                                           a.a.z + a.b.z + a.c.z) / 3.0f;
                         const float cb = (b.a.x + b.b.x + b.c.x + b.a.y + b.b.y + b.c.y +
                                           b.a.z + b.b.z + b.c.z) / 3.0f;
                         if (axis == 0) return a.a.x < b.a.x;
                         if (axis == 1) return a.a.y < b.a.y;
                         (void)ca;
                         (void)cb;
                         return a.a.z < b.a.z;
                     });

    const int left = BuildNode(start, mid - start, depth + 1);
    const int right = BuildNode(mid, start + count - mid, depth + 1);
    nodes_[static_cast<std::size_t>(index)].left = left;
    nodes_[static_cast<std::size_t>(index)].right = right;
    return index;
}

bool TraceScene::Intersect(const Vec3& origin, const Vec3& direction, float maxDistance, Hit& hit,
                           std::uint64_t* rayCounter) const {
    if (nodes_.empty()) return false;
    if (rayCounter) ++(*rayCounter);

    const Vec3 inverse(1.0f / (direction.x == 0.0f ? 1e-9f : direction.x),
                       1.0f / (direction.y == 0.0f ? 1e-9f : direction.y),
                       1.0f / (direction.z == 0.0f ? 1e-9f : direction.z));

    float best = maxDistance;
    int bestTriangle = -1;
    float bestU = 0.0f;
    float bestV = 0.0f;

    int stack[64];
    int stackSize = 0;
    stack[stackSize++] = 0;
    while (stackSize > 0) {
        const Node& node = nodes_[static_cast<std::size_t>(stack[--stackSize])];
        if (!Slab(node.min, node.max, origin, inverse, best)) continue;
        if (node.count > 0) {
            for (int i = 0; i < node.count; ++i) {
                const int triangleIndex = order_[static_cast<std::size_t>(node.start + i)];
                float t = 0.0f, u = 0.0f, v = 0.0f;
                if (IntersectTriangle(triangles_[static_cast<std::size_t>(triangleIndex)], origin,
                                      direction, best, t, u, v)) {
                    best = t;
                    bestTriangle = triangleIndex;
                    bestU = u;
                    bestV = v;
                }
            }
            continue;
        }
        if (node.left >= 0 && stackSize < 63) stack[stackSize++] = node.left;
        if (node.right >= 0 && stackSize < 63) stack[stackSize++] = node.right;
    }

    if (bestTriangle < 0) return false;
    const Triangle& triangle = triangles_[static_cast<std::size_t>(bestTriangle)];
    hit.t = best;
    hit.position = VAdd(origin, VMul(direction, best));
    const Vec3 normal = VNormalize(VAdd(triangle.na,
                                      VAdd(VMul(VSub(triangle.nb, triangle.na), bestU),
                                          VMul(VSub(triangle.nc, triangle.na), bestV))));
    hit.normal = VDot(normal, direction) > 0.0f ? VMul(normal, -1.0f) : normal;
    hit.albedo = triangle.albedo;
    hit.emission = triangle.emission;
    hit.roughness = triangle.roughness;
    hit.metallic = triangle.metallic;
    hit.primitive = bestTriangle;
    return true;
}

bool TraceScene::Occluded(const Vec3& origin, const Vec3& direction, float maxDistance) const {
    Hit hit;
    return Intersect(origin, direction, maxDistance, hit, nullptr);
}

image::Buffer Render(const TraceScene& scene, const Camera& camera, const TraceSettings& settings,
                     TraceStats* stats, const std::function<void(float)>& progress) {
    const auto startTime = std::chrono::steady_clock::now();
    const int width = std::clamp(settings.width, 16, 4096);
    const int height = std::clamp(settings.height, 9, 4096);
    const int samples = std::clamp(settings.samples, 1, 4096);
    const int bounces = std::clamp(settings.maxBounces, 0, 12);

    image::Buffer buffer(width, height);

    Vec3 forward, right, up;
    camera.Basis(forward, right, up);
    const Vec3 eye = camera.Position();
    const float aspect = static_cast<float>(width) / static_cast<float>(height);
    const float tanHalf = std::tan(camera.fovDeg * kPi / 360.0f);

    const int threadCount =
        std::max(1, std::min(32, static_cast<int>(std::thread::hardware_concurrency())));
    std::atomic<int> rowsDone{0};
    std::vector<std::uint64_t> rayCounts(static_cast<std::size_t>(threadCount), 0);
    std::vector<std::thread> workers;
    workers.reserve(static_cast<std::size_t>(threadCount));

    // The sun is a disk, not a point: sampling it per shadow ray is what gives soft shadows.
    const Vec3 sunDirection = scene.SunDirection();
    Vec3 sunTangent = VNormalize(VCross(sunDirection, Vec3(0.0f, 1.0f, 0.0f)));
    if (VLength(sunTangent) < 0.1f) sunTangent = Vec3(1.0f, 0.0f, 0.0f);
    const Vec3 sunBitangent = VCross(sunDirection, sunTangent);
    const float sunRadius = std::tan(settings.sunAngularRadiusDeg * kPi / 180.0f);

    for (int thread = 0; thread < threadCount; ++thread) {
        workers.emplace_back([&, thread]() {
            Rng rng(static_cast<std::uint32_t>(thread * 7919 + 17));
            std::uint64_t rays = 0;

            for (int y = 0; y < height; ++y) {
                if ((y % threadCount) != thread) continue;
                for (int x = 0; x < width; ++x) {
                    Vec3 color(0.0f, 0.0f, 0.0f);
                    for (int sample = 0; sample < samples; ++sample) {
                        const float jx = (static_cast<float>(x) + rng.Float()) / static_cast<float>(width);
                        const float jy = (static_cast<float>(y) + rng.Float()) / static_cast<float>(height);
                        const float sx = (jx * 2.0f - 1.0f) * tanHalf * aspect;
                        const float sy = (1.0f - jy * 2.0f) * tanHalf;
                        const Vec3 direction =
                            VNormalize(VAdd(forward, VAdd(VMul(right, sx), VMul(up, sy))));

                        // Path: throughput-weighted radiance accumulation.
                        Vec3 radiance(0.0f, 0.0f, 0.0f);
                        Vec3 throughput(1.0f, 1.0f, 1.0f);
                        Vec3 origin = eye;
                        Vec3 rayDirection = direction;
                        bool bounced = false;

                        for (int bounce = 0; bounce <= bounces; ++bounce) {
                            TraceScene::Hit hit;
                            if (!scene.Intersect(origin, rayDirection, 1e9f, hit, &rays)) {
                                // Sky: a vertical gradient with a soft sun disk.
                                const float up01 = std::clamp(rayDirection.y * 0.5f + 0.5f, 0.0f, 1.0f);
                                Vec3 sky = VLerp(scene.SkyHorizon(), scene.SkyTop(),
                                                std::pow(up01, 0.6f));
                                const float sunCos = VDot(rayDirection, VMul(sunDirection, -1.0f));
                                const float disk = sunCos > 0.9995f
                                                       ? 12.0f
                                                       : std::pow(std::max(0.0f, sunCos), 220.0f) *
                                                             scene.SunIntensity() * 2.0f;
                                radiance = VAdd(radiance,
                                               VMulVec(throughput, VAdd(sky, VMul(scene.SunColor(), disk))));
                                break;
                            }

                            if (!bounced) {
                                radiance = VAdd(radiance, hit.emission);
                            } else {
                                radiance = VAdd(radiance, VMulVec(throughput, hit.emission));
                            }

                            if (bounce == bounces) break;

                            // Direct sunlight with a soft shadow ray towards the sun disk.
                            const Vec3 sunSample = VNormalize(VAdd(
                                VMul(sunDirection, -1.0f),
                                VAdd(VMul(sunTangent, (rng.Float() * 2.0f - 1.0f) * sunRadius),
                                    VMul(sunBitangent, (rng.Float() * 2.0f - 1.0f) * sunRadius))));
                            const float sunDot = VDot(hit.normal, sunSample);
                            if (sunDot > 0.0f) {
                                const Vec3 shadowOrigin = VAdd(hit.position, VMul(hit.normal, kEpsilon * 4.0f));
                                if (!scene.Occluded(shadowOrigin, sunSample, 1e9f)) {
                                    const float ndotl = std::max(0.0f, sunDot);
                                    // Lambert BRDF: 1/pi keeps a bright sun from clipping the
                                    // whole frame to white at the default exposure.
                                    const Vec3 direct = VMulVec(
                                        hit.albedo,
                                        VMul(scene.SunColor(),
                                             ndotl * scene.SunIntensity() * (1.0f / kPi)));
                                    radiance = VAdd(radiance, VMulVec(throughput, direct));
                                }
                            }

                            // Ambient: hemisphere sky light, optionally with a short AO ray.
                            float occlusion = 1.0f;
                            if (settings.ambientOcclusion) {
                                int blocked = 0;
                                for (int ao = 0; ao < 2; ++ao) {
                                    Vec3 dir = rng.UnitSphere();
                                    if (VDot(dir, hit.normal) < 0.0f) dir = VMul(dir, -1.0f);
                                    if (scene.Occluded(VAdd(hit.position, VMul(hit.normal, kEpsilon * 4.0f)),
                                                       VNormalize(dir), 2.5f)) {
                                        ++blocked;
                                    }
                                }
                                occlusion = 1.0f - static_cast<float>(blocked) * 0.35f;
                            }
                            // Sky dome irradiance, also with the diffuse 1/pi factor and a
                            // little extra weight so shadowed sides still read as lit.
                            const Vec3 ambient = VMulVec(
                                hit.albedo,
                                VMul(scene.SkyHorizon(),
                                     (0.30f + 0.22f * hit.normal.y) * (1.6f / kPi)));
                            radiance = VAdd(radiance, VMulVec(throughput, VMul(ambient, occlusion)));

                            // Bounce: cosine-weighted diffuse or a glossy reflection.
                            const bool mirror = settings.reflections && rng.Float() < hit.metallic;
                            Vec3 nextDirection;
                            if (mirror) {
                                const Vec3 reflected = VSub(rayDirection,
                                                           VMul(hit.normal, 2.0f * VDot(rayDirection, hit.normal)));
                                const float jitter = hit.roughness * hit.roughness;
                                nextDirection = VNormalize(VAdd(reflected, VMul(rng.UnitSphere(), jitter)));
                                throughput = VMulVec(throughput, hit.albedo);
                            } else {
                                Vec3 dir = rng.UnitSphere();
                                if (VDot(dir, hit.normal) < 0.0f) dir = VMul(dir, -1.0f);
                                nextDirection = VNormalize(VAdd(hit.normal, dir));
                                throughput = VMulVec(throughput, hit.albedo);
                            }
                            origin = VAdd(hit.position, VMul(hit.normal, kEpsilon * 6.0f));
                            rayDirection = nextDirection;
                            bounced = true;

                            // Russian roulette keeps long paths from dominating the cost.
                            if (bounce >= 1) {
                                const float survival = std::min(0.9f, std::max(0.15f, VLength(throughput)));
                                if (rng.Float() > survival) break;
                                throughput = VMul(throughput, 1.0f / survival);
                            }
                        }
                        color = VAdd(color, radiance);
                    }
                    color = VMul(color, 1.0f / static_cast<float>(samples));

                    // ACES-style tonemap + exposure, then sRGB encode for the PNG. The curve
                    // keeps highlights on the sun disk from clipping to flat white.
                    const float exposure = settings.exposure;
                    const auto tonemap = [exposure](float value) {
                        const float x = value * exposure;
                        const float a = 2.51f, b = 0.03f, c = 2.43f, d = 0.59f, e = 0.14f;
                        return std::clamp((x * (a * x + b)) / (x * (c * x + d) + e), 0.0f, 1.0f);
                    };
                    const auto encode = [](float value) {
                        return std::pow(std::clamp(value, 0.0f, 1.0f), 1.0f / 2.2f);
                    };
                    buffer.SetPixel(x, y,
                                    image::Color(encode(tonemap(color.x)), encode(tonemap(color.y)),
                                                 encode(tonemap(color.z)), 1.0f));
                }
                const int done = ++rowsDone;
                if (progress && (done % 4 == 0 || done == height)) {
                    progress(static_cast<float>(done) / static_cast<float>(height));
                }
            }
            rayCounts[static_cast<std::size_t>(thread)] = rays;
        });
    }
    for (auto& worker : workers) worker.join();

    if (settings.denoise) {
        buffer.BoxBlur(1);
    }

    if (stats) {
        stats->triangles = scene.TriangleCount();
        stats->nodes = scene.NodeCount();
        stats->threads = threadCount;
        stats->rays = 0;
        for (std::uint64_t count : rayCounts) stats->rays += count;
        stats->milliseconds =
            std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - startTime)
                .count();
    }
    return buffer;
}

bool RenderToFile(const scene::Scene& source, const Camera& camera, const TraceSettings& settings,
                  const std::string& path, TraceStats* stats) {
    TraceScene scene;
    scene.Build(source);
    if (scene.Empty()) return false;
    const image::Buffer buffer = Render(scene, camera, settings, stats);
    return image::WritePng(buffer, path);
}

const char* TraceBackendName(TraceBackend backend) {
    switch (backend) {
        case TraceBackend::Gpu:
            return "GPU (D3D12 compute)";
        case TraceBackend::Cpu:
        default:
            return "CPU (all cores)";
    }
}

bool TraceBackendAvailable(TraceBackend backend) {
    // The CPU backend is always available; the D3D12 compute backend lands with the render
    // graph, so the UI disables it instead of silently falling back.
    return backend == TraceBackend::Cpu;
}

}  // namespace astra::render
