#include "astra/asset/MeshBuilder.h"

#include <algorithm>
#include <cmath>

namespace astra::asset {
namespace {

constexpr float kPi = 3.14159265358979f;
constexpr float kTau = 6.28318530717959f;

using scene::MeshVertex;
using scene::Vec3;

Vec3 Add(const Vec3& a, const Vec3& b) { return Vec3(a.x + b.x, a.y + b.y, a.z + b.z); }
Vec3 Sub(const Vec3& a, const Vec3& b) { return Vec3(a.x - b.x, a.y - b.y, a.z - b.z); }
Vec3 Scale(const Vec3& a, float s) { return Vec3(a.x * s, a.y * s, a.z * s); }

Vec3 Cross(const Vec3& a, const Vec3& b) {
    return Vec3(a.y * b.z - a.z * b.y, a.z * b.x - a.x * b.z, a.x * b.y - a.y * b.x);
}

float Dot(const Vec3& a, const Vec3& b) { return a.x * b.x + a.y * b.y + a.z * b.z; }

Vec3 Normalize(const Vec3& a) {
    const float length = std::sqrt(Dot(a, a));
    if (length < 1e-6f) return Vec3(0.0f, 1.0f, 0.0f);
    return Vec3(a.x / length, a.y / length, a.z / length);
}

void AddQuad(MeshAsset& mesh, const MeshVertex& a, const MeshVertex& b, const MeshVertex& c,
             const MeshVertex& d) {
    const auto base = static_cast<u32>(mesh.vertices.size());
    mesh.vertices.push_back(a);
    mesh.vertices.push_back(b);
    mesh.vertices.push_back(c);
    mesh.vertices.push_back(d);
    mesh.indices.insert(mesh.indices.end(), {base, base + 1, base + 2, base, base + 2, base + 3});
}

MeshVertex Vertex(const Vec3& position, const Vec3& normal, float u, float v) {
    MeshVertex vertex;
    vertex.position = position;
    vertex.normal = normal;
    vertex.u = u;
    vertex.v = v;
    return vertex;
}

}  // namespace

MeshAsset MakeCube() {
    MeshAsset mesh;
    mesh.name = "Cube";
    const Vec3 normals[6] = {{1, 0, 0}, {-1, 0, 0}, {0, 1, 0}, {0, -1, 0}, {0, 0, 1}, {0, 0, -1}};
    for (int face = 0; face < 6; ++face) {
        const Vec3 normal = normals[face];
        const Vec3 u = std::fabs(normal.y) > 0.5f ? Vec3(1, 0, 0) : Vec3(0, 1, 0);
        const Vec3 v = Cross(normal, u);
        const Vec3 center = Scale(normal, 0.5f);
        AddQuad(mesh,
                Vertex(Sub(Sub(center, Scale(u, 0.5f)), Scale(v, 0.5f)), normal, 0.0f, 1.0f),
                Vertex(Sub(Add(center, Scale(u, 0.5f)), Scale(v, 0.5f)), normal, 1.0f, 1.0f),
                Vertex(Add(Add(center, Scale(u, 0.5f)), Scale(v, 0.5f)), normal, 1.0f, 0.0f),
                Vertex(Add(Sub(center, Scale(u, 0.5f)), Scale(v, 0.5f)), normal, 0.0f, 0.0f));
    }
    return mesh;
}

MeshAsset MakeSphere(int segments, int rings) {
    MeshAsset mesh;
    mesh.name = "Sphere";
    segments = std::max(3, segments);
    rings = std::max(2, rings);
    for (int ring = 0; ring <= rings; ++ring) {
        const float v = static_cast<float>(ring) / static_cast<float>(rings);
        const float phi = v * kPi;
        for (int segment = 0; segment <= segments; ++segment) {
            const float u = static_cast<float>(segment) / static_cast<float>(segments);
            const float theta = u * kTau;
            const Vec3 normal(std::sin(phi) * std::cos(theta), std::cos(phi),
                              std::sin(phi) * std::sin(theta));
            mesh.vertices.push_back(Vertex(Scale(normal, 0.5f), normal, u, v));
        }
    }
    const int stride = segments + 1;
    for (int ring = 0; ring < rings; ++ring) {
        for (int segment = 0; segment < segments; ++segment) {
            const auto a = static_cast<u32>(ring * stride + segment);
            const auto b = a + 1;
            const auto c = a + static_cast<u32>(stride) + 1;
            const auto d = a + static_cast<u32>(stride);
            mesh.indices.insert(mesh.indices.end(), {a, d, c, a, c, b});
        }
    }
    return mesh;
}

MeshAsset MakeCylinder(int segments) {
    MeshAsset mesh;
    mesh.name = "Cylinder";
    segments = std::max(3, segments);
    for (int segment = 0; segment <= segments; ++segment) {
        const float u = static_cast<float>(segment) / static_cast<float>(segments);
        const float theta = u * kTau;
        const Vec3 normal(std::cos(theta), 0.0f, std::sin(theta));
        mesh.vertices.push_back(
            Vertex(Vec3(normal.x * 0.5f, -0.5f, normal.z * 0.5f), normal, u, 1.0f));
        mesh.vertices.push_back(
            Vertex(Vec3(normal.x * 0.5f, 0.5f, normal.z * 0.5f), normal, u, 0.0f));
    }
    for (int segment = 0; segment < segments; ++segment) {
        const auto base = static_cast<u32>(segment * 2);
        mesh.indices.insert(mesh.indices.end(),
                            {base, base + 1, base + 3, base, base + 3, base + 2});
    }
    for (int cap = 0; cap < 2; ++cap) {
        const float y = cap == 0 ? -0.5f : 0.5f;
        const Vec3 normal(0.0f, cap == 0 ? -1.0f : 1.0f, 0.0f);
        const auto center = static_cast<u32>(mesh.vertices.size());
        mesh.vertices.push_back(Vertex(Vec3(0.0f, y, 0.0f), normal, 0.5f, 0.5f));
        for (int segment = 0; segment <= segments; ++segment) {
            const float t = static_cast<float>(segment) / static_cast<float>(segments);
            const float theta = t * kTau;
            mesh.vertices.push_back(
                Vertex(Vec3(std::cos(theta) * 0.5f, y, std::sin(theta) * 0.5f), normal,
                       std::cos(theta) * 0.5f + 0.5f, std::sin(theta) * 0.5f + 0.5f));
        }
        for (int segment = 0; segment < segments; ++segment) {
            const auto a = center + 1 + static_cast<u32>(segment);
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

MeshAsset MakeCone(int segments) {
    MeshAsset mesh;
    mesh.name = "Cone";
    segments = std::max(3, segments);
    const Vec3 apex(0.0f, 0.5f, 0.0f);
    for (int segment = 0; segment < segments; ++segment) {
        const float t0 = static_cast<float>(segment) / static_cast<float>(segments);
        const float t1 = static_cast<float>(segment + 1) / static_cast<float>(segments);
        const Vec3 p0(std::cos(t0 * kTau) * 0.5f, -0.5f, std::sin(t0 * kTau) * 0.5f);
        const Vec3 p1(std::cos(t1 * kTau) * 0.5f, -0.5f, std::sin(t1 * kTau) * 0.5f);
        const Vec3 sideNormal = Normalize(Cross(Sub(p1, p0), Sub(apex, p0)));
        const Vec3 mid = Scale(Add(p0, p1), 0.5f);
        const Vec3 outward = Normalize(Vec3(mid.x, 0.0f, mid.z));
        const Vec3 normal = Dot(sideNormal, outward) < 0.0f ? Scale(sideNormal, -1.0f) : sideNormal;
        const auto base = static_cast<u32>(mesh.vertices.size());
        mesh.vertices.push_back(
            Vertex(apex, Normalize(Add(normal, Vec3(0.0f, 0.4f, 0.0f))), (t0 + t1) * 0.5f, 0.0f));
        mesh.vertices.push_back(Vertex(p0, normal, t0, 1.0f));
        mesh.vertices.push_back(Vertex(p1, normal, t1, 1.0f));
        mesh.indices.insert(mesh.indices.end(), {base, base + 2, base + 1});
    }
    const Vec3 down(0.0f, -1.0f, 0.0f);
    const auto center = static_cast<u32>(mesh.vertices.size());
    mesh.vertices.push_back(Vertex(Vec3(0.0f, -0.5f, 0.0f), down, 0.5f, 0.5f));
    for (int segment = 0; segment <= segments; ++segment) {
        const float t = static_cast<float>(segment) / static_cast<float>(segments);
        mesh.vertices.push_back(Vertex(Vec3(std::cos(t * kTau) * 0.5f, -0.5f,
                                           std::sin(t * kTau) * 0.5f),
                                       down, std::cos(t * kTau) * 0.5f + 0.5f,
                                       std::sin(t * kTau) * 0.5f + 0.5f));
    }
    for (int segment = 0; segment < segments; ++segment) {
        const auto a = center + 1 + static_cast<u32>(segment);
        mesh.indices.insert(mesh.indices.end(), {center, a, a + 1});
    }
    return mesh;
}

MeshAsset MakePlane(int divisions) {
    MeshAsset mesh;
    mesh.name = "Plane";
    divisions = std::max(1, divisions);
    const Vec3 up(0.0f, 1.0f, 0.0f);
    for (int z = 0; z <= divisions; ++z) {
        for (int x = 0; x <= divisions; ++x) {
            const float fx = static_cast<float>(x) / static_cast<float>(divisions) - 0.5f;
            const float fz = static_cast<float>(z) / static_cast<float>(divisions) - 0.5f;
            mesh.vertices.push_back(Vertex(Vec3(fx, 0.0f, fz), up, fx + 0.5f, fz + 0.5f));
        }
    }
    const int stride = divisions + 1;
    for (int z = 0; z < divisions; ++z) {
        for (int x = 0; x < divisions; ++x) {
            const auto a = static_cast<u32>(z * stride + x);
            const auto b = a + 1;
            const auto c = a + static_cast<u32>(stride) + 1;
            const auto d = a + static_cast<u32>(stride);
            mesh.indices.insert(mesh.indices.end(), {a, c, b, a, d, c});
        }
    }
    return mesh;
}

MeshAsset MakeTorus(int segments, int rings, float tube) {
    MeshAsset mesh;
    mesh.name = "Torus";
    segments = std::max(3, segments);
    rings = std::max(3, rings);
    tube = std::clamp(tube, 0.02f, 0.5f);
    for (int ring = 0; ring <= rings; ++ring) {
        const float v = static_cast<float>(ring) / static_cast<float>(rings);
        for (int segment = 0; segment <= segments; ++segment) {
            const float u = static_cast<float>(segment) / static_cast<float>(segments);
            const float theta = u * kTau;
            const float phi = v * kTau;
            const float radius = 0.5f - tube;
            const Vec3 center(std::cos(theta) * radius, 0.0f, std::sin(theta) * radius);
            const Vec3 normal(std::cos(theta) * std::cos(phi), std::sin(phi),
                              std::sin(theta) * std::cos(phi));
            mesh.vertices.push_back(Vertex(Add(center, Scale(normal, tube)), Normalize(normal), u, v));
        }
    }
    const int stride = segments + 1;
    for (int ring = 0; ring < rings; ++ring) {
        for (int segment = 0; segment < segments; ++segment) {
            const auto a = static_cast<u32>(ring * stride + segment);
            const auto b = a + 1;
            const auto c = a + static_cast<u32>(stride) + 1;
            const auto d = a + static_cast<u32>(stride);
            mesh.indices.insert(mesh.indices.end(), {a, d, c, a, c, b});
        }
    }
    return mesh;
}

MeshAsset MakeTriangle() {
    MeshAsset mesh;
    mesh.name = "Triangle";
    mesh.vertices.push_back(Vertex(Vec3(-0.5f, -0.5f, 0.0f), Vec3(0, 0, 1), 0.0f, 1.0f));
    mesh.vertices.push_back(Vertex(Vec3(0.5f, -0.5f, 0.0f), Vec3(0, 0, 1), 1.0f, 1.0f));
    mesh.vertices.push_back(Vertex(Vec3(0.0f, 0.5f, 0.0f), Vec3(0, 0, 1), 0.5f, 0.0f));
    mesh.indices = {0, 1, 2};
    return mesh;
}

const MeshAsset& Primitive(MeshShape shape) {
    static const MeshAsset cube = MakeCube();
    static const MeshAsset sphere = MakeSphere();
    static const MeshAsset cylinder = MakeCylinder();
    static const MeshAsset cone = MakeCone();
    // Tessellated: the rasteriser sorts triangles by depth, so a single huge quad would
    // sometimes paint over actors standing on it.
    static const MeshAsset plane = MakePlane(8);
    static const MeshAsset triangle = MakeTriangle();
    switch (shape) {
        case MeshShape::Sphere:
            return sphere;
        case MeshShape::Cylinder:
            return cylinder;
        case MeshShape::Cone:
            return cone;
        case MeshShape::Plane:
            return plane;
        case MeshShape::Custom:
            return triangle;  // callers resolve custom geometry through the scene
        case MeshShape::Cube:
        default:
            return cube;
    }
}

void Recenter(MeshAsset& mesh) {
    if (mesh.vertices.empty()) return;
    Vec3 min, max;
    mesh.Bounds(min, max);
    const Vec3 center((min.x + max.x) * 0.5f, (min.y + max.y) * 0.5f, (min.z + max.z) * 0.5f);
    for (auto& vertex : mesh.vertices) {
        vertex.position = Vec3(vertex.position.x - center.x, vertex.position.y - center.y,
                               vertex.position.z - center.z);
    }
}

void FitToUnitBox(MeshAsset& mesh) {
    if (mesh.vertices.empty()) return;
    Vec3 min, max;
    mesh.Bounds(min, max);
    const float extent = std::max({max.x - min.x, max.y - min.y, max.z - min.z, 1e-4f});
    const float scale = 1.0f / extent;
    const Vec3 center((min.x + max.x) * 0.5f, (min.y + max.y) * 0.5f, (min.z + max.z) * 0.5f);
    for (auto& vertex : mesh.vertices) {
        vertex.position = Vec3((vertex.position.x - center.x) * scale,
                               (vertex.position.y - center.y) * scale,
                               (vertex.position.z - center.z) * scale);
    }
}

}  // namespace astra::asset
