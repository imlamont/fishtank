#pragma once
// A minimal, self-contained loader for the handful of post-GL-1.1 functions
// this project uses. Avoids depending on GLEW/glad — this is the whole list
// the renderer and shader code call, loaded once via glfwGetProcAddress.
//
// Not used under Emscripten: there, <GLES3/gl3.h> provides these as real
// linked symbols already.
#include <GL/gl.h>
#include <cstddef>

typedef char GLchar;
typedef ptrdiff_t GLintptr;
typedef ptrdiff_t GLsizeiptr;

#define GL_ARRAY_BUFFER 0x8892
#define GL_STATIC_DRAW 0x88E4
#define GL_DYNAMIC_DRAW 0x88E8
#define GL_VERTEX_SHADER 0x8B31
#define GL_FRAGMENT_SHADER 0x8B30
#define GL_COMPILE_STATUS 0x8B81
#define GL_LINK_STATUS 0x8B82

namespace ft::gl {

typedef GLuint (*PFN_glCreateShader)(GLenum);
typedef void (*PFN_glShaderSource)(GLuint, GLsizei, const GLchar* const*, const GLint*);
typedef void (*PFN_glCompileShader)(GLuint);
typedef void (*PFN_glGetShaderiv)(GLuint, GLenum, GLint*);
typedef void (*PFN_glGetShaderInfoLog)(GLuint, GLsizei, GLsizei*, GLchar*);
typedef void (*PFN_glDeleteShader)(GLuint);
typedef GLuint (*PFN_glCreateProgram)();
typedef void (*PFN_glAttachShader)(GLuint, GLuint);
typedef void (*PFN_glLinkProgram)(GLuint);
typedef void (*PFN_glGetProgramiv)(GLuint, GLenum, GLint*);
typedef void (*PFN_glGetProgramInfoLog)(GLuint, GLsizei, GLsizei*, GLchar*);
typedef void (*PFN_glDeleteProgram)(GLuint);
typedef void (*PFN_glUseProgram)(GLuint);
typedef GLint (*PFN_glGetUniformLocation)(GLuint, const GLchar*);
typedef void (*PFN_glUniformMatrix4fv)(GLint, GLsizei, GLboolean, const GLfloat*);
typedef void (*PFN_glUniform1f)(GLint, GLfloat);

typedef void (*PFN_glGenVertexArrays)(GLsizei, GLuint*);
typedef void (*PFN_glBindVertexArray)(GLuint);
typedef void (*PFN_glDeleteVertexArrays)(GLsizei, const GLuint*);
typedef void (*PFN_glGenBuffers)(GLsizei, GLuint*);
typedef void (*PFN_glBindBuffer)(GLenum, GLuint);
typedef void (*PFN_glBufferData)(GLenum, GLsizeiptr, const void*, GLenum);
typedef void (*PFN_glBufferSubData)(GLenum, GLintptr, GLsizeiptr, const void*);
typedef void (*PFN_glDeleteBuffers)(GLsizei, const GLuint*);
typedef void (*PFN_glEnableVertexAttribArray)(GLuint);
typedef void (*PFN_glVertexAttribPointer)(GLuint, GLint, GLenum, GLboolean, GLsizei, const void*);
typedef void (*PFN_glVertexAttribDivisor)(GLuint, GLuint);
typedef void (*PFN_glDrawArraysInstanced)(GLenum, GLint, GLsizei, GLsizei);

struct Functions {
    PFN_glCreateShader CreateShader;
    PFN_glShaderSource ShaderSource;
    PFN_glCompileShader CompileShader;
    PFN_glGetShaderiv GetShaderiv;
    PFN_glGetShaderInfoLog GetShaderInfoLog;
    PFN_glDeleteShader DeleteShader;
    PFN_glCreateProgram CreateProgram;
    PFN_glAttachShader AttachShader;
    PFN_glLinkProgram LinkProgram;
    PFN_glGetProgramiv GetProgramiv;
    PFN_glGetProgramInfoLog GetProgramInfoLog;
    PFN_glDeleteProgram DeleteProgram;
    PFN_glUseProgram UseProgram;
    PFN_glGetUniformLocation GetUniformLocation;
    PFN_glUniformMatrix4fv UniformMatrix4fv;
    PFN_glUniform1f Uniform1f;
    PFN_glGenVertexArrays GenVertexArrays;
    PFN_glBindVertexArray BindVertexArray;
    PFN_glDeleteVertexArrays DeleteVertexArrays;
    PFN_glGenBuffers GenBuffers;
    PFN_glBindBuffer BindBuffer;
    PFN_glBufferData BufferData;
    PFN_glBufferSubData BufferSubData;
    PFN_glDeleteBuffers DeleteBuffers;
    PFN_glEnableVertexAttribArray EnableVertexAttribArray;
    PFN_glVertexAttribPointer VertexAttribPointer;
    PFN_glVertexAttribDivisor VertexAttribDivisor;
    PFN_glDrawArraysInstanced DrawArraysInstanced;
};

extern Functions g;

// Loads every entry above via glfwGetProcAddress. Returns false (and logs
// which symbol failed) if the driver doesn't provide one of them.
bool loadFunctions();

} // namespace ft::gl

