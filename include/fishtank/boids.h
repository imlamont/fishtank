#pragma once
#include <vector>
#include "math3d.h"

namespace ft {

// Boid: the usual separation/alignment/cohesion flocking.
// Random: wanders in a slowly-drifting random heading, still keeping
//         separation from neighbors — no alignment/cohesion.
// Food: swims toward the nearest active food, still keeping separation —
//       no alignment/cohesion. Entered/left as a group: as long as any
//       food is active anywhere in the tank, every fish is in Food mode,
//       overriding whatever Boid/Random timer it was mid-way through; once
//       food runs out, each fish rolls a fresh Boid-or-Random mode + timer.
enum class FishMode { Boid, Random, Food };

struct Fish {
    Vec3 pos;
    Vec3 vel;
    float colorHue = 0.0f; // 0..1, assigned once at spawn for visual variety

    FishMode mode = FishMode::Boid;
    float modeTimer = 0.0f;  // seconds left in Boid/Random before re-rolling; not counted down in Food mode
    Vec3 wanderDir{1, 0, 0}; // current heading target while in Random mode, drifts slowly each frame
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

    // The still-water base height (before the renderer's animated ripple).
    // Single source of truth shared by feedAt's spawn height, the water
    // mesh's rest position, and pickSurfacePoint's raymarch target.
    float waterSurfaceY() const { return halfExtents_.y * 0.92f; }

private:
    void spawnFish(Fish& f);
    void pickNewRoamingMode(Fish& f);

    std::vector<Fish> fish_;
    std::vector<Food> food_;
    Vec3 halfExtents_;
    unsigned rngState_;

    float randf(); // deterministic xorshift, no <random> dependency needed
};

} // namespace ft
