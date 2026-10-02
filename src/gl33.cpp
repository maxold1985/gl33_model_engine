#include "gl33.h"
#include <cstdio>
#include <cstring>
#include <cstdint>
#include <string>

PFNGLGENVERTEXARRAYSPROC33 gl33GenVertexArrays = nullptr;
PFNGLBINDVERTEXARRAYPROC33 gl33BindVertexArray = nullptr;
PFNGLDELETEVERTEXARRAYSPROC33 gl33DeleteVertexArrays = nullptr;

PFNGLGENBUFFERSPROC33 gl33GenBuffers = nullptr;
PFNGLBINDBUFFERPROC33 gl33BindBuffer = nullptr;
PFNGLBUFFERDATAPROC33 gl33BufferData = nullptr;
PFNGLDELETEBUFFERSPROC33 gl33DeleteBuffers = nullptr;

PFNGLCREATESHADERPROC33 gl33CreateShader = nullptr;
PFNGLSHADERSOURCEPROC33 gl33ShaderSource = nullptr;
PFNGLCOMPILESHADERPROC33 gl33CompileShader = nullptr;
PFNGLGETSHADERIVPROC33 gl33GetShaderiv = nullptr;
PFNGLGETSHADERINFOLOGPROC33 gl33GetShaderInfoLog = nullptr;
PFNGLDELETESHADERPROC33 gl33DeleteShader = nullptr;

PFNGLCREATEPROGRAMPROC33 gl33CreateProgram = nullptr;
PFNGLATTACHSHADERPROC33 gl33AttachShader = nullptr;
PFNGLLINKPROGRAMPROC33 gl33LinkProgram = nullptr;
PFNGLGETPROGRAMIVPROC33 gl33GetProgramiv = nullptr;
PFNGLGETPROGRAMINFOLOGPROC33 gl33GetProgramInfoLog = nullptr;
PFNGLUSEPROGRAMPROC33 gl33UseProgram = nullptr;
PFNGLDELETEPROGRAMPROC33 gl33DeleteProgram = nullptr;

PFNGLVERTEXATTRIBPOINTERPROC33 gl33VertexAttribPointer = nullptr;
PFNGLENABLEVERTEXATTRIBARRAYPROC33 gl33EnableVertexAttribArray = nullptr;

PFNGLGETUNIFORMLOCATIONPROC33 gl33GetUniformLocation = nullptr;
PFNGLUNIFORMMATRIX4FVPROC33 gl33UniformMatrix4fv = nullptr;
PFNGLUNIFORM1IPROC33 gl33Uniform1i = nullptr;
PFNGLUNIFORM1FPROC33 gl33Uniform1f = nullptr;
PFNGLUNIFORM4FPROC33 gl33Uniform4f = nullptr;

PFNGLACTIVETEXTUREPROC33 gl33ActiveTexture = nullptr;
PFNGLGENERATEMIPMAPPROC33 gl33GenerateMipmap = nullptr;

static PROC get_proc(const char* name)
{
    PROC p = wglGetProcAddress(name);

    std::uintptr_t value = 0;
    static_assert(sizeof(p) == sizeof(value), "unexpected function pointer size");
    std::memcpy(&value, &p, sizeof(p));

    if (!p || value == 1 || value == 2 || value == 3 || value == (std::uintptr_t)-1) {
        HMODULE module = GetModuleHandleA("opengl32.dll");
        if (!module)
            module = LoadLibraryA("opengl32.dll");
        if (module)
            p = GetProcAddress(module, name);
    }

    return p;
}

template <typename T>
static bool load_one(T& out, const char* variable_name)
{
    const char* suffix = variable_name;

    if (std::strncmp(variable_name, "gl33", 4) == 0)
        suffix = variable_name + 4;

    const std::string api_name =
        std::string("gl") + suffix;

    PROC p = get_proc(api_name.c_str());

    if (!p) {
        std::fprintf(
            stderr,
            "OpenGL function missing: %s\n",
            api_name.c_str()
        );
        return false;
    }

    static_assert(sizeof(out) == sizeof(p), "unexpected pointer size");
    std::memcpy(&out, &p, sizeof(out));
    return true;
}

bool gl33_load()
{
#define GL33_LOAD(x) do { if (!load_one(x, #x)) return false; } while (0)

    GL33_LOAD(gl33GenVertexArrays);
    GL33_LOAD(gl33BindVertexArray);
    GL33_LOAD(gl33DeleteVertexArrays);

    GL33_LOAD(gl33GenBuffers);
    GL33_LOAD(gl33BindBuffer);
    GL33_LOAD(gl33BufferData);
    GL33_LOAD(gl33DeleteBuffers);

    GL33_LOAD(gl33CreateShader);
    GL33_LOAD(gl33ShaderSource);
    GL33_LOAD(gl33CompileShader);
    GL33_LOAD(gl33GetShaderiv);
    GL33_LOAD(gl33GetShaderInfoLog);
    GL33_LOAD(gl33DeleteShader);

    GL33_LOAD(gl33CreateProgram);
    GL33_LOAD(gl33AttachShader);
    GL33_LOAD(gl33LinkProgram);
    GL33_LOAD(gl33GetProgramiv);
    GL33_LOAD(gl33GetProgramInfoLog);
    GL33_LOAD(gl33UseProgram);
    GL33_LOAD(gl33DeleteProgram);

    GL33_LOAD(gl33VertexAttribPointer);
    GL33_LOAD(gl33EnableVertexAttribArray);

    GL33_LOAD(gl33GetUniformLocation);
    GL33_LOAD(gl33UniformMatrix4fv);
    GL33_LOAD(gl33Uniform1i);
    GL33_LOAD(gl33Uniform1f);
    GL33_LOAD(gl33Uniform4f);

    GL33_LOAD(gl33ActiveTexture);
    GL33_LOAD(gl33GenerateMipmap);

#undef GL33_LOAD
    return true;
}
