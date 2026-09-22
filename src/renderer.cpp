#include "fishtank/renderer.h"
#include "fishtank/fish_mesh.h"
#include "fishtank/environment_mesh.h"
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

uniform float uAlpha;

void main() {
    vec3 n = normalize(vNormal);
    vec3 lightDir = normalize(vec3(0.4, 0.9, 0.3));
    float diff = max(dot(n, lightDir), 0.0);
    vec3 color = vColor * 0.35 + vColor * diff * 0.85;
    fragColor = vec4(color, uAlpha);
}
)GLSL";

// Same lighting/instancing as kVertSrc, plus a per-instance sway: the blade
// mesh is authored with local y in [0, 1] (base to tip), and that RAW local
// y — before the instance's model matrix is applied — is used as the bend
// weight, so short and tall plants (different model-matrix scale) still
// bend the same way relative to their own height.
const char* kPlantVertSrc = R"GLSL(
layout(location = 0) in vec3 aPos;
layout(location = 1) in vec3 aNormal;
layout(location = 2) in vec4 aModelCol0;
layout(location = 3) in vec4 aModelCol1;
layout(location = 4) in vec4 aModelCol2;
layout(location = 5) in vec4 aModelCol3;
layout(location = 6) in vec3 aColor;
layout(location = 7) in float aSwayPhase;
layout(location = 8) in float aSwayAmplitude;
layout(location = 9) in float aSwaySpeed;

uniform mat4 uViewProj;
uniform float uTime;

out vec3 vNormal;
out vec3 vColor;

void main() {
    mat4 model = mat4(aModelCol0, aModelCol1, aModelCol2, aModelCol3);
    float bendWeight = aPos.y * aPos.y; // eases from fixed base to freely-swaying tip
    float sway = sin(uTime * aSwaySpeed + aSwayPhase) * aSwayAmplitude * bendWeight;
    vec3 bentPos = aPos + vec3(sway, 0.0, 0.0);
    vec4 worldPos = model * vec4(bentPos, 1.0);
    gl_Position = uViewProj * worldPos;
    vNormal = mat3(model) * aNormal;
    vColor = aColor;
}
)GLSL";

constexpr int kMaxFishInstances = 400;
constexpr int kMaxFoodInstances = 128;
constexpr int kMaxPlantInstances = 96;
constexpr int kFloatsPerInstance = 16 + 3; // mat4 + vec3 color
constexpr int kFloatsPerPlantInstance = kFloatsPerInstance + 3; // + phase, amplitude, speed

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

void writeInstance(std::vector<float>& buf, const Mat4& model, const Vec3& color,
                    float a, float b, float c) {
    writeInstance(buf, model, color);
    buf.push_back(a);
    buf.push_back(b);
    buf.push_back(c);
}

} // namespace

