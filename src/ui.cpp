#include "ui.h"
#include "gl33.h"

#include <cstdio>
#include <array>
#include <cstdlib>
#include <map>
#include <string>

static GLuint compile_shader(
    GLenum type,
    const char* source
)
{
    GLuint shader = gl33CreateShader(type);

    gl33ShaderSource(shader, 1, &source, nullptr);
    gl33CompileShader(shader);

    GLint ok = 0;
    gl33GetShaderiv(shader, GL_COMPILE_STATUS, &ok);

    if (!ok) {
        GLint length = 0;
        gl33GetShaderiv(shader, GL_INFO_LOG_LENGTH, &length);

        std::string log(
            (std::size_t)(length > 0 ? length : 1),
            '\0'
        );

        gl33GetShaderInfoLog(
            shader,
            length,
            nullptr,
            &log[0]
        );

        std::fprintf(stderr, "UI shader error:\n%s\n", log.c_str());
        gl33DeleteShader(shader);
        return 0;
    }

    return shader;
}

static GLuint create_program()
{
    static const char* vs =
        "#version 330 core\n"
        "layout(location=0) in vec2 aPos;\n"
        "layout(location=1) in vec3 aColor;\n"
        "out vec3 vColor;\n"
        "void main(){\n"
        "  gl_Position=vec4(aPos,0.0,1.0);\n"
        "  vColor=aColor;\n"
        "}\n";

    static const char* fs =
        "#version 330 core\n"
        "in vec3 vColor;\n"
        "out vec4 FragColor;\n"
        "void main(){ FragColor=vec4(vColor,1.0); }\n";

    GLuint v = compile_shader(GL_VERTEX_SHADER, vs);
    GLuint f = compile_shader(GL_FRAGMENT_SHADER, fs);

    if (!v || !f)
        return 0;

    GLuint p = gl33CreateProgram();

    gl33AttachShader(p, v);
    gl33AttachShader(p, f);
    gl33LinkProgram(p);

    gl33DeleteShader(v);
    gl33DeleteShader(f);

    GLint ok = 0;
    gl33GetProgramiv(p, GL_LINK_STATUS, &ok);

    if (!ok) {
        gl33DeleteProgram(p);
        return 0;
    }

    return p;
}

bool ui_init(UiRenderer& ui)
{
    ui.program = create_program();

    if (!ui.program)
        return false;

    gl33GenVertexArrays(1, &ui.vao);
    gl33BindVertexArray(ui.vao);

    gl33GenBuffers(1, &ui.vbo);
    gl33BindBuffer(GL_ARRAY_BUFFER, ui.vbo);

    gl33BufferData(
        GL_ARRAY_BUFFER,
        1024,
        nullptr,
        GL_DYNAMIC_DRAW
    );

    gl33VertexAttribPointer(
        0,
        2,
        GL_FLOAT,
        GL_FALSE,
        sizeof(UiVertex),
        (const void*)0
    );

    gl33EnableVertexAttribArray(0);

    gl33VertexAttribPointer(
        1,
        3,
        GL_FLOAT,
        GL_FALSE,
        sizeof(UiVertex),
        (const void*)(2 * sizeof(float))
    );

    gl33EnableVertexAttribArray(1);

    gl33BindVertexArray(0);

    return true;
}

void ui_shutdown(UiRenderer& ui)
{
    if (ui.vbo)
        gl33DeleteBuffers(1, &ui.vbo);

    if (ui.vao)
        gl33DeleteVertexArrays(1, &ui.vao);

    if (ui.program)
        gl33DeleteProgram(ui.program);

    ui = UiRenderer{};
}

void ui_begin(UiRenderer& ui)
{
    ui.vertices.clear();
}

static void push_tri(
    UiRenderer& ui,
    float x0,
    float y0,
    float x1,
    float y1,
    float x2,
    float y2,
    float r,
    float g,
    float b
)
{
    ui.vertices.push_back({x0, y0, r, g, b});
    ui.vertices.push_back({x1, y1, r, g, b});
    ui.vertices.push_back({x2, y2, r, g, b});
}

void ui_rect(
    UiRenderer& ui,
    float x,
    float y,
    float w,
    float h,
    float r,
    float g,
    float b
)
{
    push_tri(ui, x, y, x+w, y, x+w, y+h, r,g,b);
    push_tri(ui, x, y, x+w, y+h, x, y+h, r,g,b);
}

