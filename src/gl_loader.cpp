#include "fishtank/gl_loader.h"
#include <GLFW/glfw3.h>
#include <cstdio>

namespace ft::gl {

Functions g{};

namespace {
template <typename Fn>
bool load(Fn& slot, const char* name) {
    slot = reinterpret_cast<Fn>(glfwGetProcAddress(name));
    if (!slot) std::fprintf(stderr, "[fishtank] failed to load GL function: %s\n", name);
    return slot != nullptr;
}
} // namespace

bool loadFunctions() {
    bool ok = true;
    ok &= load(g.CreateShader, "glCreateShader");
    ok &= load(g.ShaderSource, "glShaderSource");
    ok &= load(g.CompileShader, "glCompileShader");
    ok &= load(g.GetShaderiv, "glGetShaderiv");
    ok &= load(g.GetShaderInfoLog, "glGetShaderInfoLog");
    ok &= load(g.DeleteShader, "glDeleteShader");
    ok &= load(g.CreateProgram, "glCreateProgram");
    ok &= load(g.AttachShader, "glAttachShader");
    ok &= load(g.LinkProgram, "glLinkProgram");
    ok &= load(g.GetProgramiv, "glGetProgramiv");
    ok &= load(g.GetProgramInfoLog, "glGetProgramInfoLog");
    ok &= load(g.DeleteProgram, "glDeleteProgram");
    ok &= load(g.UseProgram, "glUseProgram");
    ok &= load(g.GetUniformLocation, "glGetUniformLocation");
    ok &= load(g.UniformMatrix4fv, "glUniformMatrix4fv");
    ok &= load(g.Uniform1f, "glUniform1f");
    ok &= load(g.GenVertexArrays, "glGenVertexArrays");
    ok &= load(g.BindVertexArray, "glBindVertexArray");
    ok &= load(g.DeleteVertexArrays, "glDeleteVertexArrays");
    ok &= load(g.GenBuffers, "glGenBuffers");
    ok &= load(g.BindBuffer, "glBindBuffer");
    ok &= load(g.BufferData, "glBufferData");
    ok &= load(g.BufferSubData, "glBufferSubData");
    ok &= load(g.DeleteBuffers, "glDeleteBuffers");
    ok &= load(g.EnableVertexAttribArray, "glEnableVertexAttribArray");
    ok &= load(g.VertexAttribPointer, "glVertexAttribPointer");
    ok &= load(g.VertexAttribDivisor, "glVertexAttribDivisor");
    ok &= load(g.DrawArraysInstanced, "glDrawArraysInstanced");
    return ok;
}

} // namespace ft::gl
