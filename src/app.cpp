#include "fishtank/app.h"

namespace ft {

bool App::init(int widthPx, int heightPx) {
    if (!renderer_.init(sim_)) return false;
    resize(widthPx, heightPx);
    return true;
}

void App::resize(int widthPx, int heightPx) { renderer_.resize(widthPx, heightPx); }

void App::frame(float dtSeconds) {
    elapsed_ += dtSeconds;
    sim_.update(dtSeconds);
    renderer_.render(sim_, elapsed_);
}

void App::feedAtScreen(float ndcX, float ndcY) {
    Vec3 surface = renderer_.pickSurfacePoint(ndcX, ndcY, sim_, elapsed_);
    sim_.feedAt(surface.x, surface.z);
}

void App::orbit(float dx, float dy) {
    const float yawPerFullDrag = 3.6f;   // ~1.15 full turns dragging all the way across
    const float pitchPerFullDrag = 2.4f; // radians, well within Renderer's own clamp
    renderer_.addOrbitDelta(dx * yawPerFullDrag, dy * pitchPerFullDrag);
}

void App::setFishCount(int count) { sim_.setFishCount(count); }

} // namespace ft
