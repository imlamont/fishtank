#include "fishtank/renderer.h"
#include "fishtank/fish_mesh.h"
#include "fishtank/environment_mesh.h"
#include <cmath>
#include <cstddef>
#include <string>

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

// The water's shared ripple formula — this exact formula also lives in
// kWaterVertSrc/kWallVertSrc as GLSL. The two MUST stay in sync (see
// CLAUDE.md): this copy is what pickSurfacePoint raymarches against, the
// GLSL copy is what's actually drawn, and picking silently drifting from
// what's visually on screen would be a much worse bug than either copy
// alone. Small, smooth, dependency-free — a couple of overlapping sines
// rather than anything physically simulated.
float waterHeight(float x, float z, float t) {
    return 0.05f * std::sin(x * 0.8f + t * 1.3f) * std::sin(z * 0.7f + t * 0.9f + 1.0f) +
           0.025f * std::sin(x * 1.7f - t * 2.1f + z * 0.5f);
}

// The GLSL twin of the C++ waterHeight() above — must stay numerically
// identical (see the comment on that function). Renderer::init() prepends
// this to kWallVertSrc/kWaterVertSrc before compiling them (string
// concatenation, not a preprocessor include — GLSL has no #include), which
// is why those two don't define waterHeight() themselves despite calling it.
const char* kWaterHeightGLSL = R"GLSL(
float waterHeight(float x, float z, float t) {
    return 0.05 * sin(x * 0.8 + t * 1.3) * sin(z * 0.7 + t * 0.9 + 1.0) +
           0.025 * sin(x * 1.7 - t * 2.1 + z * 0.5);
}
)GLSL";

// Walls are drawn already in world space (model is identity — see render()),
// so aPos IS worldPos here. Unlike the other shaders, color AND alpha are
// computed in the fragment stage (kWallFragSrc) from the interpolated
// world position, not per-vertex here — the wall mesh is just 2 triangles
// (4 corners) per face, so a per-vertex waterline/floor-line calculation
// would linearly interpolate across the *entire* wall height instead of
// forming the intended sharp band; per-fragment interpolation of a planar
// quad's world position is exact regardless of how few vertices it has.
const char* kWallVertSrc = R"GLSL(
layout(location = 0) in vec3 aPos;
layout(location = 1) in vec3 aNormal;
layout(location = 2) in vec4 aModelCol0;
layout(location = 3) in vec4 aModelCol1;
layout(location = 4) in vec4 aModelCol2;
layout(location = 5) in vec4 aModelCol3;
layout(location = 6) in vec3 aColor;

uniform mat4 uViewProj;

out vec3 vNormal;
out vec3 vWorldPos;

void main() {
    mat4 model = mat4(aModelCol0, aModelCol1, aModelCol2, aModelCol3);
    vec4 worldPos = model * vec4(aPos, 1.0);
    gl_Position = uViewProj * worldPos;
    vNormal = mat3(model) * aNormal;
    vWorldPos = worldPos.xyz;
}
)GLSL";

// Companion fragment shader for the walls: white above the (animated)
// waterline, blue below it — same as before — but now also fading to
// fully opaque near/below the floor, so the sand's edge doesn't show
// through the glass at the bottom of the tank the way a uniformly
// translucent wall would.
const char* kWallFragSrc = R"GLSL(
in vec3 vNormal;
in vec3 vWorldPos;
out vec4 fragColor;

uniform float uAlpha;
uniform float uTime;
uniform float uWaterBaseY;
uniform float uFloorY;

void main() {
    vec3 n = normalize(vNormal);
    vec3 lightDir = normalize(vec3(0.4, 0.9, 0.3));
    float diff = max(dot(n, lightDir), 0.0);

    float waterY = uWaterBaseY + waterHeight(vWorldPos.x, vWorldPos.z, uTime);
    float aboveWater = smoothstep(waterY - 0.15, waterY + 0.15, vWorldPos.y);
    vec3 blueColor = vec3(0.42, 0.62, 0.82);
    vec3 whiteColor = vec3(0.93, 0.95, 0.97);
    vec3 baseColor = mix(blueColor, whiteColor, aboveWater);
    vec3 color = baseColor * 0.35 + baseColor * diff * 0.85;

    // Opaque right at the floor, translucent (uAlpha) a bit above it —
    // hides the sand's edge instead of showing it through clear glass.
    float translucentFactor = smoothstep(uFloorY, uFloorY + 1.0, vWorldPos.y);
    float alpha = mix(1.0, uAlpha, translucentFactor);

    fragColor = vec4(color, alpha);
}
)GLSL";

// Water mesh is authored flat (local y = 0) and translated to the water's
// rest height per instance; the ripple is added here in world space, after
// the model transform, the same pattern kPlantVertSrc uses for sway.
const char* kWaterVertSrc = R"GLSL(
layout(location = 0) in vec3 aPos;
layout(location = 1) in vec3 aNormal;
layout(location = 2) in vec4 aModelCol0;
layout(location = 3) in vec4 aModelCol1;
layout(location = 4) in vec4 aModelCol2;
layout(location = 5) in vec4 aModelCol3;
layout(location = 6) in vec3 aColor;