/*
5x7 bitmap font.
Only characters used by this menu are included.
*/
static const unsigned char* glyph(char c)
{
    static const std::map<char, std::array<unsigned char,7>> font = {
        {' ', {0,0,0,0,0,0,0}},
        {'/', {1,2,4,8,16,0,0}},
        {'.', {0,0,0,0,0,12,12}},
        {'-', {0,0,0,31,0,0,0}},
        {':', {0,4,4,0,4,4,0}},
        {'0', {14,17,19,21,25,17,14}},
        {'1', {4,12,4,4,4,4,14}},
        {'2', {14,17,1,2,4,8,31}},
        {'3', {30,1,1,14,1,1,30}},
        {'4', {2,6,10,18,31,2,2}},
        {'5', {31,16,16,30,1,1,30}},
        {'6', {14,16,16,30,17,17,14}},
        {'7', {31,1,2,4,8,8,8}},
        {'8', {14,17,17,14,17,17,14}},
        {'9', {14,17,17,15,1,1,14}},
        {'A', {14,17,17,31,17,17,17}},
        {'B', {30,17,17,30,17,17,30}},
        {'C', {14,17,16,16,16,17,14}},
        {'D', {30,17,17,17,17,17,30}},
        {'E', {31,16,16,30,16,16,31}},
        {'F', {31,16,16,30,16,16,16}},
        {'G', {14,17,16,23,17,17,15}},
        {'H', {17,17,17,31,17,17,17}},
        {'I', {14,4,4,4,4,4,14}},
        {'J', {7,2,2,2,2,18,12}},
        {'K', {17,18,20,24,20,18,17}},
        {'L', {16,16,16,16,16,16,31}},
        {'M', {17,27,21,21,17,17,17}},
        {'N', {17,25,21,19,17,17,17}},
        {'O', {14,17,17,17,17,17,14}},
        {'P', {30,17,17,30,16,16,16}},
        {'Q', {14,17,17,17,21,18,13}},
        {'R', {30,17,17,30,20,18,17}},
        {'S', {15,16,16,14,1,1,30}},
        {'T', {31,4,4,4,4,4,4}},
        {'U', {17,17,17,17,17,17,14}},
        {'V', {17,17,17,17,17,10,4}},
        {'W', {17,17,17,21,21,21,10}},
        {'X', {17,17,10,4,10,17,17}},
        {'Y', {17,17,10,4,4,4,4}},
        {'Z', {31,1,2,4,8,16,31}}
    };

    auto it = font.find(c);

    if (it == font.end()) {
        auto space = font.find(' ');
        return space->second.data();
    }

    return it->second.data();
}

void ui_text(
    UiRenderer& ui,
    const std::string& text,
    float x,
    float y,
    float scale,
    float r,
    float g,
    float b
)
{
    float cursor = x;

    for (char raw : text) {
        char c = raw;

        if (c >= 'a' && c <= 'z')
            c = (char)(c - 'a' + 'A');

        const unsigned char* rows = glyph(c);

        for (int row = 0; row < 7; ++row) {
            for (int col = 0; col < 5; ++col) {
                if (rows[row] & (1 << (4 - col))) {
                    ui_rect(
                        ui,
                        cursor + col * scale,
                        y + row * scale,
                        scale,
                        scale,
                        r, g, b
                    );
                }
            }
        }

        cursor += 6.0f * scale;
    }
}

void ui_draw(
    UiRenderer& ui,
    int width,
    int height
)
{
    if (ui.vertices.empty())
        return;

    std::vector<UiVertex> ndc = ui.vertices;

    for (UiVertex& v : ndc) {
        v.x = (v.x / (float)width) * 2.0f - 1.0f;
        v.y = 1.0f - (v.y / (float)height) * 2.0f;
    }

    glDisable(GL_DEPTH_TEST);

    gl33UseProgram(ui.program);
    gl33BindVertexArray(ui.vao);
    gl33BindBuffer(GL_ARRAY_BUFFER, ui.vbo);

    gl33BufferData(
        GL_ARRAY_BUFFER,
        (GLsizeiptr33)(ndc.size() * sizeof(UiVertex)),
        ndc.data(),
        GL_DYNAMIC_DRAW
    );

    glDrawArrays(
        GL_TRIANGLES,
        0,
        (GLsizei)ndc.size()
    );

    gl33BindVertexArray(0);
    glEnable(GL_DEPTH_TEST);
}
