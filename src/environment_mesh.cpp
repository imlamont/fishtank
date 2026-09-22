#include "fishtank/environment_mesh.h"
#include <cmath>

namespace ft {

namespace {

void pushTri(std::vector<MeshVertex>& out, const Vec3& a, const Vec3& b, const Vec3& c) {
    Vec3 n = cross(b - a, c - a).normalized();
    out.push_back({a.x, a.y, a.z, n.x, n.y, n.z});
    out.push_back({b.x, b.y, b.z, n.x, n.y, n.z});
    out.push_back({c.x, c.y, c.z, n.x, n.y, n.z});
}

// Deterministic, dependency-free pseudo-random in [0, 1) from a grid cell +
// seed — the classic "sine hash", fine for cosmetic terrain jitter.
float hash01(int i, int j, unsigned seed) {
    float x = std::sin((float)i * 127.1f + (float)j * 311.7f + (float)seed * 0.001f) * 43758.5453f;
    return x - std::floor(x);
}

} // namespace

std::vector<MeshVertex> buildFloorMesh(const Vec3& halfExtents, int gridN, unsigned seed) {
    std::vector<MeshVertex> verts;
    if (gridN < 1) gridN = 1;
    verts.reserve((size_t)gridN * gridN * 6);

    const float floorY = -halfExtents.y;
    const float jitterAmp = 0.12f; // absolute world units — small undulation, not dunes

    auto gridPoint = [&](int i, int j) {
        float x = -halfExtents.x + (2.0f * halfExtents.x) * ((float)i / gridN);
        float z = -halfExtents.z + (2.0f * halfExtents.z) * ((float)j / gridN);
        float y = floorY + (hash01(i, j, seed) - 0.5f) * 2.0f * jitterAmp;
        return Vec3(x, y, z);
    };

    for (int i = 0; i < gridN; ++i) {
        for (int j = 0; j < gridN; ++j) {
            Vec3 a = gridPoint(i, j);
            Vec3 b = gridPoint(i + 1, j);
            Vec3 c = gridPoint(i + 1, j + 1);
            Vec3 d = gridPoint(i, j + 1);
            // Flipped winding so the cross product faces +Y (up) — see
            // CLAUDE.md for the full derivation of wall/floor winding.
            pushTri(verts, a, c, b);
            pushTri(verts, a, d, c);
        }
    }
    return verts;
}

std::vector<MeshVertex> buildWallsMesh(const Vec3& halfExtents) {
    std::vector<MeshVertex> verts;
    float hx = halfExtents.x, hy = halfExtents.y, hz = halfExtents.z;

    // Left wall (x = -hx), outward normal -X: default winding already
    // gives -X here (see CLAUDE.md derivation).
    {
        Vec3 a(-hx, -hy, -hz), b(-hx, -hy, hz), c(-hx, hy, hz), d(-hx, hy, -hz);
        pushTri(verts, a, b, c);
        pushTri(verts, a, c, d);
    }
    // Right wall (x = +hx), outward normal +X: needs flipped winding.
    {
        Vec3 a(hx, -hy, -hz), b(hx, -hy, hz), c(hx, hy, hz), d(hx, hy, -hz);
        pushTri(verts, a, c, b);
        pushTri(verts, a, d, c);
    }
    // Back wall (z = -hz), outward normal -Z: needs flipped winding.
    {
        Vec3 a(-hx, -hy, -hz), b(hx, -hy, -hz), c(hx, hy, -hz), d(-hx, hy, -hz);
        pushTri(verts, a, c, b);
        pushTri(verts, a, d, c);
    }
    // Front wall (z = +hz), outward normal +Z: default winding.
    {
        Vec3 a(-hx, -hy, hz), b(hx, -hy, hz), c(hx, hy, hz), d(-hx, hy, hz);
        pushTri(verts, a, b, c);
        pushTri(verts, a, c, d);
    }
    return verts;
}

std::vector<MeshVertex> buildPlantBladeMesh() {
    std::vector<MeshVertex> verts;
    const int segments = 5;
    const float baseWidth = 0.12f;

    for (int s = 0; s < segments; ++s) {
        float t0 = (float)s / segments;
        float t1 = (float)(s + 1) / segments;
        float w0 = baseWidth * (1.0f - t0 * 0.75f);
        float w1 = baseWidth * (1.0f - t1 * 0.75f);

        Vec3 a(-w0 * 0.5f, t0, 0.0f);
        Vec3 b(w0 * 0.5f, t0, 0.0f);
        Vec3 c(w1 * 0.5f, t1, 0.0f);
        Vec3 d(-w1 * 0.5f, t1, 0.0f);
        pushTri(verts, a, b, c);
        pushTri(verts, a, c, d);
    }
    return verts;
}

} // namespace ft