inline GLuint glCreateShader(GLenum type) { return ft::gl::g.CreateShader(type); }
inline void glShaderSource(GLuint s, GLsizei n, const GLchar* const* src, const GLint* len) { ft::gl::g.ShaderSource(s, n, src, len); }
inline void glCompileShader(GLuint s) { ft::gl::g.CompileShader(s); }
inline void glGetShaderiv(GLuint s, GLenum pname, GLint* p) { ft::gl::g.GetShaderiv(s, pname, p); }
inline void glGetShaderInfoLog(GLuint s, GLsizei bufSize, GLsizei* len, GLchar* log) { ft::gl::g.GetShaderInfoLog(s, bufSize, len, log); }
inline void glDeleteShader(GLuint s) { ft::gl::g.DeleteShader(s); }
inline GLuint glCreateProgram() { return ft::gl::g.CreateProgram(); }
inline void glAttachShader(GLuint p, GLuint s) { ft::gl::g.AttachShader(p, s); }
inline void glLinkProgram(GLuint p) { ft::gl::g.LinkProgram(p); }
inline void glGetProgramiv(GLuint p, GLenum pname, GLint* v) { ft::gl::g.GetProgramiv(p, pname, v); }
inline void glGetProgramInfoLog(GLuint p, GLsizei bufSize, GLsizei* len, GLchar* log) { ft::gl::g.GetProgramInfoLog(p, bufSize, len, log); }
inline void glDeleteProgram(GLuint p) { ft::gl::g.DeleteProgram(p); }
inline void glUseProgram(GLuint p) { ft::gl::g.UseProgram(p); }
inline GLint glGetUniformLocation(GLuint p, const GLchar* name) { return ft::gl::g.GetUniformLocation(p, name); }
inline void glUniformMatrix4fv(GLint loc, GLsizei count, GLboolean transpose, const GLfloat* v) { ft::gl::g.UniformMatrix4fv(loc, count, transpose, v); }
inline void glUniform1f(GLint loc, GLfloat v) { ft::gl::g.Uniform1f(loc, v); }
inline void glGenVertexArrays(GLsizei n, GLuint* arrays) { ft::gl::g.GenVertexArrays(n, arrays); }
inline void glBindVertexArray(GLuint a) { ft::gl::g.BindVertexArray(a); }
inline void glDeleteVertexArrays(GLsizei n, const GLuint* arrays) { ft::gl::g.DeleteVertexArrays(n, arrays); }
inline void glGenBuffers(GLsizei n, GLuint* buffers) { ft::gl::g.GenBuffers(n, buffers); }
inline void glBindBuffer(GLenum target, GLuint buffer) { ft::gl::g.BindBuffer(target, buffer); }
inline void glBufferData(GLenum target, GLsizeiptr size, const void* data, GLenum usage) { ft::gl::g.BufferData(target, size, data, usage); }
inline void glBufferSubData(GLenum target, GLintptr offset, GLsizeiptr size, const void* data) { ft::gl::g.BufferSubData(target, offset, size, data); }
inline void glDeleteBuffers(GLsizei n, const GLuint* buffers) { ft::gl::g.DeleteBuffers(n, buffers); }
inline void glEnableVertexAttribArray(GLuint index) { ft::gl::g.EnableVertexAttribArray(index); }
inline void glVertexAttribPointer(GLuint index, GLint size, GLenum type, GLboolean normalized, GLsizei stride, const void* ptr) { ft::gl::g.VertexAttribPointer(index, size, type, normalized, stride, ptr); }
inline void glVertexAttribDivisor(GLuint index, GLuint divisor) { ft::gl::g.VertexAttribDivisor(index, divisor); }
inline void glDrawArraysInstanced(GLenum mode, GLint first, GLsizei count, GLsizei instancecount) { ft::gl::g.DrawArraysInstanced(mode, first, count, instancecount); }