Renderer::Mesh Renderer::createMesh(const std::vector<MeshVertex>& verts, int maxInstances, int extraFloatAttribs) {
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

    const int floatsPerInstance = kFloatsPerInstance + extraFloatAttribs;
    glGenBuffers(1, &mesh.instanceVbo);
    glBindBuffer(GL_ARRAY_BUFFER, mesh.instanceVbo);
    glBufferData(GL_ARRAY_BUFFER, (GLsizeiptr)(maxInstances * floatsPerInstance * sizeof(float)), nullptr, GL_DYNAMIC_DRAW);

    const GLsizei stride = floatsPerInstance * sizeof(float);
    for (int col = 0; col < 4; ++col) {
        GLuint loc = 2 + col;
        glEnableVertexAttribArray(loc);
        glVertexAttribPointer(loc, 4, GL_FLOAT, GL_FALSE, stride, (void*)(size_t)(col * 4 * sizeof(float)));
        glVertexAttribDivisor(loc, 1);
    }
    glEnableVertexAttribArray(6);
    glVertexAttribPointer(6, 3, GL_FLOAT, GL_FALSE, stride, (void*)(size_t)(16 * sizeof(float)));
    glVertexAttribDivisor(6, 1);

    for (int i = 0; i < extraFloatAttribs; ++i) {
        GLuint loc = 7 + i;
        glEnableVertexAttribArray(loc);
        glVertexAttribPointer(loc, 1, GL_FLOAT, GL_FALSE, stride, (void*)(size_t)((19 + i) * sizeof(float)));
        glVertexAttribDivisor(loc, 1);
    }

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

bool Renderer::init(const Boids& sim) {
    if (!shader_.compile(kVertSrc, kFragSrc)) return false;
    if (!plantShader_.compile(kPlantVertSrc, kFragSrc)) return false;

    fishMesh_ = createMesh(buildFishMesh(), kMaxFishInstances);
    foodMesh_ = createMesh(buildOctahedronMesh(0.06f), kMaxFoodInstances);

    const Vec3& half = sim.tankHalfExtents();
    floorMesh_ = createMesh(buildFloorMesh(half, 10, 1234u), 1);
    wallsMesh_ = createMesh(buildWallsMesh(half), 1);
    plantMesh_ = createMesh(buildPlantBladeMesh(), kMaxPlantInstances, 3);
    plants_ = std::make_unique<Plants>(kMaxPlantInstances / 2, half, 4321u);

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

Mat4 Renderer::computeViewProj(const Boids& sim, float timeSeconds) const {
    const Vec3& half = sim.tankHalfExtents();
    float aspect = (float)width_ / (float)height_;
    const float fovY = 0.9f;

    // A downward-tilted view (not level) — mostly for looks, like peering
    // into a tank from slightly above, but it also matters for correctness:
    // pickSurfacePoint() intersects click rays with the water-surface plane,
    // which is only numerically stable if the camera actually looks down at
    // a meaningful angle. A near-level camera makes that intersection
    // ill-conditioned (a ray barely changes height per unit of depth), which
    // is exactly what caused clicks to resolve to wildly wrong positions.
    //
    // Distance is derived from the tank size + current aspect ratio (not
    // hardcoded) so the whole tank stays in frame regardless of the embed's
    // width/height; the tilt then trades a bit of that fit for a
    // well-conditioned click plane, so the margin below is generous.
    // Must clear halfFov (~26 degrees) by a comfortable margin, or the
    // top-of-frame ray (tilt - halfFov) grazes near-horizontal and the
    // surface-plane intersection goes unstable again — found by checking
    // actual picked coordinates at the frame edges, not just the center.
    const float tilt = 0.9f; // ~52 degrees downward
    float halfFov = fovY * 0.5f;
    float distForWidth = half.x / (std::tan(halfFov) * aspect);
    float distForHeight = half.y / std::tan(halfFov);
    float dist = half.z + std::max(distForWidth, distForHeight) * 1.3f;

    float sway = std::sin(timeSeconds * 0.15f) * 0.25f;
    Vec3 eye(sway, dist * std::sin(tilt), dist * std::cos(tilt));
    Vec3 center(0, 0, 0);
    Mat4 view = Mat4::lookAt(eye, center, Vec3(0, 1, 0));
    Mat4 proj = Mat4::perspective(fovY, aspect, 0.1f, 200.0f);
    return proj * view;
}

Vec3 Renderer::pickSurfacePoint(float ndcX, float ndcY, const Boids& sim, float timeSeconds) const {
    Mat4 viewProj = computeViewProj(sim, timeSeconds);
    Mat4 inv;
    if (!invert(viewProj, inv)) return Vec3(0, 0, 0);

    Vec4 nearClip{ndcX, ndcY, -1.0f, 1.0f};
    Vec4 farClip{ndcX, ndcY, 1.0f, 1.0f};
    Vec4 nearWorld4 = transform(inv, nearClip);
    Vec4 farWorld4 = transform(inv, farClip);
    Vec3 nearWorld(nearWorld4.x / nearWorld4.w, nearWorld4.y / nearWorld4.w, nearWorld4.z / nearWorld4.w);
    Vec3 farWorld(farWorld4.x / farWorld4.w, farWorld4.y / farWorld4.w, farWorld4.z / farWorld4.w);

    Vec3 ray = farWorld - nearWorld;
    // Intersect with the same height food actually spawns at (see Boids::feedAt).
    float planeY = sim.tankHalfExtents().y * 0.92f;
    if (std::fabs(ray.y) < 1e-6f) return Vec3(nearWorld.x, planeY, nearWorld.z);
    float t = (planeY - nearWorld.y) / ray.y;
    return Vec3(nearWorld.x + ray.x * t, planeY, nearWorld.z + ray.z * t);
}

void Renderer::render(const Boids& sim, float timeSeconds) {
    glClearColor(0.043f, 0.086f, 0.114f, 1.0f); // deep aquarium blue, dark-theme friendly
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    Mat4 viewProj = computeViewProj(sim, timeSeconds);

    // --- Opaque pass: floor, plants, fish, food (any order among these is fine). ---
    shader_.use();
    glUniformMatrix4fv(shader_.uniformLocation("uViewProj"), 1, GL_FALSE, viewProj.m);
    glUniform1f(shader_.uniformLocation("uAlpha"), 1.0f);

    instanceScratch_.clear();
    writeInstance(instanceScratch_, Mat4::identity(), Vec3(0.76f, 0.68f, 0.47f)); // sand
    uploadInstances(floorMesh_, instanceScratch_.data(), instanceScratch_.size() * sizeof(float));
    drawInstanced(floorMesh_, 1);

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

    // Plants aren't a closed volume (flat single-sided blades), so backface
    // culling would make them vanish from some angles — not worth doubling
    // the geometry for a handful of decorative blades, so just disable
    // culling for this one draw.
    glDisable(GL_CULL_FACE);
    plantShader_.use();
    glUniformMatrix4fv(plantShader_.uniformLocation("uViewProj"), 1, GL_FALSE, viewProj.m);
    glUniform1f(plantShader_.uniformLocation("uAlpha"), 1.0f);
    glUniform1f(plantShader_.uniformLocation("uTime"), timeSeconds);

    instanceScratch_.clear();
    int plantCount = 0;
    if (plants_) {
        for (const auto& p : plants_->instances()) {
            if (plantCount >= kMaxPlantInstances) break;
            Mat4 model = Mat4::translation(p.basePos) * Mat4::scale(p.height);
            Vec3 color = hsvToRgb(p.colorHue, 0.6f, 0.55f);
            writeInstance(instanceScratch_, model, color, p.swayPhase, p.swayAmplitude, p.swaySpeed);
            ++plantCount;
        }
    }
    if (plantCount > 0) {
        uploadInstances(plantMesh_, instanceScratch_.data(), instanceScratch_.size() * sizeof(float));
        drawInstanced(plantMesh_, plantCount);
    }
    glEnable(GL_CULL_FACE);

    // --- Transparent pass: glass walls, drawn last, blended over everything
    // above without occluding it (depth test stays on so walls still hide
    // correctly behind each other and the floor, but depth WRITE is off so
    // a wall never blocks something behind it that hasn't drawn yet). ---
    shader_.use();
    glUniform1f(shader_.uniformLocation("uAlpha"), 0.22f);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glDepthMask(GL_FALSE);

    instanceScratch_.clear();
    writeInstance(instanceScratch_, Mat4::identity(), Vec3(0.55f, 0.75f, 0.85f)); // pale glass-blue
    uploadInstances(wallsMesh_, instanceScratch_.data(), instanceScratch_.size() * sizeof(float));
    drawInstanced(wallsMesh_, 1);

    glDepthMask(GL_TRUE);
    glDisable(GL_BLEND);
}

Renderer::~Renderer() {
    auto destroy = [](Mesh& m) {
        if (m.vbo) glDeleteBuffers(1, &m.vbo);
        if (m.instanceVbo) glDeleteBuffers(1, &m.instanceVbo);
        if (m.vao) glDeleteVertexArrays(1, &m.vao);
    };
    destroy(fishMesh_);
    destroy(foodMesh_);
    destroy(floorMesh_);
    destroy(wallsMesh_);
    destroy(plantMesh_);
}

} // namespace ft
