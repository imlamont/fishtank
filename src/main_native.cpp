// Native desktop test harness: a plain GLFW window running the same App
// used by the wasm build. Click to feed the fish, drag to orbit the camera
// around the tank, Esc or close the window to quit.
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <vector>
#include "fishtank/gl_compat.h"
#include "fishtank/app.h"

namespace {
// Dumps the current framebuffer to a PPM file — set FISHTANK_SCREENSHOT to a
// path to capture one frame (after letting the sim settle a moment) and
// exit. Useful for visually verifying rendering changes headlessly (e.g.
// under Xvfb) without needing a browser at all.
void dumpScreenshot(const char* path, int w, int h) {
    std::vector<unsigned char> pixels(w * h * 3);
    glReadPixels(0, 0, w, h, GL_RGB, GL_UNSIGNED_BYTE, pixels.data());
    FILE* f = std::fopen(path, "wb");
    if (!f) return;
    std::fprintf(f, "P6\n%d %d\n255\n", w, h);
    // glReadPixels rows are bottom-to-top; PPM wants top-to-bottom.
    for (int y = h - 1; y >= 0; --y) std::fwrite(&pixels[y * w * 3], 1, w * 3, f);
    std::fclose(f);
}
} // namespace

namespace {
ft::App g_app;
int g_width = 1024, g_height = 640;

bool g_dragging = false;
double g_lastX = 0, g_lastY = 0;
double g_pressX = 0, g_pressY = 0;
double g_dragDistPx = 0; // accumulated, to tell a click from a drag on release

// Below this, a press+release is treated as a click (feed), not a drag
// (orbit) — real mice/trackpads always jitter a pixel or two between
// button-down and button-up.
constexpr double kClickDragThresholdPx = 4.0;

void onFramebufferResize(GLFWwindow*, int w, int h) {
    g_width = w;
    g_height = h;
    g_app.resize(w, h);
}

void onCursorPos(GLFWwindow*, double x, double y) {
    if (!g_dragging) return;
    double dx = x - g_lastX, dy = y - g_lastY;
    g_dragDistPx += std::sqrt(dx * dx + dy * dy);
    g_app.orbit((float)(dx / g_width), (float)(dy / g_height));
    g_lastX = x;
    g_lastY = y;
}

void onMouseButton(GLFWwindow* window, int button, int action, int) {
    if (button != GLFW_MOUSE_BUTTON_LEFT) return;
    double mx, my;
    glfwGetCursorPos(window, &mx, &my);

    if (action == GLFW_PRESS) {
        g_dragging = true;
        g_dragDistPx = 0;
        g_lastX = g_pressX = mx;
        g_lastY = g_pressY = my;
        return;
    }

    // GLFW_RELEASE
    g_dragging = false;
    if (g_dragDistPx >= kClickDragThresholdPx) return; // was a drag, not a click

    // GLFW cursor coords are pixels from the top-left; NDC is [-1,1] with
    // +Y up, so the Y axis has to flip here or clicks land mirrored vertically.
    float ndcX = (float)(g_pressX / g_width) * 2.0f - 1.0f;
    float ndcY = 1.0f - (float)(g_pressY / g_height) * 2.0f;
    g_app.feedAtScreen(ndcX, ndcY);
}

void onKey(GLFWwindow* window, int key, int, int action, int) {
    if (key == GLFW_KEY_ESCAPE && action == GLFW_PRESS) glfwSetWindowShouldClose(window, GLFW_TRUE);
}
} // namespace

namespace {
void onGlfwError(int code, const char* desc) {
    std::fprintf(stderr, "[fishtank] GLFW error %d: %s\n", code, desc);
}
} // namespace

int main() {
    std::setvbuf(stdout, nullptr, _IOLBF, 0);
    glfwSetErrorCallback(onGlfwError);
    if (!glfwInit()) {
        std::fprintf(stderr, "[fishtank] glfwInit failed\n");
        return 1;
    }
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
#ifdef __APPLE__
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GLFW_TRUE);
#endif

    GLFWwindow* window = glfwCreateWindow(g_width, g_height, "fishtank (native test)", nullptr, nullptr);
    if (!window) {
        std::fprintf(stderr, "[fishtank] glfwCreateWindow failed\n");
        glfwTerminate();
        return 1;
    }
    glfwMakeContextCurrent(window);
    glfwSwapInterval(1);
    glfwSetFramebufferSizeCallback(window, onFramebufferResize);
    glfwSetMouseButtonCallback(window, onMouseButton);
    glfwSetCursorPosCallback(window, onCursorPos);
    glfwSetKeyCallback(window, onKey);

    if (!ft::gl::loadFunctions()) {
        std::fprintf(stderr, "[fishtank] failed to load required GL functions\n");
        glfwTerminate();
        return 1;
    }
    std::printf("[fishtank] GL_VERSION: %s\n", glGetString(GL_VERSION));

    if (!g_app.init(g_width, g_height)) {
        std::fprintf(stderr, "[fishtank] App::init failed\n");
        glfwTerminate();
        return 1;
    }

    std::printf("[fishtank] running — click to feed, drag to orbit, Esc to quit\n");

    double lastTime = glfwGetTime();
    while (!glfwWindowShouldClose(window)) {
        double now = glfwGetTime();
        float dt = (float)(now - lastTime);
        lastTime = now;

        g_app.frame(dt);

        const char* shotPath = std::getenv("FISHTANK_SCREENSHOT");
        if (shotPath && now > 1.0) { // let a frame or two of sim settle first
            dumpScreenshot(shotPath, g_width, g_height);
            break;
        }

        glfwSwapBuffers(window);
        glfwPollEvents();
    }

    glfwDestroyWindow(window);
    glfwTerminate();
    return 0;
}
