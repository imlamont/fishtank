#pragma once
// Picks the right GL headers per platform. Everything else in this codebase
// talks to plain GLES3/WebGL2-subset calls that both platforms provide.

#ifdef __EMSCRIPTEN__
    #include <GLES3/gl3.h>
    #include <emscripten.h>
    #include <emscripten/html5.h>
    #define FT_IS_GLES 1
#else
    #include <GLFW/glfw3.h>
    #include "gl_loader.h"
    #define FT_IS_GLES 0
#endif
