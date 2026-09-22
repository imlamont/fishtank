#include "fishtank/shader.h"
#include <cstdio>
#include <string>

namespace ft {

namespace {

GLuint compileStage(GLenum stage, const std::string& src) {
    GLuint s = glCreateShader(stage);
    const char* csrc = src.c_str();
    glShaderSource(s, 1, &csrc, nullptr);
    glCompileShader(s);
    GLint ok = GL_FALSE;
    glGetShaderiv(s, GL_COMPILE_STATUS, &ok);
    if (!ok) {
        char log[2048];
        GLsizei len = 0;
        glGetShaderInfoLog(s, sizeof(log), &len, log);
        std::fprintf(stderr, "[fishtank] shader compile error:\n%.*s\n", (int)len, log);
        glDeleteShader(s);
        return 0;
    }
    return s;
}

std::string withHeader(const char* body) {
#if FT_IS_GLES
    return std::string("#version 300 es\nprecision mediump float;\n") + body;
#else
    return std::string("#version 330 core\n") + body;
#endif
}

} // namespace

bool Shader::compile(const char* vertSrc, const char* fragSrc) {
    GLuint vs = compileStage(GL_VERTEX_SHADER, withHeader(vertSrc));
    if (!vs) return false;
    GLuint fs = compileStage(GL_FRAGMENT_SHADER, withHeader(fragSrc));
    if (!fs) { glDeleteShader(vs); return false; }

    program_ = glCreateProgram();
    glAttachShader(program_, vs);
    glAttachShader(program_, fs);
    glLinkProgram(program_);

    GLint ok = GL_FALSE;
    glGetProgramiv(program_, GL_LINK_STATUS, &ok);
    glDeleteShader(vs);
    glDeleteShader(fs);
    if (!ok) {
        char log[2048];
        GLsizei len = 0;
        glGetProgramInfoLog(program_, sizeof(log), &len, log);
        std::fprintf(stderr, "[fishtank] program link error:\n%.*s\n", (int)len, log);
        glDeleteProgram(program_);
        program_ = 0;
        return false;
    }
    return true;
}

void Shader::use() const { glUseProgram(program_); }

GLint Shader::uniformLocation(const char* name) const {
    return glGetUniformLocation(program_, name);
}

Shader::~Shader() {
    if (program_) glDeleteProgram(program_);
}

} // namespace ft
