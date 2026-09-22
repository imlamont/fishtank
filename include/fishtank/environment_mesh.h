#pragma once
#include <vector>
#include "fish_mesh.h"
#include "math3d.h"

namespace ft {

// Static tank dressing — floor, walls, plant blade — all built once at
// startup from the tank's fixed half-extents. None of these need per-frame
// CPU updates: the floor/walls are motionless, and plant sway is done
// entirely in the vertex shader (see Renderer's plant shader).

// A jittered grid spanning the tank footprint at the tank's floor height,
// in world space already (no per-instance model matrix needed — draw with
// an identity transform).
std::vector<MeshVertex> buildFloorMesh(const Vec3& halfExtents, int gridN, unsigned seed);

// The 4 vertical side walls (no top, no bottom — the floor mesh covers the
// bottom and the tank is open at the top, matching where food drops in).
// Also in world space already, outward-facing normals.
std::vector<MeshVertex> buildWallsMesh(const Vec3& halfExtents);

// A single flat plant blade, authored in LOCAL space: base at y=0, tip at
// y=1, tapering width, centered on x=0. Meant to be scaled (to a random
// height) and translated (to a random floor position) per instance, and
// bent per-vertex in the vertex shader using the blade's un-transformed
// local y as the bend weight — see Renderer's plant vertex shader.
std::vector<MeshVertex> buildPlantBladeMesh();

// A flat grid spanning the tank's XZ footprint, authored in LOCAL space at
// y=0 (translate it to the water's rest height per instance). Subdivided
// (not a single quad) so the renderer's ripple vertex shader has interior
// vertices to displace — a single quad could only ever tilt at its 4
// corners, not undulate.
std::vector<MeshVertex> buildWaterMesh(const Vec3& halfExtents, int gridN);

} // namespace ft
