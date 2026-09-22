#include "fishtank/renderer.h"
#include "fishtank/fish_mesh.h"
#include <cmath>
#include <cstddef>

namespace ft {

namespace {

const char* kVertSrc = R"GLSL(
layout(location = 0) in vec3 aPos;
layout(location = 1) in vec3 aNormal;
layout(location = 2) in vec4 aModelCol0;
layout(location = 3) in vec4 aModelCol1;
layout(location = 4) in vec4 aModelCol2;
layout(location = 5) in vec4 aModelCol3;
layout(location = 6) in vec3 aColor;

uniform mat4 uViewProj;

out vec3 vNormal;
out vec3 vColor;

void main() {
    mat4 model = mat4(aModelCol0, aModelCol1, aModelCol2, aModelCol3);
    vec4 worldPos = model * vec4(aPos, 1.0);
    gl_Position = uViewProj * worldPos;
    vNormal = mat3(model) * aNormal;
    vColor = aColor;
}
)GLSL";

const char* kFragSrc = R"GLSL(
in vec3 vNormal;
in vec3 vColor;
out vec4 fragColor;

void main() {
    vec3 n = normalize(vNormal);
    vec3 lightDir = normalize(vec3(0.4, 0.9, 0.3));
    float diff = max(dot(n, lightDir), 0.0);
    vec3 color = vColor * 0.35 + vColor * diff * 0.85;
    fragColor = vec4(color, 1.0);
}
)GLSL";

constexpr int kMaxFishInstances = 400;
constexpr int kMaxFoodInstances = 128;
constexpr int kFloatsPerInstance = 16 + 3; // mat4 + vec3 color

Vec3 hsvToRgb(float h, float s, float v) {
    float r, g, b;
    int i = (int)(h * 6.0f);
    float f = h * 6.0f - i;
    float p = v * (1 - s);
    float q = v * (1 - f * s);
    float t = v * (1 - (1 - f) * s);
    switch (i % 6) {
        case 0: r = v; g = t; b = p; break;
        case 1: r = q; g = v; b = p; break;
        case 2: r = p; g = v; b = t; break;
        case 3: r = p; g = q; b = v; break;
        case 4: r = t; g = p; b = v; break;
        default: r = v; g = p; b = q; break;
    }
    return {r, g, b};
}

void writeInstance(std::vector<float>& buf, const Mat4& model, const Vec3& color) {
    for (float f : model.m) buf.push_back(f);
    buf.push_back(color.x);
    buf.push_back(color.y);
    buf.push_back(color.z);
}

} // namespace

Renderer::Mesh Renderer::createMesh(const std::vector<MeshVertex>& verts, int maxInstances) {
    Mesh mesh;
    mesh.vertexCount = (GLsizei)verts.size();

    glGenVertexArrays(1, &mesh.vao);
    glBindVertexArray(mesh.vao);

    glGenBuffers(1, &mesh.vbo);
    glBindBuffer(GL_ARRAY_BUFFER, mesh.vbo);
    glBufferData(GL_ARRAY_BUFFER, verts.size() * sizeof(MeshVertex), verts.data(), GL_STATIC_DRAW);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(MeshVertex), (void*)offsetof(MeshVertex, px));
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(MeshVertex), (void*)offsetof(MeshVertex, nx));

    glGenBuffers(1, &mesh.instanceVbo);
    glBindBuffer(GL_ARRAY_BUFFER, mesh.instanceVbo);
    glBufferData(GL_ARRAY_BUFFER, (GLsizeiptr)(maxInstances * kFloatsPerInstance * sizeof(float)), nullptr, GL_DYNAMIC_DRAW);

    const GLsizei stride = kFloatsPerInstance * sizeof(float);
    for (int col = 0; col < 4; ++col) {
        GLuint loc = 2 + col;
        glEnableVertexAttribArray(loc);
        glVertexAttribPointer(loc, 4, GL_FLOAT, GL_FALSE, stride, (void*)(size_t)(col * 4 * sizeof(float)));
        glVertexAttribDivisor(loc, 1);
    }
    glEnableVertexAttribArray(6);
    glVertexAttribPointer(6, 3, GL_FLOAT, GL_FALSE, stride, (void*)(size_t)(16 * sizeof(float)));
    glVertexAttribDivisor(6, 1);

    glBindVertexArray(0);
    return mesh;
}

