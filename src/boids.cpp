#include "fishtank/boids.h"
#include <algorithm>
#include <cmath>

namespace ft {

namespace {
constexpr float kMaxSpeed = 1.6f;
constexpr float kMinSpeed = 0.5f;
constexpr float kMaxForce = 2.2f;
constexpr float kNeighborRadius = 1.6f;
constexpr float kSeparationRadius = 0.55f;
constexpr float kFoodEatRadius = 0.35f;
constexpr float kFoodLifetime = 14.0f;
constexpr float kBoundaryMargin = 1.2f;
constexpr float kMinModeSeconds = 1.0f;
constexpr float kMaxModeSeconds = 10.0f;
} // namespace

Boids::Boids(int fishCount, const Vec3& tankHalfExtents)
    : halfExtents_(tankHalfExtents), rngState_(0x9e3779b9u) {
    fish_.resize(std::max(0, fishCount));
    for (auto& f : fish_) spawnFish(f);
    food_.reserve(64);
}

float Boids::randf() {
    // xorshift32 — deterministic, dependency-free, plenty random enough for flocking jitter.
    rngState_ ^= rngState_ << 13;
    rngState_ ^= rngState_ >> 17;
    rngState_ ^= rngState_ << 5;
    return (rngState_ & 0xFFFFFFu) / float(0xFFFFFFu);
}

void Boids::spawnFish(Fish& f) {
    f.pos = Vec3((randf() * 2 - 1) * halfExtents_.x * 0.7f,
                 (randf() * 2 - 1) * halfExtents_.y * 0.7f,
                 (randf() * 2 - 1) * halfExtents_.z * 0.7f);
    float ang = randf() * 6.2831853f;
    f.vel = Vec3(std::cos(ang), (randf() - 0.5f) * 0.3f, std::sin(ang)) * kMinSpeed;
    f.colorHue = randf();

    f.mode = randf() < 0.5f ? FishMode::Boid : FishMode::Random;
    f.modeTimer = kMinModeSeconds + randf() * (kMaxModeSeconds - kMinModeSeconds);
    float wanderAng = randf() * 6.2831853f;
    f.wanderDir = Vec3(std::cos(wanderAng), (randf() - 0.5f) * 0.5f, std::sin(wanderAng)).normalized();
}

// Rolls a fresh Boid-or-Random mode + 1-10s timer — used both for the
// periodic re-roll and for the moment a fish leaves Food mode once food
// runs out. Never picks Food: that mode is only ever entered via the
// global food-active check in update(), not this per-fish roll.
void Boids::pickNewRoamingMode(Fish& f) {
    f.mode = randf() < 0.5f ? FishMode::Boid : FishMode::Random;
    f.modeTimer = kMinModeSeconds + randf() * (kMaxModeSeconds - kMinModeSeconds);
    if (f.mode == FishMode::Random) {
        float ang = randf() * 6.2831853f;
        f.wanderDir = Vec3(std::cos(ang), (randf() - 0.5f) * 0.5f, std::sin(ang)).normalized();
    }
}

void Boids::setFishCount(int count) {
    count = std::max(0, count);
    if ((int)fish_.size() == count) return;
    if ((int)fish_.size() > count) {
        fish_.resize(count);
        return;
    }
    size_t oldSize = fish_.size();
    fish_.resize(count);
    for (size_t i = oldSize; i < fish_.size(); ++i) spawnFish(fish_[i]);
}

void Boids::feedAt(float worldX, float worldZ) {
    float margin = 0.9f; // keep the pinch's random jitter from spawning outside the walls
    worldX = std::max(-halfExtents_.x * margin, std::min(halfExtents_.x * margin, worldX));
    worldZ = std::max(-halfExtents_.z * margin, std::min(halfExtents_.z * margin, worldZ));
    int pellets = 4 + (int)(randf() * 3); // a small pinch, not one lonely flake
    for (int i = 0; i < pellets; ++i) {
        Food food;
        food.pos = Vec3(worldX + (randf() - 0.5f) * 0.5f,
                         waterSurfaceY(),
                         worldZ + (randf() - 0.5f) * 0.5f);
        food.vel = Vec3((randf() - 0.5f) * 0.1f, -0.15f - randf() * 0.1f, (randf() - 0.5f) * 0.1f);
        food.life = kFoodLifetime;
        food.active = true;

        // Reuse a dead slot if one exists, otherwise grow.
        bool placed = false;
        for (auto& slot : food_) {
            if (!slot.active) { slot = food; placed = true; break; }
        }
        if (!placed) food_.push_back(food);
    }
}

void Boids::update(float dt) {
    if (dt <= 0.0f) return;
    dt = std::min(dt, 0.05f); // clamp so a tab-switch hiccup doesn't blow up the sim

    // --- food: sink, age out, clamp to tank floor ---
    for (auto& food : food_) {
        if (!food.active) continue;
        food.pos += food.vel * dt;
        food.life -= dt;
        float floorY = -halfExtents_.y * 0.95f;
        if (food.pos.y < floorY) {
            food.pos.y = floorY;
            food.vel.y = 0;
        }
        if (food.life <= 0.0f) food.active = false;
    }

    // --- mode state machine: Food overrides everything else as a group,
    // for as long as any food is active; otherwise each fish counts down
    // its own Boid/Random timer and re-rolls when it hits zero. ---
    bool anyFoodActive = false;
    for (const auto& food : food_) {
        if (food.active) { anyFoodActive = true; break; }
    }
    for (auto& f : fish_) {
        if (anyFoodActive) {
            f.mode = FishMode::Food; // timer is paused, not consumed, while in Food mode
        } else if (f.mode == FishMode::Food) {
            pickNewRoamingMode(f); // food just ran out — pick what's next
        } else {
            f.modeTimer -= dt;
            if (f.modeTimer <= 0.0f) pickNewRoamingMode(f);
        }
    }

    // --- fish: separation/alignment/cohesion (Boid), wander (Random), or
    // food-seeking (Food) — separation applies in every mode, alignment/
    // cohesion only in Boid, food-seeking only in Food, wander only in
    // Random. Boundary avoidance always applies regardless of mode. ---
    const size_t n = fish_.size();
    std::vector<Vec3> newVel(n);

    for (size_t i = 0; i < n; ++i) {
        Fish& self = fish_[i];
        Vec3 separation, alignment, cohesion;
        int neighbors = 0;

        for (size_t j = 0; j < n; ++j) {
            if (i == j) continue;
            const Fish& other = fish_[j];
            Vec3 diff = self.pos - other.pos;
            float dist = diff.length();
            if (dist > kNeighborRadius || dist < 1e-5f) continue;

            if (dist < kSeparationRadius) separation += diff * (1.0f / dist);
            alignment += other.vel;
            cohesion += other.pos;
            ++neighbors;
        }

        Vec3 accel;
        if (neighbors > 0) {
            accel += separation.normalized() * kMaxForce * 1.4f;
            if (self.mode == FishMode::Boid) {
                alignment = alignment * (1.0f / neighbors);
                cohesion = (cohesion * (1.0f / neighbors)) - self.pos;
                accel += (alignment.normalized() * kMaxSpeed - self.vel).normalized() * kMaxForce * 0.8f;
                accel += cohesion.normalized() * kMaxForce * 0.6f;
            }
        }

        if (self.mode == FishMode::Food) {
            // Seek the nearest active food — no range cap: being in Food
            // mode at all already means food exists somewhere, so head
            // for it regardless of distance.
            float bestDist = 1e6f;
            const Food* bestFood = nullptr;
            for (const auto& food : food_) {
                if (!food.active) continue;
                float d = (food.pos - self.pos).length();
                if (d < bestDist) { bestDist = d; bestFood = &food; }
            }
            if (bestFood) {
                Vec3 toFood = (bestFood->pos - self.pos);
                float d = toFood.length();
                if (d < kFoodEatRadius) {
                    const_cast<Food*>(bestFood)->active = false; // eaten
                } else {
                    accel += toFood.normalized() * kMaxForce * 1.6f;
                }
            }
        } else if (self.mode == FishMode::Random) {
            // Slowly drift the wander heading for organic movement, then
            // steer toward it — same steering formula Boid mode uses for
            // alignment, just aimed at a personal random heading instead
            // of the neighborhood's average velocity.
            float driftAngle = (randf() - 0.5f) * 1.5f * dt;
            float cosA = std::cos(driftAngle), sinA = std::sin(driftAngle);
            float wx = self.wanderDir.x * cosA - self.wanderDir.z * sinA;
            float wz = self.wanderDir.x * sinA + self.wanderDir.z * cosA;
            float wy = self.wanderDir.y + (randf() - 0.5f) * 0.4f * dt;
            wy = std::max(-0.6f, std::min(0.6f, wy));
            self.wanderDir = Vec3(wx, wy, wz).normalized();
            accel += (self.wanderDir * kMaxSpeed - self.vel).normalized() * kMaxForce * 0.9f;
        }

        // Soft boundary: steer back in once past the margin.
        Vec3 steerBack;
        auto boundAxis = [&](float pos, float half, float& out) {
            float limit = half - kBoundaryMargin;
            if (pos > limit) out -= (pos - limit) / kBoundaryMargin;
            else if (pos < -limit) out -= (pos + limit) / kBoundaryMargin;
        };
        boundAxis(self.pos.x, halfExtents_.x, steerBack.x);
        boundAxis(self.pos.y, halfExtents_.y, steerBack.y);
        boundAxis(self.pos.z, halfExtents_.z, steerBack.z);
        accel += steerBack * kMaxForce * 2.0f;

        Vec3 v = self.vel + accel * dt;
        float speed = v.length();
        if (speed > kMaxSpeed) v *= (kMaxSpeed / speed);
        else if (speed < kMinSpeed && speed > 1e-5f) v *= (kMinSpeed / speed);
        newVel[i] = v;
    }

    for (size_t i = 0; i < n; ++i) {
        fish_[i].vel = newVel[i];
        fish_[i].pos += fish_[i].vel * dt;
    }
}

} // namespace ft
