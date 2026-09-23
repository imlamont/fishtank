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

// The floor's per-vertex height jitter can dip up to this far below
// halfExtents.y — including at the floor's own edge, right where it meets
// the walls. buildWallsMesh reads this too, extending its bottom edge
// further down than that worst case (see kWallBottomExtraDepth below), or
// the floor's deepest dip at the boundary pokes out past the wall.
constexpr float kFloorJitterAmp = 0.12f; // absolute world units — small undulation, not dunes

} // namespace

std::vector<MeshVertex> buildFloorMesh(const Vec3& halfExtents, int gridN, unsigned seed) {
    std::vector<MeshVertex> verts;
    if (gridN < 1) gridN = 1;
    verts.reserve((size_t)gridN * gridN * 6);

    const float floorY = -halfExtents.y;

    auto gridPoint = [&](int i, int j) {
        float x = -halfExtents.x + (2.0f * halfExtents.x) * ((float)i / gridN);
        float z = -halfExtents.z + (2.0f * halfExtents.z) * ((float)j / gridN);
        float y = floorY + (hash01(i, j, seed) - 0.5f) * 2.0f * kFloorJitterAmp;
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
    float hx = halfExtents.x, hz = halfExtents.z;

    // The floor can dip as low as halfExtents.y + kFloorJitterAmp, right at
    // its own edge where it meets the walls (see kFloorJitterAmp's
    // comment) — extend the walls' bottom edge past that worst case (with
    // a bit of margin) so there's no gap between the two at any floor seed.
    float wallBottomY = -halfExtents.y - kFloorJitterAmp - 0.08f;
    float hy = halfExtents.y; // top edge is unaffected, keep the name for the top corners below

    // Left wall (x = -hx), outward normal -X: default winding already
    // gives -X here (see CLAUDE.md derivation).
    {
        Vec3 a(-hx, wallBottomY, -hz), b(-hx, wallBottomY, hz), c(-hx, hy, hz), d(-hx, hy, -hz);
        pushTri(verts, a, b, c);
        pushTri(verts, a, c, d);
    }
    // Right wall (x = +hx), outward normal +X: needs flipped winding.
    {
        Vec3 a(hx, wallBottomY, -hz), b(hx, wallBottomY, hz), c(hx, hy, hz), d(hx, hy, -hz);
        pushTri(verts, a, c, b);
        pushTri(verts, a, d, c);
    }
    // Back wall (z = -hz), outward normal -Z: needs flipped winding.
    {
        Vec3 a(-hx, wallBottomY, -hz), b(hx, wallBottomY, -hz), c(hx, hy, -hz), d(-hx, hy, -hz);
        pushTri(verts, a, c, b);
        pushTri(verts, a, d, c);
    }
    // Front wall (z = +hz), outward normal +Z: default winding.
    {
        Vec3 a(-hx, wallBottomY, hz), b(hx, wallBottomY, hz), c(hx, hy, hz), d(-hx, hy, hz);
        pushTri(verts, a, b, c);
        pushTri(verts, a, c, d);
    }
    return verts;
}

std::vector<MeshVertex> buildWaterMesh(const Vec3& halfExtents, int gridN) {
    std::vector<MeshVertex> verts;
    if (gridN < 1) gridN = 1;
    verts.reserve((size_t)gridN * gridN * 6);

    auto gridPoint = [&](int i, int j) {
        float x = -halfExtents.x + (2.0f * halfExtents.x) * ((float)i / gridN);
        float z = -halfExtents.z + (2.0f * halfExtents.z) * ((float)j / gridN);
        return Vec3(x, 0.0f, z); // flat at rest — the renderer's shader adds the ripple
    };

    for (int i = 0; i < gridN; ++i) {
        for (int j = 0; j < gridN; ++j) {
            Vec3 a = gridPoint(i, j);
            Vec3 b = gridPoint(i + 1, j);
            Vec3 c = gridPoint(i + 1, j + 1);
            Vec3 d = gridPoint(i, j + 1);
            // Flipped winding for +Y normal, same reasoning as the floor.
            pushTri(verts, a, c, b);
            pushTri(verts, a, d, c);
        }
    }
    return verts;
}

namespace {

// One flat tapering panel, in the local XY plane (z=0) — the single-panel
// version this used to be, factored out so buildPlantBladeMesh can cross
// two of them.
void appendFlatBladePanel(std::vector<MeshVertex>& verts) {
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
}

} // namespace

std::vector<MeshVertex> buildPlantBladeMesh() {
    // A single flat panel reads as a razor-thin line when viewed edge-on —
    // there's no way for a flat quad to look 3D from every horizontal
    // angle. Crossing two panels at 90 degrees (the classic billboard-
    // grass trick) gives real volume from any side, cheaply: still just 2
    // panels, and backface culling is already off for plants so both
    // faces of both panels render.
    std::vector<MeshVertex> verts;
    appendFlatBladePanel(verts);

    // Rotate this first panel's vertices 90 degrees around Y (local up),
    // then add a second, unrotated panel — together they form an X-shaped
    // cross-section.
    for (auto& v : verts) {
        float x = v.px, z = v.pz, nx = v.nx, nz = v.nz;
        v.px = -z; v.pz = x;
        v.nx = -nz; v.nz = nx;
    }
    appendFlatBladePanel(verts);
    return verts;
}

} // namespace ft
