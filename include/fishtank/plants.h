#pragma once
#include <vector>
#include "math3d.h"

namespace ft {

struct PlantInstance {
    Vec3 basePos;      // on the tank floor
    float height;      // random scale
    float colorHue;    // greenish range
    float swayPhase;
    float swayAmplitude;
    float swaySpeed;
};

// Randomly scatters plant blades (in loose clusters, like real tank
// vegetation) across the tank floor at construction time. Purely static
// data — there's no per-frame simulation here, since the swaying itself is
// done entirely in the vertex shader (see Renderer's plant shader) driven
// by each instance's phase/amplitude/speed plus the current time.
class Plants {
public:
    Plants(int count, const Vec3& tankHalfExtents, unsigned seed);

    const std::vector<PlantInstance>& instances() const { return instances_; }

private:
    std::vector<PlantInstance> instances_;
};

} // namespace ft
