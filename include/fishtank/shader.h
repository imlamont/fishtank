#pragma once
#include "gl_compat.h"

namespace ft {

// Compiles a vertex+fragment GLSL pair, prefixing the right `#version` line
// (and a precision qualifier on GLES) so one shared shader body compiles
// under both desktop GL 3.3 core and WebGL2/GLES3.
class Shader {
public:
    bool compile(const char* vertSrc, const char* fragSrc);
    void use() const;
    GLint uniformLocation(const char* name) const;
    GLuint id() const { return program_; }
    ~Shader();

private:
    GLuint program_ = 0;
};

} // namespace ft
