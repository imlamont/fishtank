#pragma once
#include "gl_compat.h"
#include "shader.h"
#include "boids.h"
#include "math3d.h"
#include "fish_mesh.h"

namespace ft {

// Owns all GL objects and draws one frame of the tank given the current
// simulation state. Nothing here touches simulation logic.
class Renderer {
public:
    bool init();
    void resize(int widthPx, int heightPx);
    void render(const Boids& sim, float timeSeconds);

    // Casts a ray from the camera through the given screen point (NDC,
    // x/y in [-1, 1], y-up) and intersects it with the tank's water-surface
    // plane — i.e. converts a click/tap into the same world (x, z) that
    // ends up on screen where the user actually clicked, given the current
    // (possibly swaying) camera. Uses the exact same camera as render() for
    // the given sim/timeSeconds, so call it with the same timeSeconds you're
    // about to render with.
    Vec3 pickSurfacePoint(float ndcX, float ndcY, const Boids& sim, float timeSeconds) const;

    ~Renderer();

private:
    struct Mesh {
        GLuint vao = 0, vbo = 0, instanceVbo = 0;
        GLsizei vertexCount = 0;
    };

    Mat4 computeViewProj(const Boids& sim, float timeSeconds) const;

    Mesh createMesh(const std::vector<MeshVertex>& verts, int maxInstances);
    void uploadInstances(Mesh& mesh, const void* data, size_t byteSize);
    void drawInstanced(const Mesh& mesh, int instanceCount);

    Shader shader_;
    Mesh fishMesh_;
    Mesh foodMesh_;
    int width_ = 1, height_ = 1;

    std::vector<float> instanceScratch_; // reused each frame to avoid per-frame heap churn
};

} // namespace ft
