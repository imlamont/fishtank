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

    // ndcX, ndcY: click/tap position in normalized device coords, x/y in
    // [-1, 1], y-up (i.e. (-1,-1) is bottom-left, (1,1) is top-right — NOT
    // raw pixel coordinates, and NOT top-left-origin screen space). Projects
    // through the camera used for the most recent frame() call so the food
    // lands where it visually looks like the user clicked.
    void feedAtScreen(float ndcX, float ndcY);

    // dx, dy: drag delta as a fraction of canvas width/height (e.g. dragging
    // all the way across the canvas is dx = ±1.0) — resolution-independent,
    // same convention as the NDC coords above but as a delta, not a
    // position. Converts to an angle here (not in Renderer) since "how far
    // a drag rotates the view" is an interaction/feel decision, not a
    // camera-geometry one.
    void orbit(float dx, float dy);

    void setFishCount(int count);

private:
    Boids sim_{120, Vec3(6.0f, 3.2f, 3.0f)};
    Renderer renderer_;
    float elapsed_ = 0.0f;
};

} // namespace ft
