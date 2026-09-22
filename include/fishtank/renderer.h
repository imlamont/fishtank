#pragma once
#include <memory>
#include "gl_compat.h"
#include "shader.h"
#include "boids.h"
#include "math3d.h"
#include "fish_mesh.h"
#include "plants.h"

namespace ft {

// Owns all GL objects and draws one frame of the tank given the current
// simulation state. Nothing here touches simulation logic.
class Renderer {
public:
    // Needs sim at init time (not just render time) so the floor, walls,
    // and plant scatter can be sized/seeded to the tank's actual
    // half-extents once, up front, instead of being rebuilt every frame.
    bool init(const Boids& sim);
    void resize(int widthPx, int heightPx);
    void render(const Boids& sim, float timeSeconds);

    // Casts a ray from the camera through the given screen point (NDC,
    // x/y in [-1, 1], y-up) and raymarches it against the tank's *animated*
    // water surface (the same ripple the water mesh renders with) to find
    // where it crosses — i.e. converts a click/tap into the same world
    // (x, z) that ends up on screen where the user actually clicked. Uses
    // the exact same camera as render() for the given sim/timeSeconds, so
    // call it with the same timeSeconds you're about to render with.
    Vec3 pickSurfacePoint(float ndcX, float ndcY, const Boids& sim, float timeSeconds) const;

    // Orbits the camera around the tank. dYaw/dPitch are in radians and
    // added directly to the current orbit angles — the caller (App::orbit)
    // is responsible for converting a drag distance into an angle. Pitch is
    // clamped to a range that keeps pickSurfacePoint()'s ray-plane
    // intersection numerically stable (see CLAUDE.md) and avoids the
    // lookAt basis degenerating near straight-down.
    void addOrbitDelta(float dYaw, float dPitch);

    ~Renderer();

private:
    struct Mesh {
        GLuint vao = 0, vbo = 0, instanceVbo = 0;
        GLsizei vertexCount = 0;
    };

    Mat4 computeViewProj(const Boids& sim, float timeSeconds) const;

    // extraFloatAttribs: additional scalar (float) per-instance attributes
    // beyond the standard mat4 + vec3 color, bound at consecutive locations
    // starting at 7. Used by the plant mesh (phase, amplitude, speed).
    Mesh createMesh(const std::vector<MeshVertex>& verts, int maxInstances, int extraFloatAttribs = 0);
    void uploadInstances(Mesh& mesh, const void* data, size_t byteSize);
    void drawInstanced(const Mesh& mesh, int instanceCount);

    Shader shader_;
    Shader plantShader_;
    Shader wallShader_;
    Shader waterShader_;
    Mesh fishMesh_;
    Mesh foodMesh_;
    Mesh floorMesh_;
    Mesh wallsMesh_;
    Mesh plantMesh_;
    Mesh waterMesh_;
    std::unique_ptr<Plants> plants_;
    int width_ = 1, height_ = 1;

    float orbitYaw_ = 0.0f;   // radians, around the tank's vertical (Y) axis
    float orbitPitch_ = 0.0f; // radians, offset from the base downward tilt

    std::vector<float> instanceScratch_; // reused each frame to avoid per-frame heap churn
};

} // namespace ft