uniform mat4 uViewProj;
uniform float uTime;

out vec3 vNormal;
out vec3 vColor;

void main() {
    mat4 model = mat4(aModelCol0, aModelCol1, aModelCol2, aModelCol3);
    vec4 basePos = model * vec4(aPos, 1.0);
    float h = waterHeight(basePos.x, basePos.z, uTime);
    vec3 worldPos = vec3(basePos.x, basePos.y + h, basePos.z);
    gl_Position = uViewProj * vec4(worldPos, 1.0);
    vNormal = aNormal; // approximate: ignores the ripple's slope, kept simple on purpose
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

    // See kWaterHeightGLSL's comment: GLSL has no #include, so this is
    // plain string concatenation done once at startup, not per frame.
    std::string wallFragSrc = std::string(kWaterHeightGLSL) + kWallFragSrc;
    std::string waterVertSrc = std::string(kWaterHeightGLSL) + kWaterVertSrc;
    if (!wallShader_.compile(kWallVertSrc, wallFragSrc.c_str())) return false;
    if (!waterShader_.compile(waterVertSrc.c_str(), kFragSrc)) return false;

    fishMesh_ = createMesh(buildFishMesh(), kMaxFishInstances);
    foodMesh_ = createMesh(buildOctahedronMesh(0.06f), kMaxFoodInstances);

    const Vec3& half = sim.tankHalfExtents();
    floorMesh_ = createMesh(buildFloorMesh(half, 10, 1234u), 1);
    wallsMesh_ = createMesh(buildWallsMesh(half), 1);
    plantMesh_ = createMesh(buildPlantBladeMesh(), kMaxPlantInstances, 3);
    waterMesh_ = createMesh(buildWaterMesh(half, 14), 1);
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
    // addOrbitDelta() clamps orbitPitch_ to keep this true even under user
    // dragging — see its clamp bounds for the actual safe range.
    //
    // Distance is derived from the tank size + current aspect ratio (not
    // hardcoded) so the whole tank stays in frame regardless of the embed's
    // width/height; the margin is generous partly for the click-plane tilt
    // and partly because an orbited (non-front-on) view can show more of
    // the tank's diagonal extent than a straight-on one.
    const float baseTilt = 0.9f; // ~52 degrees downward, before user orbit
    float halfFov = fovY * 0.5f;
    float distForWidth = half.x / (std::tan(halfFov) * aspect);
    float distForHeight = half.y / std::tan(halfFov);
    float dist = half.z + std::max(distForWidth, distForHeight) * 1.45f;

    float tilt = baseTilt + orbitPitch_;
    float eyeHeight = dist * std::sin(tilt);
    float eyeRadius = dist * std::cos(tilt); // horizontal distance from the tank's vertical axis
    Vec3 eye(eyeRadius * std::sin(orbitYaw_), eyeHeight, eyeRadius * std::cos(orbitYaw_));
    Vec3 center(0, 0, 0);
    Mat4 view = Mat4::lookAt(eye, center, Vec3(0, 1, 0));
    Mat4 proj = Mat4::perspective(fovY, aspect, 0.1f, 200.0f);
    return proj * view;
}

