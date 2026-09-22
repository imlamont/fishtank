#pragma once
#include "boids.h"
#include "renderer.h"

namespace ft {

// Platform-independent glue: owns the simulation + renderer and drives one
// frame. main_native.cpp and main_web.cpp both just wrap this.
class App {
public:
    bool init(int widthPx, int heightPx);
    void resize(int widthPx, int heightPx);
    void frame(float dtSeconds);
    void feedAt(float nx, float nz);
    void setFishCount(int count);

private:
    Boids sim_{120, Vec3(6.0f, 3.2f, 3.0f)};
    Renderer renderer_;
    float elapsed_ = 0.0f;
};

} // namespace ft
