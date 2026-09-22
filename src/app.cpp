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

void App::setFishCount(int count) { sim_.setFishCount(count); }

} // namespace ft