void Renderer::addOrbitDelta(float dYaw, float dPitch) {
    orbitYaw_ += dYaw; // unbounded — a full spin around the tank is fine

    // Keep tilt just barely off both extremes: exactly level (or worse,
    // looking slightly up) or exactly straight down would make the lookAt
    // basis degenerate (forward parallel to the world-up hint) and/or
    // stand pickSurfacePoint's ray-plane intersection on a knife's edge
    // (see computeViewProj's comment). Within that, the range is wide on
    // purpose — this is the user's actual vertical look range, not just a
    // safety margin.
    //
    // At the shallow end, pickSurfacePoint's click-to-world math does get
    // numerically shakier (a near-level ray barely changes height per unit
    // of depth, so the surface-plane intersection can extrapolate well
    // outside the tank) — but Boids::feedAt clamps the result to the tank
    // bounds regardless, so the visible failure mode is "food lands at the
    // nearest edge instead of exactly under the cursor" at extreme angles,
    // not a crash or a wildly-out-of-tank result. That's an acceptable
    // trade for actually letting the camera go near water-level.
    const float baseTilt = 0.9f;
    const float minTilt = 0.2f;  // ~11 degrees downward — near water-level
    const float maxTilt = 1.5f;  // ~86 degrees downward — near top-down
    float tilt = std::max(minTilt, std::min(maxTilt, baseTilt + orbitPitch_ + dPitch));
    orbitPitch_ = tilt - baseTilt;
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

    Vec3 fullRay = farWorld - nearWorld;
    float fullLen = fullRay.length();
    Vec3 dir = fullLen > 1e-6f ? fullRay * (1.0f / fullLen) : Vec3(0, -1, 0);

    // The near/far clip points sit at world-space depths of ~0.1 and ~200
    // (see computeViewProj's perspective near/far), so the full clip-to-clip
    // ray spans far more distance than the tank occupies. Marching that
    // whole span at fine-enough resolution to resolve the ripple (whose
    // amplitude is ~0.05 world units) would need a huge step count, so
    // instead: solve the flat-water-level analytically first just to find
    // *where* along the ray to march, then march a small bounded window
    // around that estimate against the real (rippled) surface. This is a
    // real raymarch, not the old direct analytic solve — it's just bounded
    // by an analytic estimate rather than searching the entire ray, purely
    // for efficiency.
    float baseY = sim.waterSurfaceY();
    float estimateT = std::fabs(dir.y) > 1e-6f ? (baseY - nearWorld.y) / dir.y : fullLen * 0.5f;
    estimateT = std::max(0.0f, std::min(fullLen, estimateT));

    auto heightDiff = [&](float t) {
        Vec3 p = nearWorld + dir * t;
        return p.y - (baseY + waterHeight(p.x, p.z, timeSeconds));
    };

    const float windowHalf = 2.5f; // world units — comfortably more than the ripple amplitude
    const int steps = 32;
    float t0 = std::max(0.0f, estimateT - windowHalf);
    float t1 = std::min(fullLen, estimateT + windowHalf);
    float step = (t1 - t0) / steps;

    float prevT = t0;
    float prevDiff = heightDiff(t0);
    float hitT = estimateT; // fallback: the flat-water estimate, if no crossing is found in-window
    for (int i = 1; i <= steps; ++i) {
        float t = t0 + step * i;
        float diff = heightDiff(t);
        if ((diff > 0.0f) != (prevDiff > 0.0f)) {
            float frac = prevDiff / (prevDiff - diff); // linear interpolation to the crossing
            hitT = prevT + (t - prevT) * frac;
            break;
        }
        prevT = t;
        prevDiff = diff;
    }

    Vec3 hit = nearWorld + dir * hitT;
    return Vec3(hit.x, baseY + waterHeight(hit.x, hit.z, timeSeconds), hit.z);
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

    // The body is a closed bipyramid (culling is fine there), but the
    // dorsal/pectoral/tail fins are flat single-sided triangles — with
    // culling on, a fin facing away from the camera at any given moment
    // (which happens constantly as fish turn) would simply vanish. Same
    // fix as the plants below: just don't cull this draw.
    glDisable(GL_CULL_FACE);
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
    glEnable(GL_CULL_FACE);

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

    // --- Transparent pass: glass walls + water surface, drawn last, blended
    // over everything above without occluding it (depth test stays on so
    // these still hide correctly behind each other and the floor, but depth
    // WRITE is off so neither blocks something behind it that hasn't drawn
    // yet). Culling is off for both: walls need their back face visible too
    // (seen from odd orbit angles, or simply the inside surface facing the
    // fish), and the water plane can end up viewed from underneath at the
    // wide pitch range the camera now supports. ---
    glDisable(GL_CULL_FACE);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glDepthMask(GL_FALSE);

    wallShader_.use();
    glUniformMatrix4fv(wallShader_.uniformLocation("uViewProj"), 1, GL_FALSE, viewProj.m);
    glUniform1f(wallShader_.uniformLocation("uAlpha"), 0.22f);
    glUniform1f(wallShader_.uniformLocation("uTime"), timeSeconds);
    glUniform1f(wallShader_.uniformLocation("uWaterBaseY"), sim.waterSurfaceY());
    glUniform1f(wallShader_.uniformLocation("uFloorY"), -sim.tankHalfExtents().y);

    instanceScratch_.clear();
    writeInstance(instanceScratch_, Mat4::identity(), Vec3(0, 0, 0)); // color is computed in-shader, not from this
    uploadInstances(wallsMesh_, instanceScratch_.data(), instanceScratch_.size() * sizeof(float));
    drawInstanced(wallsMesh_, 1);

    waterShader_.use();
    glUniformMatrix4fv(waterShader_.uniformLocation("uViewProj"), 1, GL_FALSE, viewProj.m);
    glUniform1f(waterShader_.uniformLocation("uAlpha"), 0.35f);
    glUniform1f(waterShader_.uniformLocation("uTime"), timeSeconds);

    instanceScratch_.clear();
    writeInstance(instanceScratch_, Mat4::translation(Vec3(0, sim.waterSurfaceY(), 0)), Vec3(0.35f, 0.65f, 0.85f));
    uploadInstances(waterMesh_, instanceScratch_.data(), instanceScratch_.size() * sizeof(float));
    drawInstanced(waterMesh_, 1);

    glDepthMask(GL_TRUE);
    glDisable(GL_BLEND);
    glEnable(GL_CULL_FACE);
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
    destroy(waterMesh_);
}

} // namespace ft
