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

    // nx, nz in [-1, 1]: horizontal drop position across the tank's
    // width/depth. Spawns a small pinch of food flakes at the surface that
    // sink and get eaten (or expire).
    void feedAt(float nx, float nz);

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
