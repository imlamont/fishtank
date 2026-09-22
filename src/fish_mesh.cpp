#include "fishtank/fish_mesh.h"
#include "fishtank/math3d.h"

namespace ft {

namespace {

void pushTri(std::vector<MeshVertex>& out, const Vec3& a, const Vec3& b, const Vec3& c) {
    Vec3 n = cross(b - a, c - a).normalized();
    out.push_back({a.x, a.y, a.z, n.x, n.y, n.z});
    out.push_back({b.x, b.y, b.z, n.x, n.y, n.z});
    out.push_back({c.x, c.y, c.z, n.x, n.y, n.z});
}

} // namespace

std::vector<MeshVertex> buildFishMesh() {
    std::vector<MeshVertex> verts;
    verts.reserve(8 * 3 + 2 * 3 + 4 * 3);

    // Body: a stretched bipyramid. Authored facing +X (nose), origin near center.
    Vec3 nose(0.55f, 0.0f, 0.0f);
    Vec3 tailBase(-0.35f, 0.0f, 0.0f);
    Vec3 top(0.0f, 0.22f, 0.0f);
    Vec3 bottom(0.0f, -0.16f, 0.0f);
    Vec3 left(0.0f, 0.02f, 0.18f);
    Vec3 right(0.0f, 0.02f, -0.18f);

    // Nose -> ring (4 faces).
    pushTri(verts, nose, top, left);
    pushTri(verts, nose, left, bottom);
    pushTri(verts, nose, bottom, right);
    pushTri(verts, nose, right, top);

    // Ring -> tail base (4 faces).
    pushTri(verts, tailBase, left, top);
    pushTri(verts, tailBase, bottom, left);
    pushTri(verts, tailBase, right, bottom);
    pushTri(verts, tailBase, top, right);

    // Caudal (tail) fin: a flat V behind the tail base, in the XY plane.
    Vec3 tailTip(-0.62f, 0.0f, 0.0f);
    Vec3 tailUpper(-0.5f, 0.22f, 0.0f);
    Vec3 tailLower(-0.5f, -0.22f, 0.0f);
    pushTri(verts, tailBase, tailUpper, tailTip);
    pushTri(verts, tailBase, tailTip, tailLower);

    // Dorsal fin: a small flat triangle standing up off the back.
    Vec3 dorsalFront(0.05f, 0.22f, 0.0f);
    Vec3 dorsalBack(-0.2f, 0.22f, 0.0f);
    Vec3 dorsalPeak(-0.08f, 0.4f, 0.0f);
    pushTri(verts, dorsalFront, dorsalPeak, dorsalBack);

    // Pectoral fins: two small flat triangles on either side, near the nose.
    Vec3 finRoot(0.15f, -0.05f, 0.0f);
    Vec3 finTipL(0.0f, -0.2f, 0.22f);
    Vec3 finTipR(0.0f, -0.2f, -0.22f);
    pushTri(verts, finRoot, finTipL, Vec3(finRoot.x, finRoot.y, finRoot.z + 0.001f));
    pushTri(verts, finRoot, Vec3(finRoot.x, finRoot.y, finRoot.z - 0.001f), finTipR);

    return verts;
}

std::vector<MeshVertex> buildOctahedronMesh(float radius) {
    std::vector<MeshVertex> verts;
    Vec3 px(radius, 0, 0), nx(-radius, 0, 0);
    Vec3 py(0, radius, 0), ny(0, -radius, 0);
    Vec3 pz(0, 0, radius), nz(0, 0, -radius);

    pushTri(verts, px, py, pz);
    pushTri(verts, pz, py, nx);
    pushTri(verts, nx, py, nz);
    pushTri(verts, nz, py, px);

    pushTri(verts, px, pz, ny);
    pushTri(verts, pz, nx, ny);
    pushTri(verts, nx, nz, ny);
    pushTri(verts, nz, px, ny);

    return verts;
}

} // namespace ft
