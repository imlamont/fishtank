#include "fishtank/plants.h"
#include <algorithm>

namespace ft {

namespace {
// Same tiny xorshift32 approach as Boids — deterministic, no <random> needed.
struct Rng {
    unsigned state;
    explicit Rng(unsigned seed) : state(seed ? seed : 0x9e3779b9u) {}
    float next() {
        state ^= state << 13;
        state ^= state >> 17;
        state ^= state << 5;
        return (state & 0xFFFFFFu) / float(0xFFFFFFu);
    }
    float range(float lo, float hi) { return lo + next() * (hi - lo); }
};
} // namespace

Plants::Plants(int count, const Vec3& tankHalfExtents, unsigned seed) {
    if (count <= 0) return;
    instances_.reserve(count);
    Rng rng(seed);

    float floorY = -tankHalfExtents.y;
    int perCluster = 3 + (int)(rng.next() * 3.0f); // 3-5 blades per tuft
    int clusterCount = std::max(1, count / perCluster);

    int spawned = 0;
    for (int c = 0; c < clusterCount && spawned < count; ++c) {
        // Keep clusters off the walls so blades don't clip through glass.
        float margin = 0.85f;
        float clusterX = rng.range(-tankHalfExtents.x * margin, tankHalfExtents.x * margin);
        float clusterZ = rng.range(-tankHalfExtents.z * margin, tankHalfExtents.z * margin);

        int bladesHere = std::min(perCluster, count - spawned);
        for (int b = 0; b < bladesHere; ++b) {
            PlantInstance p;
            p.basePos = Vec3(clusterX + rng.range(-0.35f, 0.35f), floorY, clusterZ + rng.range(-0.35f, 0.35f));
            p.height = rng.range(0.5f, 1.6f) * (tankHalfExtents.y * 0.55f);
            p.colorHue = rng.range(0.28f, 0.42f); // green -> yellow-green range
            p.swayPhase = rng.range(0.0f, 6.2831853f);
            p.swayAmplitude = rng.range(0.06f, 0.16f);
            p.swaySpeed = rng.range(0.5f, 1.1f);
            instances_.push_back(p);
            ++spawned;
        }
    }
}

} // namespace ft
