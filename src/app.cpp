#include "fishtank/app.h"

namespace ft {

bool App::init(int widthPx, int heightPx) {
    if (!renderer_.init()) return false;
    resize(widthPx, heightPx);
    return true;
}

void App::resize(int widthPx, int heightPx) { renderer_.resize(widthPx, heightPx); }

void App::frame(float dtSeconds) {
    elapsed_ += dtSeconds;
    sim_.update(dtSeconds);
    renderer_.render(sim_, elapsed_);
}

void App::feedAt(float nx, float nz) { sim_.feedAt(nx, nz); }

void App::setFishCount(int count) { sim_.setFishCount(count); }

} // namespace ft
