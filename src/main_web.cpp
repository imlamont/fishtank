// Emscripten entry point + the C API the website's JS glue calls into.
//
// Boundary (deliberate): this module only simulates and renders. Feeding UI,
// the name/email log form, and any network calls live in the website's own
// JS/HTML — ft_feed_at() here just triggers the visual food-drop.
#include <emscripten.h>
#include <emscripten/html5.h>
#include "fishtank/app.h"

namespace {
ft::App g_app;
} // namespace

extern "C" {

// canvasSelector must name a <canvas> already present in the DOM, e.g.
// "#fishtank-canvas". Does NOT start its own loop — JS drives frames via
// requestAnimationFrame and calls ft_frame() each tick. (An earlier version
// used emscripten_set_main_loop() from here, but that throws a JS "unwind"
// exception to hand control back to the browser, which silently aborted the
// rest of the caller's JS right after this ccall — found by loading the
// page in a real browser, not by the C++ compiling cleanly.)
EMSCRIPTEN_KEEPALIVE
int ft_init(const char* canvasSelector, int widthPx, int heightPx) {
    EmscriptenWebGLContextAttributes attrs;
    emscripten_webgl_init_context_attributes(&attrs);
    attrs.alpha = false;
    attrs.depth = true;
    attrs.stencil = false;
    attrs.antialias = true;
    attrs.majorVersion = 2;
    attrs.minorVersion = 0;

    EMSCRIPTEN_WEBGL_CONTEXT_HANDLE ctx = emscripten_webgl_create_context(canvasSelector, &attrs);
    if (ctx <= 0) return 0;
    emscripten_webgl_make_context_current(ctx);

    return g_app.init(widthPx, heightPx) ? 1 : 0;
}

// dtSeconds: time since the previous ft_frame() call, in seconds. JS is
// expected to call this once per requestAnimationFrame tick.
EMSCRIPTEN_KEEPALIVE
void ft_frame(float dtSeconds) { g_app.frame(dtSeconds); }

EMSCRIPTEN_KEEPALIVE
void ft_resize(int widthPx, int heightPx) { g_app.resize(widthPx, heightPx); }

// nx, nz in [-1, 1]: horizontal drop position across the tank's width/depth.
EMSCRIPTEN_KEEPALIVE
void ft_feed_at(float nx, float nz) { g_app.feedAt(nx, nz); }

EMSCRIPTEN_KEEPALIVE
void ft_set_fish_count(int count) { g_app.setFishCount(count); }

} // extern "C"
