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
    ~Renderer();

private:
    struct Mesh {
        GLuint vao = 0, vbo = 0, instanceVbo = 0;
        GLsizei vertexCount = 0;
    };

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
