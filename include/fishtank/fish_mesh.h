#pragma once
#include <vector>

namespace ft {

// One interleaved vertex: position (3) + flat-face normal (3).
struct MeshVertex {
    float px, py, pz;
    float nx, ny, nz;
};

// Builds a small low-poly fish authored facing +X, centered near the origin,
// roughly 1 unit long. Triangle list, flat-shaded (non-shared vertices per
// face so each gets its own face normal).
std::vector<MeshVertex> buildFishMesh();

// A tiny flat-shaded octahedron, used for food pellets.
std::vector<MeshVertex> buildOctahedronMesh(float radius);

} // namespace ft
