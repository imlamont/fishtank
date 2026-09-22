// Native desktop test harness: a plain GLFW window running the same App
// used by the wasm build. Click anywhere in the window to feed the fish;
// press Escape or close the window to quit.
#include <cstdio>
#include "fishtank/gl_compat.h"
#include "fishtank/app.h"

namespace {
ft::App g_app;
int g_width = 1024, g_height = 640;

void onFramebufferResize(GLFWwindow*, int w, int h) {
    g_width = w;
    g_height = h;
    g_app.resize(w, h);
}

void onMouseButton(GLFWwindow* window, int button, int action, int) {
    if (button != GLFW_MOUSE_BUTTON_LEFT || action != GLFW_PRESS) return;
    double mx, my;
    glfwGetCursorPos(window, &mx, &my);
    // GLFW cursor coords are pixels from the top-left; NDC is [-1,1] with
    // +Y up, so the Y axis has to flip here or clicks land mirrored vertically.
    float ndcX = (float)(mx / g_width) * 2.0f - 1.0f;
    float ndcY = 1.0f - (float)(my / g_height) * 2.0f;
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

    std::printf("[fishtank] running — click to feed, Esc to quit\n");

    double lastTime = glfwGetTime();
    while (!glfwWindowShouldClose(window)) {
        double now = glfwGetTime();
        float dt = (float)(now - lastTime);
        lastTime = now;

        g_app.frame(dt);

        glfwSwapBuffers(window);
        glfwPollEvents();
    }

    glfwDestroyWindow(window);
    glfwTerminate();
    return 0;
}