void Renderer::uploadInstances(Mesh& mesh, const void* data, size_t byteSize) {
    glBindBuffer(GL_ARRAY_BUFFER, mesh.instanceVbo);
    glBufferSubData(GL_ARRAY_BUFFER, 0, (GLsizeiptr)byteSize, data);
}

void Renderer::drawInstanced(const Mesh& mesh, int instanceCount) {
    if (instanceCount <= 0) return;
    glBindVertexArray(mesh.vao);
    glDrawArraysInstanced(GL_TRIANGLES, 0, mesh.vertexCount, instanceCount);
    glBindVertexArray(0);
}

bool Renderer::init() {
    if (!shader_.compile(kVertSrc, kFragSrc)) return false;

    fishMesh_ = createMesh(buildFishMesh(), kMaxFishInstances);
    foodMesh_ = createMesh(buildOctahedronMesh(0.06f), kMaxFoodInstances);

    glEnable(GL_DEPTH_TEST);
    glEnable(GL_CULL_FACE);
    glCullFace(GL_BACK);

    instanceScratch_.reserve(kMaxFishInstances * kFloatsPerInstance);
    return true;
}

void Renderer::resize(int widthPx, int heightPx) {
    width_ = widthPx > 0 ? widthPx : 1;
    height_ = heightPx > 0 ? heightPx : 1;
    glViewport(0, 0, width_, height_);
}

void Renderer::render(const Boids& sim, float timeSeconds) {
    glClearColor(0.043f, 0.086f, 0.114f, 1.0f); // deep aquarium blue, dark-theme friendly
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    const Vec3& half = sim.tankHalfExtents();
    float aspect = (float)width_ / (float)height_;
    const float fovY = 0.9f;

    // Fixed front-on view with a very slight ambient sway, not a full orbit —
    // this runs embedded in a page, not as a standalone demo, so a busy
    // camera would compete with the rest of the site. Distance is derived
    // from the tank size + current aspect ratio (not hardcoded) so the whole
    // tank stays in frame whether the embed ends up wide or narrow.
    float halfFov = fovY * 0.5f;
    float distForWidth = half.x / (std::tan(halfFov) * aspect);
    float distForHeight = half.y / std::tan(halfFov);
    float eyeZ = half.z + std::max(distForWidth, distForHeight) * 1.15f;

    float sway = std::sin(timeSeconds * 0.15f) * 0.25f;
    Vec3 eye(sway, half.y * 0.15f, eyeZ);
    Vec3 center(0, 0, 0);
    Mat4 view = Mat4::lookAt(eye, center, Vec3(0, 1, 0));
    Mat4 proj = Mat4::perspective(fovY, aspect, 0.1f, 200.0f);
    Mat4 viewProj = proj * view;

    shader_.use();
    glUniformMatrix4fv(shader_.uniformLocation("uViewProj"), 1, GL_FALSE, viewProj.m);

    instanceScratch_.clear();
    int fishCount = 0;
    for (const auto& f : sim.fish()) {
        if (fishCount >= kMaxFishInstances) break;
        Mat4 basis = Mat4::basisFromForward(f.vel, Vec3(0, 1, 0));
        Mat4 model = Mat4::translation(f.pos) * basis;
        Vec3 color = hsvToRgb(f.colorHue, 0.55f, 0.95f);
        writeInstance(instanceScratch_, model, color);
        ++fishCount;
    }
    if (fishCount > 0) {
        uploadInstances(fishMesh_, instanceScratch_.data(), instanceScratch_.size() * sizeof(float));
        drawInstanced(fishMesh_, fishCount);
    }

    instanceScratch_.clear();
    int foodCount = 0;
    Vec3 foodColor(0.85f, 0.7f, 0.35f);
    for (const auto& food : sim.food()) {
        if (!food.active) continue;
        if (foodCount >= kMaxFoodInstances) break;
        Mat4 model = Mat4::translation(food.pos);
        writeInstance(instanceScratch_, model, foodColor);
        ++foodCount;
    }
    if (foodCount > 0) {
        uploadInstances(foodMesh_, instanceScratch_.data(), instanceScratch_.size() * sizeof(float));
        drawInstanced(foodMesh_, foodCount);
    }
}

Renderer::~Renderer() {
    auto destroy = [](Mesh& m) {
        if (m.vbo) glDeleteBuffers(1, &m.vbo);
        if (m.instanceVbo) glDeleteBuffers(1, &m.instanceVbo);
        if (m.vao) glDeleteVertexArrays(1, &m.vao);
    };
    destroy(fishMesh_);
    destroy(foodMesh_);
}

} // namespace ft
