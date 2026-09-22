#pragma once
#include <vector>
#include "math3d.h"

namespace ft {

struct Fish {
    Vec3 pos;
    Vec3 vel;
    float colorHue = 0.0f; // 0..1, assigned once at spawn for visual variety
};

struct Food {
    Vec3 pos;
    Vec3 vel;
    float life = 0.0f;   // seconds remaining before it despawns uneaten
    bool active = false;
};

// A schooling-fish simulation confined to a box "tank". Pure CPU state, no
// rendering — Renderer reads the public accessors each frame.
class Boids {
public:
    explicit Boids(int fishCount, const Vec3& tankHalfExtents);

    void update(float dtSeconds);

    // worldX, worldZ: world-space horizontal drop position (clamped to the
    // tank). Spawns a small pinch of food flakes at the surface, above
    // (worldX, worldZ), that sink and get eaten (or expire). Callers
    // resolving a screen click should project through the camera first
    // (see Renderer::pickSurfacePoint) rather than passing screen
    // coordinates here directly.
    void feedAt(float worldX, float worldZ);

    void setFishCount(int count);

    const std::vector<Fish>& fish() const { return fish_; }
    const std::vector<Food>& food() const { return food_; }
    const Vec3& tankHalfExtents() const { return halfExtents_; }

private:
    void spawnFish(Fish& f);

    std::vector<Fish> fish_;
    std::vector<Food> food_;
    Vec3 halfExtents_;
    unsigned rngState_;

    float randf(); // deterministic xorshift, no <random> dependency needed
};

} // namespace ft
