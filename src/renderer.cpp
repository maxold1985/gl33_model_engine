#include "renderer.h"
#include "gl33.h"
#include "math3d.h"
#include "model_loader.h"


#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdio>
#include <cstring>
#include <limits>
#include <string>
#include <vector>
#include <utility>

struct Vertex {
    float px, py, pz;
    float nx, ny, nz;
    float u, v;
};

static GLuint compile_shader(
    GLenum type,
    const char* source
)
{
    GLuint shader =
        gl33CreateShader(type);

    gl33ShaderSource(
        shader,
        1,
        &source,
        nullptr
    );

    gl33CompileShader(shader);

    GLint ok = 0;

    gl33GetShaderiv(
        shader,
        GL_COMPILE_STATUS,
        &ok
    );

    if (!ok) {
        GLint length = 0;

        gl33GetShaderiv(
            shader,
            GL_INFO_LOG_LENGTH,
            &length
        );

        if (length < 1)
            length = 1;

        std::string log(
            (std::size_t)length,
            '\0'
        );

        gl33GetShaderInfoLog(
            shader,
            length,
            nullptr,
            &log[0]
        );

        std::fprintf(
            stderr,
            "Shader compile error:\n%s\n",
            log.c_str()
        );

        gl33DeleteShader(shader);
        return 0;
    }

    return shader;
}

static GLuint create_program()
{
    static const char* vs =
        "#version 330 core\n"
        "layout(location=0) in vec3 aPosition;\n"
        "layout(location=1) in vec3 aNormal;\n"
        "layout(location=2) in vec2 aUV;\n"
        "uniform mat4 uMVP;\n"
        "out vec3 vNormal;\n"
        "out vec2 vUV;\n"
        "void main(){\n"
        "    gl_Position = uMVP * vec4(aPosition,1.0);\n"
        "    vNormal = aNormal;\n"
        "    vUV = aUV;\n"
        "}\n";

    static const char* fs =
        "#version 330 core\n"
        "in vec3 vNormal;\n"
        "in vec2 vUV;\n"
        "uniform sampler2D uTexture;\n"
        "uniform int uUseTexture;\n"
        "uniform vec4 uBaseColor;\n"
        "uniform float uMetallic;\n"
        "uniform float uRoughness;\n"
        "out vec4 FragColor;\n"
        "void main(){\n"
        "    vec4 texel = uUseTexture != 0\n"
        "        ? texture(uTexture, vUV)\n"
        "        : vec4(1.0);\n"
        "\n"
        "    vec4 base = texel * uBaseColor;\n"
        "\n"
        "    vec3 N = normalize(vNormal);\n"
        "\n"
        "    // Outdoor key + fill + sky illumination over the whole map.\n"
        "    vec3 L0 = normalize(vec3(-0.42,0.88,0.34));\n"
        "    vec3 L1 = normalize(vec3(0.62,0.38,-0.52));\n"
        "    vec3 V = normalize(vec3(0.0,0.0,1.0));\n"
        "    vec3 H = normalize(L0 + V);\n"
        "\n"
        "    float keyLight = max(dot(N,L0),0.0);\n"
        "    float fillLight = max(dot(N,L1),0.0);\n"
        "    float skyLight = clamp(N.y*0.5+0.5,0.0,1.0);\n"
        "    float rough = clamp(uRoughness,0.04,1.0);\n"
        "    float metal = clamp(uMetallic,0.0,1.0);\n"
        "    float power = mix(96.0,4.0,rough);\n"
        "    float spec = pow(max(dot(N,H),0.0),power);\n"
        "\n"
        "    vec3 dielectricF0 = vec3(0.04);\n"
        "    vec3 F0 = mix(dielectricF0,base.rgb,metal);\n"
        "    vec3 diffuse = base.rgb * (1.0-metal);\n"
        "    float lightAmount = 0.46 + keyLight*0.62 + fillLight*0.22 + skyLight*0.12;\n"
        "    vec3 color = diffuse * lightAmount;\n"
        "               color += F0 * spec * (1.0-rough*0.65);\n"
        "\n"
        "    FragColor = vec4(color,base.a);\n"
        "}\n";

    GLuint v =
        compile_shader(
            GL_VERTEX_SHADER,
            vs
        );

    GLuint f =
        compile_shader(
            GL_FRAGMENT_SHADER,
            fs
        );

    if (!v || !f)
        return 0;

    GLuint p =
        gl33CreateProgram();

    gl33AttachShader(p, v);
    gl33AttachShader(p, f);

    gl33LinkProgram(p);

    gl33DeleteShader(v);
    gl33DeleteShader(f);

    GLint ok = 0;

    gl33GetProgramiv(
        p,
        GL_LINK_STATUS,
        &ok
    );

    if (!ok) {
        GLint length = 0;

        gl33GetProgramiv(
            p,
            GL_INFO_LOG_LENGTH,
            &length
        );

        if (length < 1)
            length = 1;

        std::string log(
            (std::size_t)length,
            '\0'
        );

        gl33GetProgramInfoLog(
            p,
            length,
            nullptr,
            &log[0]
        );

        std::fprintf(
            stderr,
            "Program link error:\n%s\n",
            log.c_str()
        );

        gl33DeleteProgram(p);
        return 0;
    }

    return p;
}

static void destroy_mesh(
    GpuMesh& mesh
)
{
    if (mesh.texture)
        glDeleteTextures(
            1,
            &mesh.texture
        );

    if (mesh.ebo)
        gl33DeleteBuffers(
            1,
            &mesh.ebo
        );

    if (mesh.vbo)
        gl33DeleteBuffers(
            1,
            &mesh.vbo
        );

    if (mesh.vao)
        gl33DeleteVertexArrays(
            1,
            &mesh.vao
        );

    mesh = GpuMesh{};
}

static void destroy_model_parts(
    Renderer& renderer
)
{
    for (GpuMesh& part :
         renderer.model_parts) {
        destroy_mesh(part);
    }

    renderer.model_parts.clear();

    renderer.animation_nodes.clear();
    renderer.animation_clips.clear();

    const float identity_inverse[16] = {
        1,0,0,0,
        0,1,0,0,
        0,0,1,0,
        0,0,0,1
    };

    std::memcpy(
        renderer.animation_global_inverse,
        identity_inverse,
        sizeof(identity_inverse)
    );

    renderer.animation_count = 0;
    renderer.animation_index = 0;
    renderer.animation_time = 0.0f;
    renderer.animation_playing = false;

    renderer.model_center[0] = 0.0f;
    renderer.model_center[1] = 0.0f;
    renderer.model_center[2] = 0.0f;
    renderer.model_radius = 1.0f;
}

static void setup_vertex_layout()
{
    gl33VertexAttribPointer(
        0,
        3,
        GL_FLOAT,
        GL_FALSE,
        sizeof(Vertex),
        (const void*)
            offsetof(Vertex, px)
    );

    gl33EnableVertexAttribArray(0);

    gl33VertexAttribPointer(
        1,
        3,
        GL_FLOAT,
        GL_FALSE,
        sizeof(Vertex),
        (const void*)
            offsetof(Vertex, nx)
    );

    gl33EnableVertexAttribArray(1);

    gl33VertexAttribPointer(
        2,
        2,
        GL_FLOAT,
        GL_FALSE,
        sizeof(Vertex),
        (const void*)
            offsetof(Vertex, u)
    );

    gl33EnableVertexAttribArray(2);
}

static bool upload_mesh(
    GpuMesh& mesh,
    const std::vector<Vertex>& vertices,
    const std::vector<std::uint32_t>& indices,
    const GlbMaterial* material
)
{
    destroy_mesh(mesh);

    if (vertices.empty() ||
        indices.empty()) {
        return false;
    }

    gl33GenVertexArrays(
        1,
        &mesh.vao
    );

    gl33BindVertexArray(
        mesh.vao
    );

    gl33GenBuffers(
        1,
        &mesh.vbo
    );

    gl33BindBuffer(
        GL_ARRAY_BUFFER,
        mesh.vbo
    );

    gl33BufferData(
        GL_ARRAY_BUFFER,
        (GLsizeiptr33)(
            vertices.size() *
            sizeof(Vertex)
        ),
        vertices.data(),
        GL_STATIC_DRAW
    );

    gl33GenBuffers(
        1,
        &mesh.ebo
    );

    gl33BindBuffer(
        GL_ELEMENT_ARRAY_BUFFER,
        mesh.ebo
    );

    gl33BufferData(
        GL_ELEMENT_ARRAY_BUFFER,
        (GLsizeiptr33)(
            indices.size() *
            sizeof(std::uint32_t)
        ),
        indices.data(),
        GL_STATIC_DRAW
    );

    setup_vertex_layout();

    gl33BindVertexArray(0);

    mesh.index_count =
        (int)indices.size();

    if (material) {
        for (int i = 0; i < 4; ++i) {
            mesh.base_color_factor[i] =
                material->base_color_factor[i];
        }

        mesh.metallic_factor =
            material->metallic_factor;

        mesh.roughness_factor =
            material->roughness_factor;
    } else {
        mesh.base_color_factor[0] = 1.0f;
        mesh.base_color_factor[1] = 1.0f;
        mesh.base_color_factor[2] = 1.0f;
        mesh.base_color_factor[3] = 1.0f;

        mesh.metallic_factor = 0.0f;
        mesh.roughness_factor = 1.0f;
    }

    {
        float minv[3] = {
            vertices[0].px,
            vertices[0].py,
            vertices[0].pz
        };

        float maxv[3] = {
            vertices[0].px,
            vertices[0].py,
            vertices[0].pz
        };

        for (const Vertex& v :
             vertices) {
            minv[0] =
                std::min(
                    minv[0],
                    v.px
                );

            minv[1] =
                std::min(
                    minv[1],
                    v.py
                );

            minv[2] =
                std::min(
                    minv[2],
                    v.pz
                );

            maxv[0] =
                std::max(
                    maxv[0],
                    v.px
                );

            maxv[1] =
                std::max(
                    maxv[1],
                    v.py
                );

            maxv[2] =
                std::max(
                    maxv[2],
                    v.pz
                );
        }

        mesh.center[0] =
            (minv[0] + maxv[0]) * 0.5f;

        mesh.center[1] =
            (minv[1] + maxv[1]) * 0.5f;

        mesh.center[2] =
            (minv[2] + maxv[2]) * 0.5f;

        const float ex =
            (maxv[0] - minv[0]) * 0.5f;

        const float ey =
            (maxv[1] - minv[1]) * 0.5f;

        const float ez =
            (maxv[2] - minv[2]) * 0.5f;

        mesh.radius =
            std::sqrt(
                ex*ex +
                ey*ey +
                ez*ez
            );

        if (mesh.radius < 0.0001f)
            mesh.radius = 1.0f;
    }

    if (material) {
        const GlbImage& image =
            material->base_color_image;

        if (image.width > 0 &&
            image.height > 0 &&
            !image.rgba.empty()) {

            glGenTextures(
                1,
                &mesh.texture
            );

            glBindTexture(
                GL_TEXTURE_2D,
                mesh.texture
            );

            glTexParameteri(
                GL_TEXTURE_2D,
                GL_TEXTURE_MIN_FILTER,
                GL_LINEAR_MIPMAP_LINEAR
            );

            glTexParameteri(
                GL_TEXTURE_2D,
                GL_TEXTURE_MAG_FILTER,
                GL_LINEAR
            );

            glTexParameteri(
                GL_TEXTURE_2D,
                GL_TEXTURE_WRAP_S,
                GL_REPEAT
            );

            glTexParameteri(
                GL_TEXTURE_2D,
                GL_TEXTURE_WRAP_T,
                GL_REPEAT
            );

            glTexImage2D(
                GL_TEXTURE_2D,
                0,
                GL_RGBA8,
                image.width,
                image.height,
                0,
                GL_RGBA,
                GL_UNSIGNED_BYTE,
                image.rgba.data()
            );

            gl33GenerateMipmap(
                GL_TEXTURE_2D
            );

            mesh.has_texture = true;
        }
    }

    return true;
}

static void make_cube(
    GpuMesh& mesh
)
{
    static const float p[] = {
        -1,-1, 1,  0,0,1,  0,1,
         1,-1, 1,  0,0,1,  1,1,
         1, 1, 1,  0,0,1,  1,0,
        -1, 1, 1,  0,0,1,  0,0,

         1,-1,-1,  0,0,-1, 0,1,
        -1,-1,-1,  0,0,-1, 1,1,
        -1, 1,-1,  0,0,-1, 1,0,
         1, 1,-1,  0,0,-1, 0,0,

        -1,-1,-1, -1,0,0, 0,1,
        -1,-1, 1, -1,0,0, 1,1,
        -1, 1, 1, -1,0,0, 1,0,
        -1, 1,-1, -1,0,0, 0,0,

         1,-1, 1,  1,0,0, 0,1,
         1,-1,-1,  1,0,0, 1,1,
         1, 1,-1,  1,0,0, 1,0,
         1, 1, 1,  1,0,0, 0,0,

        -1, 1, 1,  0,1,0, 0,1,
         1, 1, 1,  0,1,0, 1,1,
         1, 1,-1,  0,1,0, 1,0,
        -1, 1,-1,  0,1,0, 0,0,

        -1,-1,-1,  0,-1,0, 0,1,
         1,-1,-1,  0,-1,0, 1,1,
         1,-1, 1,  0,-1,0, 1,0,
        -1,-1, 1,  0,-1,0, 0,0
    };

    static const std::uint32_t idx[] = {
         0, 1, 2,  0, 2, 3,
         4, 5, 6,  4, 6, 7,
         8, 9,10,  8,10,11,
        12,13,14, 12,14,15,
        16,17,18, 16,18,19,
        20,21,22, 20,22,23
    };

    std::vector<Vertex> vertices(24);

    for (int i = 0; i < 24; ++i) {
        const float* s =
            p + i * 8;

        vertices[
            (std::size_t)i
        ] = {
            s[0], s[1], s[2],
            s[3], s[4], s[5],
            s[6], s[7]
        };
    }

    std::vector<std::uint32_t> indices(
        idx,
        idx +
            sizeof(idx) /
            sizeof(idx[0])
    );

    upload_mesh(
        mesh,
        vertices,
        indices,
        nullptr
    );
}


static void make_ground_plane(
    GpuMesh& mesh
)
{
    /*
        Unit 1x1 plane centered at origin in XZ.
        renderer_draw scales it to 10x10.
        Both windings are included so it is visible from above/below.
    */
    std::vector<Vertex> vertices = {
        {-0.5f, 0.0f, -0.5f,  0,1,0,  0,0},
        {-0.5f, 0.0f,  0.5f,  0,1,0,  0,1},
        { 0.5f, 0.0f,  0.5f,  0,1,0,  1,1},
        { 0.5f, 0.0f, -0.5f,  0,1,0,  1,0}
    };

    std::vector<std::uint32_t> indices = {
        0,1,2,
        0,2,3,

        2,1,0,
        3,2,0
    };

    upload_mesh(
        mesh,
        vertices,
        indices,
        nullptr
    );

    /*
        Green ground.
    */
    mesh.base_color_factor[0] = 0.10f;
    mesh.base_color_factor[1] = 0.72f;
    mesh.base_color_factor[2] = 0.18f;
    mesh.base_color_factor[3] = 1.00f;

    mesh.metallic_factor = 0.0f;
    mesh.roughness_factor = 0.95f;
}

static void make_projectile_sphere(
    GpuMesh& mesh
)
{
    /*
        Low-cost UV sphere for the i686/Intel-HD target.
    */
    const int stacks = 10;
    const int slices = 16;

    const float pi =
        3.14159265358979323846f;

    std::vector<Vertex> vertices;
    std::vector<std::uint32_t> indices;

    vertices.reserve(
        (stacks + 1) *
        (slices + 1)
    );

    for (int stack = 0;
         stack <= stacks;
         ++stack) {

        const float v =
            (float)stack /
            (float)stacks;

        const float phi =
            v * pi;

        const float y =
            std::cos(phi);

        const float ring =
            std::sin(phi);

        for (int slice = 0;
             slice <= slices;
             ++slice) {

            const float u =
                (float)slice /
                (float)slices;

            const float theta =
                u *
                pi *
                2.0f;

            const float x =
                ring *
                std::cos(theta);

            const float z =
                ring *
                std::sin(theta);

            vertices.push_back({
                x, y, z,
                x, y, z,
                u, 1.0f - v
            });
        }
    }

    for (int stack = 0;
         stack < stacks;
         ++stack) {

        for (int slice = 0;
             slice < slices;
             ++slice) {

            const std::uint32_t a =
                (std::uint32_t)(
                    stack *
                    (slices + 1) +
                    slice
                );

            const std::uint32_t b =
                a +
                (std::uint32_t)(
                    slices + 1
                );

            const std::uint32_t c =
                b + 1;

            const std::uint32_t d =
                a + 1;

            indices.push_back(a);
            indices.push_back(b);
            indices.push_back(c);

            indices.push_back(a);
            indices.push_back(c);
            indices.push_back(d);
        }
    }

    upload_mesh(
        mesh,
        vertices,
        indices,
        nullptr
    );

    /*
        Projectile color.
    */
    mesh.base_color_factor[0] = 1.00f;
    mesh.base_color_factor[1] = 0.72f;
    mesh.base_color_factor[2] = 0.10f;
    mesh.base_color_factor[3] = 1.00f;

    mesh.metallic_factor = 0.05f;
    mesh.roughness_factor = 0.55f;
}

bool renderer_init(
    Renderer& renderer
)
{
    renderer.program =
        create_program();

    if (!renderer.program)
        return false;

    renderer.u_mvp =
        gl33GetUniformLocation(
            renderer.program,
            "uMVP"
        );

    renderer.u_texture =
        gl33GetUniformLocation(
            renderer.program,
            "uTexture"
        );

    renderer.u_use_texture =
        gl33GetUniformLocation(
            renderer.program,
            "uUseTexture"
        );

    renderer.u_base_color =
        gl33GetUniformLocation(
            renderer.program,
            "uBaseColor"
        );

    renderer.u_metallic =
        gl33GetUniformLocation(
            renderer.program,
            "uMetallic"
        );

    renderer.u_roughness =
        gl33GetUniformLocation(
            renderer.program,
            "uRoughness"
        );

    make_cube(renderer.cube);
    make_ground_plane(
        renderer.ground_plane
    );
    make_projectile_sphere(
        renderer.projectile_sphere
    );

    /*
        Default waypoint loop inside the 100x100 map.
        The first AI position starts at waypoint 0 and targets waypoint 1.
    */
    renderer.ai_waypoints = {
        {-15.0f, -15.0f},
        {  0.0f, -22.0f},
        { 15.0f, -15.0f},
        { 22.0f,   0.0f},
        { 15.0f,  15.0f},
        {  0.0f,  22.0f},
        {-15.0f,  15.0f},
        {-22.0f,   0.0f}
    };

    renderer.ai_agent.x =
        renderer.ai_waypoints[0].x;

    renderer.ai_agent.z =
        renderer.ai_waypoints[0].z;

    renderer.ai_agent.waypoint_index = 1;

    /*
        Weapon/ammo pickups around the 100x100 map.
        Each pickup restores +5 shots.
    */
    renderer.ammo =
        renderer.max_ammo;

    renderer.ammo_pickups = {
        {  7.0f,   6.0f, 5, true},
        {-10.0f,   9.0f, 5, true},
        { 18.0f, -12.0f, 5, true},
        {-23.0f, -17.0f, 5, true},
        {  2.0f,  28.0f, 5, true},
        { 31.0f,  22.0f, 5, true}
    };

    if (!ui_init(renderer.ui))
        return false;

    glEnable(GL_DEPTH_TEST);
    glEnable(GL_CULL_FACE);
    glCullFace(GL_BACK);

    return true;
}

void renderer_shutdown(
    Renderer& renderer
)
{
    destroy_model_parts(renderer);

    destroy_mesh(renderer.cube);
    destroy_mesh(renderer.ground_plane);
    destroy_mesh(renderer.projectile_sphere);

    ui_shutdown(renderer.ui);

    if (renderer.program) {
        gl33DeleteProgram(
            renderer.program
        );
    }

    renderer = Renderer{};
}




struct SkinVec3 {
    float x = 0.0f;
    float y = 0.0f;
    float z = 0.0f;
};

struct SkinQuat {
    float x = 0.0f;
    float y = 0.0f;
    float z = 0.0f;
    float w = 1.0f;
};

/*
    Row-major 4x4 matrix matching the raw matrix layout stored from Assimp:

        m[0]  m[1]  m[2]  m[3]
        m[4]  m[5]  m[6]  m[7]
        m[8]  m[9]  m[10] m[11]
        m[12] m[13] m[14] m[15]

    No Assimp matrix/vector methods are used in the renderer.
*/
struct SkinMat4 {
    float m[16];
};

struct SkinMat3 {
    float m[9];
};

static SkinMat4 skin_mat4_identity()
{
    SkinMat4 out{};

    out.m[0]  = 1.0f;
    out.m[5]  = 1.0f;
    out.m[10] = 1.0f;
    out.m[15] = 1.0f;

    return out;
}

static SkinMat4 skin_mat4_from_raw(
    const float raw[16]
)
{
    SkinMat4 out{};

    for (int i = 0; i < 16; ++i)
        out.m[i] = raw[i];

    return out;
}

static SkinMat4 skin_mat4_mul(
    const SkinMat4& a,
    const SkinMat4& b
)
{
    SkinMat4 out{};

    for (int row = 0; row < 4; ++row) {
        for (int col = 0; col < 4; ++col) {
            float sum = 0.0f;

            for (int k = 0; k < 4; ++k) {
                sum +=
                    a.m[row * 4 + k] *
                    b.m[k * 4 + col];
            }

            out.m[row * 4 + col] = sum;
        }
    }

    return out;
}

static SkinVec3 skin_transform_point(
    const SkinMat4& m,
    const SkinVec3& v
)
{
    SkinVec3 out;

    out.x =
        m.m[0] * v.x +
        m.m[1] * v.y +
        m.m[2] * v.z +
        m.m[3];

    out.y =
        m.m[4] * v.x +
        m.m[5] * v.y +
        m.m[6] * v.z +
        m.m[7];

    out.z =
        m.m[8] * v.x +
        m.m[9] * v.y +
        m.m[10] * v.z +
        m.m[11];

    return out;
}

static SkinVec3 skin_transform_vector3(
    const SkinMat3& m,
    const SkinVec3& v
)
{
    SkinVec3 out;

    out.x =
        m.m[0] * v.x +
        m.m[1] * v.y +
        m.m[2] * v.z;

    out.y =
        m.m[3] * v.x +
        m.m[4] * v.y +
        m.m[5] * v.z;

    out.z =
        m.m[6] * v.x +
        m.m[7] * v.y +
        m.m[8] * v.z;

    return out;
}

static SkinVec3 skin_vec3_add(
    const SkinVec3& a,
    const SkinVec3& b
)
{
    return {
        a.x + b.x,
        a.y + b.y,
        a.z + b.z
    };
}

static SkinVec3 skin_vec3_scale(
    const SkinVec3& v,
    float s
)
{
    return {
        v.x * s,
        v.y * s,
        v.z * s
    };
}

static void skin_vec3_add_scaled(
    SkinVec3& dst,
    const SkinVec3& src,
    float s
)
{
    dst.x += src.x * s;
    dst.y += src.y * s;
    dst.z += src.z * s;
}

static SkinVec3 skin_vec3_normalize(
    SkinVec3 v
)
{
    const float len =
        std::sqrt(
            v.x*v.x +
            v.y*v.y +
            v.z*v.z
        );

    if (len > 0.000001f) {
        const float inv = 1.0f / len;

        v.x *= inv;
        v.y *= inv;
        v.z *= inv;
    }

    return v;
}

static SkinQuat skin_quat_normalize(
    SkinQuat q
)
{
    const float len =
        std::sqrt(
            q.x*q.x +
            q.y*q.y +
            q.z*q.z +
            q.w*q.w
        );

    if (len <= 0.000001f)
        return SkinQuat{};

    const float inv = 1.0f / len;

    q.x *= inv;
    q.y *= inv;
    q.z *= inv;
    q.w *= inv;

    return q;
}

static float skin_quat_dot(
    const SkinQuat& a,
    const SkinQuat& b
)
{
    return
        a.x*b.x +
        a.y*b.y +
        a.z*b.z +
        a.w*b.w;
}

static SkinQuat skin_quat_slerp(
    SkinQuat a,
    SkinQuat b,
    float t
)
{
    a = skin_quat_normalize(a);
    b = skin_quat_normalize(b);

    float dot =
        skin_quat_dot(a, b);

    if (dot < 0.0f) {
        dot = -dot;

        b.x = -b.x;
        b.y = -b.y;
        b.z = -b.z;
        b.w = -b.w;
    }

    if (dot > 0.9995f) {
        return skin_quat_normalize({
            a.x + (b.x-a.x)*t,
            a.y + (b.y-a.y)*t,
            a.z + (b.z-a.z)*t,
            a.w + (b.w-a.w)*t
        });
    }

    dot = std::max(
        -1.0f,
        std::min(1.0f, dot)
    );

    const float theta =
        std::acos(dot);

    const float sin_theta =
        std::sin(theta);

    if (std::fabs(sin_theta) <
        0.000001f) {
        return a;
    }

    const float wa =
        std::sin(
            (1.0f - t) *
            theta
        ) / sin_theta;

    const float wb =
        std::sin(
            t * theta
        ) / sin_theta;

    return skin_quat_normalize({
        a.x*wa + b.x*wb,
        a.y*wa + b.y*wb,
        a.z*wa + b.z*wb,
        a.w*wa + b.w*wb
    });
}

static SkinVec3 sample_vec3_keys(
    const std::vector<ModelVec3Key>& keys,
    double tick,
    const SkinVec3& fallback
)
{
    if (keys.empty())
        return fallback;

    if (keys.size() == 1 ||
        tick <= keys.front().time_ticks) {

        return {
            keys.front().x,
            keys.front().y,
            keys.front().z
        };
    }

    if (tick >= keys.back().time_ticks) {
        return {
            keys.back().x,
            keys.back().y,
            keys.back().z
        };
    }

    for (std::size_t i = 0;
         i + 1 < keys.size();
         ++i) {

        const ModelVec3Key& a = keys[i];
        const ModelVec3Key& b = keys[i + 1];

        if (tick >= a.time_ticks &&
            tick <= b.time_ticks) {

            const double span =
                b.time_ticks -
                a.time_ticks;

            const float t =
                span > 0.0
                    ? (float)(
                        (tick - a.time_ticks) /
                        span
                    )
                    : 0.0f;

            return {
                a.x + (b.x-a.x)*t,
                a.y + (b.y-a.y)*t,
                a.z + (b.z-a.z)*t
            };
        }
    }

    return fallback;
}

static SkinQuat sample_quat_keys(
    const std::vector<ModelQuatKey>& keys,
    double tick,
    const SkinQuat& fallback
)
{
    if (keys.empty())
        return fallback;

    if (keys.size() == 1 ||
        tick <= keys.front().time_ticks) {

        return skin_quat_normalize({
            keys.front().x,
            keys.front().y,
            keys.front().z,
            keys.front().w
        });
    }

    if (tick >= keys.back().time_ticks) {
        return skin_quat_normalize({
            keys.back().x,
            keys.back().y,
            keys.back().z,
            keys.back().w
        });
    }

    for (std::size_t i = 0;
         i + 1 < keys.size();
         ++i) {

        const ModelQuatKey& a = keys[i];
        const ModelQuatKey& b = keys[i + 1];

        if (tick >= a.time_ticks &&
            tick <= b.time_ticks) {

            const double span =
                b.time_ticks -
                a.time_ticks;

            const float t =
                span > 0.0
                    ? (float)(
                        (tick - a.time_ticks) /
                        span
                    )
                    : 0.0f;

            return skin_quat_slerp(
                {a.x,a.y,a.z,a.w},
                {b.x,b.y,b.z,b.w},
                t
            );
        }
    }

    return fallback;
}

static SkinMat4 skin_mat4_from_trs(
    const SkinVec3& p,
    const SkinQuat& input_q,
    const SkinVec3& s
)
{
    const SkinQuat q =
        skin_quat_normalize(
            input_q
        );

    const float xx = q.x*q.x;
    const float yy = q.y*q.y;
    const float zz = q.z*q.z;
    const float xy = q.x*q.y;
    const float xz = q.x*q.z;
    const float yz = q.y*q.z;
    const float wx = q.w*q.x;
    const float wy = q.w*q.y;
    const float wz = q.w*q.z;

    /*
        Row-major matrix: T * R * S.
        Translation is in the last column.
    */
    SkinMat4 out =
        skin_mat4_identity();

    out.m[0] =
        (1.0f - 2.0f*(yy+zz)) *
        s.x;

    out.m[1] =
        (2.0f*(xy-wz)) *
        s.y;

    out.m[2] =
        (2.0f*(xz+wy)) *
        s.z;

    out.m[3] = p.x;

    out.m[4] =
        (2.0f*(xy+wz)) *
        s.x;

    out.m[5] =
        (1.0f - 2.0f*(xx+zz)) *
        s.y;

    out.m[6] =
        (2.0f*(yz-wx)) *
        s.z;

    out.m[7] = p.y;

    out.m[8] =
        (2.0f*(xz-wy)) *
        s.x;

    out.m[9] =
        (2.0f*(yz+wx)) *
        s.y;

    out.m[10] =
        (1.0f - 2.0f*(xx+yy)) *
        s.z;

    out.m[11] = p.z;

    out.m[12] = 0.0f;
    out.m[13] = 0.0f;
    out.m[14] = 0.0f;
    out.m[15] = 1.0f;

    return out;
}

static void skin_decompose_trs(
    const SkinMat4& m,
    SkinVec3& p,
    SkinQuat& q,
    SkinVec3& s
)
{
    p = {
        m.m[3],
        m.m[7],
        m.m[11]
    };

    /*
        With T*R*S, scale is length of the first three columns.
    */
    s.x =
        std::sqrt(
            m.m[0]*m.m[0] +
            m.m[4]*m.m[4] +
            m.m[8]*m.m[8]
        );

    s.y =
        std::sqrt(
            m.m[1]*m.m[1] +
            m.m[5]*m.m[5] +
            m.m[9]*m.m[9]
        );

    s.z =
        std::sqrt(
            m.m[2]*m.m[2] +
            m.m[6]*m.m[6] +
            m.m[10]*m.m[10]
        );

    if (s.x < 0.000001f) s.x = 1.0f;
    if (s.y < 0.000001f) s.y = 1.0f;
    if (s.z < 0.000001f) s.z = 1.0f;

    const float r00 = m.m[0] / s.x;
    const float r10 = m.m[4] / s.x;
    const float r20 = m.m[8] / s.x;

    const float r01 = m.m[1] / s.y;
    const float r11 = m.m[5] / s.y;
    const float r21 = m.m[9] / s.y;

    const float r02 = m.m[2] / s.z;
    const float r12 = m.m[6] / s.z;
    const float r22 = m.m[10] / s.z;

    SkinQuat out_q;

    const float trace =
        r00 + r11 + r22;

    if (trace > 0.0f) {
        const float ss =
            std::sqrt(
                trace + 1.0f
            ) * 2.0f;

        out_q.w = 0.25f * ss;
        out_q.x = (r21-r12) / ss;
        out_q.y = (r02-r20) / ss;
        out_q.z = (r10-r01) / ss;
    }
    else if (
        r00 > r11 &&
        r00 > r22) {

        const float ss =
            std::sqrt(
                1.0f + r00-r11-r22
            ) * 2.0f;

        out_q.w = (r21-r12) / ss;
        out_q.x = 0.25f * ss;
        out_q.y = (r01+r10) / ss;
        out_q.z = (r02+r20) / ss;
    }
    else if (r11 > r22) {
        const float ss =
            std::sqrt(
                1.0f + r11-r00-r22
            ) * 2.0f;

        out_q.w = (r02-r20) / ss;
        out_q.x = (r01+r10) / ss;
        out_q.y = 0.25f * ss;
        out_q.z = (r12+r21) / ss;
    }
    else {
        const float ss =
            std::sqrt(
                1.0f + r22-r00-r11
            ) * 2.0f;

        out_q.w = (r10-r01) / ss;
        out_q.x = (r02+r20) / ss;
        out_q.y = (r12+r21) / ss;
        out_q.z = 0.25f * ss;
    }

    q =
        skin_quat_normalize(
            out_q
        );
}

static SkinMat3 skin_normal_matrix(
    const SkinMat4& m
)
{
    /*
        For skinning transforms we need inverse-transpose of upper-left 3x3.
        Compute directly, no external library.
    */
    const float a = m.m[0];
    const float b = m.m[1];
    const float c = m.m[2];

    const float d = m.m[4];
    const float e = m.m[5];
    const float f = m.m[6];

    const float g = m.m[8];
    const float h = m.m[9];
    const float i = m.m[10];

    const float A =  (e*i - f*h);
    const float B = -(d*i - f*g);
    const float C =  (d*h - e*g);

    const float D = -(b*i - c*h);
    const float E =  (a*i - c*g);
    const float F = -(a*h - b*g);

    const float G =  (b*f - c*e);
    const float H = -(a*f - c*d);
    const float I =  (a*e - b*d);

    const float det =
        a*A + b*B + c*C;

    SkinMat3 out{};

    if (std::fabs(det) <
        0.000001f) {

        out.m[0] = 1.0f;
        out.m[4] = 1.0f;
        out.m[8] = 1.0f;

        return out;
    }

    const float inv_det =
        1.0f / det;

    /*
        Inverse-transpose = cofactor matrix / determinant.
    */
    out.m[0] = A * inv_det;
    out.m[1] = B * inv_det;
    out.m[2] = C * inv_det;

    out.m[3] = D * inv_det;
    out.m[4] = E * inv_det;
    out.m[5] = F * inv_det;

    out.m[6] = G * inv_det;
    out.m[7] = H * inv_det;
    out.m[8] = I * inv_det;

    return out;
}

static const ModelAnimationChannel*
find_animation_channel(
    const ModelAnimationClip& clip,
    const std::string& node_name
)
{
    for (const ModelAnimationChannel& channel :
         clip.channels) {

        if (channel.node_name ==
            node_name) {
            return &channel;
        }
    }

    return nullptr;
}

static std::string canonical_mixamo_name(
    const std::string& name
)
{
    std::string result =
        name;

    const std::size_t helper =
        result.find(
            "_$AssimpFbx$_"
        );

    if (helper !=
        std::string::npos) {

        result.resize(helper);
    }

    return result;
}

static int find_node_index(
    const std::vector<ModelNode>& nodes,
    const std::string& name
)
{
    for (std::size_t i = 0;
         i < nodes.size();
         ++i) {

        if (nodes[i].name == name)
            return (int)i;
    }

    const std::string wanted =
        canonical_mixamo_name(
            name
        );

    for (std::size_t i = 0;
         i < nodes.size();
         ++i) {

        if (canonical_mixamo_name(
                nodes[i].name) ==
            wanted) {

            return (int)i;
        }
    }

    return -1;
}

static void upload_animated_vertices(
    GpuMesh& mesh
)
{
    if (!mesh.vbo ||
        mesh.animated_vertices.empty()) {
        return;
    }

    std::vector<Vertex> gpu_vertices;

    gpu_vertices.reserve(
        mesh.animated_vertices.size()
    );

    for (const GlbVertex& v :
         mesh.animated_vertices) {

        gpu_vertices.push_back({
            v.px, v.py, v.pz,
            v.nx, v.ny, v.nz,
            v.u, v.v
        });
    }

    gl33BindBuffer(
        GL_ARRAY_BUFFER,
        mesh.vbo
    );

    gl33BufferData(
        GL_ARRAY_BUFFER,
        (GLsizeiptr33)(
            gpu_vertices.size() *
            sizeof(Vertex)
        ),
        gpu_vertices.data(),
        GL_DYNAMIC_DRAW
    );

    gl33BindBuffer(
        GL_ARRAY_BUFFER,
        0
    );
}

static void update_animation_pose(
    Renderer& renderer
)
{
    if (renderer.animation_clips.empty() ||
        renderer.animation_nodes.empty() ||
        renderer.model_parts.empty()) {
        return;
    }

    if (renderer.animation_index < 0)
        renderer.animation_index = 0;

    if ((std::size_t)
            renderer.animation_index >=
        renderer.animation_clips.size()) {

        renderer.animation_index =
            (int)
            renderer.animation_clips.size() -
            1;
    }

    const ModelAnimationClip& clip =
        renderer.animation_clips[
            (std::size_t)
            renderer.animation_index
        ];

    double seconds =
        renderer.animation_time;

    if (clip.duration_seconds >
        0.000001) {

        seconds =
            std::fmod(
                seconds,
                clip.duration_seconds
            );

        if (seconds < 0.0)
            seconds +=
                clip.duration_seconds;
    }

    const double tick =
        seconds *
        clip.ticks_per_second;

    const SkinMat4 inverse_root =
        skin_mat4_from_raw(
            renderer.animation_global_inverse
        );

    std::vector<SkinMat4> global(
        renderer.animation_nodes.size()
    );

    for (std::size_t node_index = 0;
         node_index <
            renderer.animation_nodes.size();
         ++node_index) {

        const ModelNode& node =
            renderer.animation_nodes[
                node_index
            ];

        SkinMat4 local =
            skin_mat4_from_raw(
                node.local_transform
            );

        const ModelAnimationChannel* channel =
            find_animation_channel(
                clip,
                node.name
            );

        if (channel) {
            SkinVec3 bind_position;
            SkinQuat bind_rotation;
            SkinVec3 bind_scale;

            skin_decompose_trs(
                local,
                bind_position,
                bind_rotation,
                bind_scale
            );

            const SkinVec3 position =
                sample_vec3_keys(
                    channel->position_keys,
                    tick,
                    bind_position
                );

            const SkinQuat rotation =
                sample_quat_keys(
                    channel->rotation_keys,
                    tick,
                    bind_rotation
                );

            const SkinVec3 scale =
                sample_vec3_keys(
                    channel->scaling_keys,
                    tick,
                    bind_scale
                );

            local =
                skin_mat4_from_trs(
                    position,
                    rotation,
                    scale
                );
        }

        const int parent =
            node.parent_index;

        if (parent >= 0 &&
            (std::size_t)parent <
                global.size()) {

            global[node_index] =
                skin_mat4_mul(
                    global[
                        (std::size_t)
                        parent
                    ],
                    local
                );
        } else {
            global[node_index] =
                local;
        }
    }

    for (GpuMesh& part :
         renderer.model_parts) {

        if (!part.cpu_animated ||
            part.bind_vertices.empty()) {
            continue;
        }

        part.animated_vertices =
            part.bind_vertices;

        if (!part.bones.empty()) {
            std::vector<SkinVec3> positions(
                part.bind_vertices.size()
            );

            std::vector<SkinVec3> normals(
                part.bind_vertices.size()
            );

            std::vector<float> weights(
                part.bind_vertices.size(),
                0.0f
            );

            for (std::size_t bone_index = 0;
                 bone_index <
                    part.bones.size();
                 ++bone_index) {

                if (bone_index >=
                    part.bone_node_indices.size()) {
                    continue;
                }

                const int bone_node =
                    part.bone_node_indices[
                        bone_index
                    ];

                if (bone_node < 0 ||
                    (std::size_t)bone_node >=
                        global.size()) {
                    continue;
                }

                const ModelBone& bone =
                    part.bones[
                        bone_index
                    ];

                const SkinMat4 offset =
                    skin_mat4_from_raw(
                        bone.offset_matrix
                    );

                const SkinMat4 final_matrix =
                    skin_mat4_mul(
                        inverse_root,
                        skin_mat4_mul(
                            global[
                                (std::size_t)
                                bone_node
                            ],
                            offset
                        )
                    );

                const SkinMat3 normal_matrix =
                    skin_normal_matrix(
                        final_matrix
                    );

                for (const ModelBoneWeight& bw :
                     bone.weights) {

                    const std::size_t vi =
                        (std::size_t)
                        bw.vertex_index;

                    if (vi >=
                        part.bind_vertices.size()) {
                        continue;
                    }

                    const GlbVertex& bind =
                        part.bind_vertices[vi];

                    const SkinVec3 p{
                        bind.px,
                        bind.py,
                        bind.pz
                    };

                    const SkinVec3 n{
                        bind.nx,
                        bind.ny,
                        bind.nz
                    };

                    const SkinVec3 tp =
                        skin_transform_point(
                            final_matrix,
                            p
                        );

                    const SkinVec3 tn =
                        skin_vec3_normalize(
                            skin_transform_vector3(
                                normal_matrix,
                                n
                            )
                        );

                    skin_vec3_add_scaled(
                        positions[vi],
                        tp,
                        bw.weight
                    );

                    skin_vec3_add_scaled(
                        normals[vi],
                        tn,
                        bw.weight
                    );

                    weights[vi] +=
                        bw.weight;
                }
            }

            for (std::size_t vi = 0;
                 vi <
                    part.bind_vertices.size();
                 ++vi) {

                GlbVertex& dst =
                    part.animated_vertices[vi];

                if (weights[vi] >
                    0.000001f) {

                    const float inv =
                        1.0f /
                        weights[vi];

                    const SkinVec3 p =
                        skin_vec3_scale(
                            positions[vi],
                            inv
                        );

                    const SkinVec3 n =
                        skin_vec3_normalize(
                            skin_vec3_scale(
                                normals[vi],
                                inv
                            )
                        );

                    dst.px = p.x;
                    dst.py = p.y;
                    dst.pz = p.z;

                    dst.nx = n.x;
                    dst.ny = n.y;
                    dst.nz = n.z;
                }
            }
        }
        else if (
            part.node_index >= 0 &&
            (std::size_t)part.node_index <
                global.size()) {

            const SkinMat4 final_matrix =
                skin_mat4_mul(
                    inverse_root,
                    global[
                        (std::size_t)
                        part.node_index
                    ]
                );

            const SkinMat3 normal_matrix =
                skin_normal_matrix(
                    final_matrix
                );

            for (std::size_t vi = 0;
                 vi <
                    part.bind_vertices.size();
                 ++vi) {

                const GlbVertex& bind =
                    part.bind_vertices[vi];

                const SkinVec3 p{
                    bind.px,
                    bind.py,
                    bind.pz
                };

                const SkinVec3 n{
                    bind.nx,
                    bind.ny,
                    bind.nz
                };

                const SkinVec3 tp =
                    skin_transform_point(
                        final_matrix,
                        p
                    );

                const SkinVec3 tn =
                    skin_vec3_normalize(
                        skin_transform_vector3(
                            normal_matrix,
                            n
                        )
                    );

                GlbVertex& dst =
                    part.animated_vertices[vi];

                dst.px = tp.x;
                dst.py = tp.y;
                dst.pz = tp.z;

                dst.nx = tn.x;
                dst.ny = tn.y;
                dst.nz = tn.z;
            }
        }

        upload_animated_vertices(
            part
        );
    }
}

bool renderer_load_model(
    Renderer& renderer,
    const std::string& path
)
{
    GlbModel model;
    std::string error;

    std::printf(
        "Loading model: %s\n",
        path.c_str()
    );

    if (!model_load(
            path,
            model,
            error)) {

        std::fprintf(
            stderr,
            "MODEL LOAD ERROR:\n%s\n",
            error.c_str()
        );

        renderer.status =
            "ERROR " + error;

        return false;
    }

    destroy_model_parts(renderer);

    float minv[3] = {
        std::numeric_limits<float>::max(),
        std::numeric_limits<float>::max(),
        std::numeric_limits<float>::max()
    };

    float maxv[3] = {
        -std::numeric_limits<float>::max(),
        -std::numeric_limits<float>::max(),
        -std::numeric_limits<float>::max()
    };

    std::size_t total_vertices = 0;
    std::size_t total_indices = 0;
    std::size_t textures = 0;

    for (const GlbPrimitive& primitive :
         model.primitives) {

        std::vector<Vertex> vertices;
        vertices.reserve(
            primitive.vertices.size()
        );

        for (const GlbVertex& v :
             primitive.vertices) {

            vertices.push_back({
                v.px, v.py, v.pz,
                v.nx, v.ny, v.nz,
                v.u, v.v
            });

            minv[0] =
                std::min(minv[0], v.px);

            minv[1] =
                std::min(minv[1], v.py);

            minv[2] =
                std::min(minv[2], v.pz);

            maxv[0] =
                std::max(maxv[0], v.px);

            maxv[1] =
                std::max(maxv[1], v.py);

            maxv[2] =
                std::max(maxv[2], v.pz);
        }

        const GlbMaterial* material =
            nullptr;

        if (primitive.material_index >= 0 &&
            (std::size_t)primitive.material_index <
                model.materials.size()) {

            material =
                &model.materials[
                    (std::size_t)
                    primitive.material_index
                ];
        }

        GpuMesh gpu_part;

        if (!upload_mesh(
                gpu_part,
                vertices,
                primitive.indices,
                material)) {

            destroy_model_parts(renderer);

            renderer.status =
                "ERROR GPU UPLOAD";

            return false;
        }

        if (gpu_part.has_texture)
            ++textures;

        /*
            Preserve raw FBX bind-pose data for CPU animation.
            GLB currently has no bind_vertices/node animation data.
        */
        gpu_part.bind_vertices =
            !primitive.bind_vertices.empty()
                ? primitive.bind_vertices
                : primitive.vertices;

        gpu_part.animated_vertices =
            primitive.vertices;

        gpu_part.bones =
            primitive.bones;

        gpu_part.node_index =
            primitive.node_index;

        gpu_part.cpu_animated =
            primitive.node_index >= 0 &&
            (
                !primitive.bind_vertices.empty() ||
                !primitive.bones.empty()
            );

        total_vertices +=
            primitive.vertices.size();

        total_indices +=
            primitive.indices.size();

        renderer.model_parts.push_back(
            std::move(gpu_part)
        );
    }

    if (renderer.model_parts.empty()) {
        renderer.status =
            "ERROR NO DRAWABLE PARTS";

        return false;
    }

    renderer.model_center[0] =
        (minv[0] + maxv[0]) * 0.5f;

    renderer.model_center[1] =
        (minv[1] + maxv[1]) * 0.5f;

    renderer.model_center[2] =
        (minv[2] + maxv[2]) * 0.5f;

    const float ex =
        (maxv[0] - minv[0]) * 0.5f;

    const float ey =
        (maxv[1] - minv[1]) * 0.5f;

    const float ez =
        (maxv[2] - minv[2]) * 0.5f;

    renderer.model_radius =
        std::sqrt(
            ex*ex +
            ey*ey +
            ez*ez
        );

    if (renderer.model_radius < 0.0001f)
        renderer.model_radius = 1.0f;

    renderer.show_model = true;
    renderer.loaded_file = path;

    /*
        New model starts from the physics origin.
    */
    renderer_physics_reset(renderer);

    renderer.animation_nodes =
        model.nodes;

    renderer.animation_clips =
        model.animations;

    std::memcpy(
        renderer.animation_global_inverse,
        model.global_inverse_transform,
        sizeof(
            renderer.animation_global_inverse
        )
    );

    renderer.animation_count =
        (int)renderer.animation_clips.size();

    renderer.animation_index = 0;
    renderer.animation_time = 0.0f;

    /*
        Start automatically when the FBX contains animation.
    */
    renderer.animation_playing =
        renderer.animation_count > 0;

    /*
        Resolve bone names to FBX node indices once at load time.
    */
    int missing_bone_nodes = 0;
    int total_bone_nodes = 0;

    for (GpuMesh& part :
         renderer.model_parts) {

        part.bone_node_indices.clear();
        part.bone_node_indices.reserve(
            part.bones.size()
        );

        for (const ModelBone& bone :
             part.bones) {

            const int node_index =
                find_node_index(
                    renderer.animation_nodes,
                    bone.name
                );

            part.bone_node_indices.push_back(
                node_index
            );

            ++total_bone_nodes;

            if (node_index < 0) {
                ++missing_bone_nodes;

                std::printf(
                    "FBX WARNING: bone node not found: %s\n",
                    bone.name.c_str()
                );
            }
        }
    }

    std::printf(
        "FBX bone mapping: total=%d missing=%d\n",
        total_bone_nodes,
        missing_bone_nodes
    );

    if (!renderer.animation_clips.empty()) {
        const ModelAnimationClip& debug_clip =
            renderer.animation_clips[0];

        int helper_channels = 0;

        for (const ModelAnimationChannel& channel :
             debug_clip.channels) {

            if (channel.node_name.find(
                    "_$AssimpFbx$_") !=
                std::string::npos) {
                ++helper_channels;
            }
        }

        std::printf(
            "Mixamo/FBX debug: clip=%s channels=%u helper_channels=%d duration=%.3fs tps=%.3f\n",
            debug_clip.name.c_str(),
            (unsigned)debug_clip.channels.size(),
            helper_channels,
            debug_clip.duration_seconds,
            debug_clip.ticks_per_second
        );
    }

    if (renderer.animation_count > 0) {
        update_animation_pose(renderer);
    }

    renderer.status =
        "MODEL PARTS " +
        std::to_string(
            renderer.model_parts.size()
        ) +
        " MAT " +
        std::to_string(
            model.materials.size()
        ) +
        " TEX " +
        std::to_string(textures) +
        " ANIM " +
        std::to_string(
            model.animations.size()
        );

    if (!renderer.animation_clips.empty()) {
        renderer.status +=
            " PLAY " +
            renderer.animation_clips[0].name;
    }

    std::printf(
        "MODEL loaded:\n"
        "  primitives: %u\n"
        "  materials : %u\n"
        "  textures  : %u\n"
        "  vertices  : %u\n"
        "  indices   : %u\n"
        "  animations: %u\n",
        (unsigned)
            renderer.model_parts.size(),
        (unsigned)
            model.materials.size(),
        (unsigned)textures,
        (unsigned)total_vertices,
        (unsigned)total_indices,
        (unsigned)model.animations.size()
    );

    return true;
}

void renderer_toggle_texture(
    Renderer& renderer
)
{
    renderer.texture_enabled =
        !renderer.texture_enabled;

    renderer.status =
        renderer.texture_enabled
            ? "TEXTURE ON"
            : "TEXTURE OFF";
}

void renderer_toggle_wireframe(
    Renderer& renderer
)
{
    renderer.wireframe =
        !renderer.wireframe;

    renderer.status =
        renderer.wireframe
            ? "WIREFRAME ON"
            : "WIREFRAME OFF";
}

void renderer_toggle_cube_model(
    Renderer& renderer
)
{
    if (renderer.model_parts.empty()) {
        renderer.status =
            "LOAD MODEL FIRST";

        return;
    }

    renderer.show_model =
        !renderer.show_model;

    renderer.status =
        renderer.show_model
            ? "SHOW MODEL"
            : "SHOW CUBE";
}

static void draw_mesh(
    Renderer& renderer,
    const GpuMesh& mesh,
    const Mat4& mvp
)
{
    gl33UseProgram(
        renderer.program
    );

    gl33UniformMatrix4fv(
        renderer.u_mvp,
        1,
        GL_FALSE,
        mvp.m
    );

    gl33Uniform4f(
        renderer.u_base_color,
        mesh.base_color_factor[0],
        mesh.base_color_factor[1],
        mesh.base_color_factor[2],
        mesh.base_color_factor[3]
    );

    gl33Uniform1f(
        renderer.u_metallic,
        mesh.metallic_factor
    );

    gl33Uniform1f(
        renderer.u_roughness,
        mesh.roughness_factor
    );

    const bool use_texture =
        renderer.texture_enabled &&
        mesh.has_texture &&
        mesh.texture != 0;

    gl33Uniform1i(
        renderer.u_use_texture,
        use_texture ? 1 : 0
    );

    gl33ActiveTexture(
        GL_TEXTURE0
    );

    glBindTexture(
        GL_TEXTURE_2D,
        use_texture
            ? mesh.texture
            : 0
    );

    gl33Uniform1i(
        renderer.u_texture,
        0
    );

    gl33BindVertexArray(
        mesh.vao
    );

    glDrawElements(
        GL_TRIANGLES,
        mesh.index_count,
        GL_UNSIGNED_INT,
        nullptr
    );

    gl33BindVertexArray(0);
}

static void build_menu(
    Renderer& renderer,
    int width,
    int height
)
{
    (void)height;

    ui_begin(renderer.ui);

    ui_rect(
        renderer.ui,
        0,
        0,
        (float)width,
        70,
        0.08f,
        0.09f,
        0.11f
    );

    struct Button {
        float x;
        float w;
        const char* text;
        bool active;
    };

    Button buttons[] = {
        {
            10,
            145,
            "F1 OPEN MODEL",
            false
        },
        {
            165,
            145,
            "F2 TEXTURE",
            renderer.texture_enabled
        },
        {
            320,
            155,
            "F3 WIREFRAME",
            renderer.wireframe
        },
        {
            485,
            150,
            "F4 CUBE MODEL",
            renderer.show_model
        }
    };

    for (const Button& button :
         buttons) {

        const float b =
            button.active
                ? 0.34f
                : 0.20f;

        ui_rect(
            renderer.ui,
            button.x,
            10,
            button.w,
            32,
            b,
            b,
            b + 0.05f
        );

        ui_text(
            renderer.ui,
            button.text,
            button.x + 8,
            20,
            2.0f,
            0.95f,
            0.95f,
            0.95f
        );
    }

    std::string status =
        renderer.status;

    if (status.size() > 75)
        status.resize(75);

    ui_text(
        renderer.ui,
        status,
        12,
        52,
        1.5f,
        0.65f,
        0.85f,
        0.65f
    );
}

void renderer_draw(
    Renderer& renderer,
    int width,
    int height,
    float time_seconds
)
{
    if (width < 1)
        width = 1;

    if (height < 1)
        height = 1;

    glViewport(
        0,
        0,
        width,
        height
    );

    /*
        Brighter outdoor background.
    */
    glClearColor(
        0.14f,
        0.20f,
        0.28f,
        1.0f
    );

    glClear(
        GL_COLOR_BUFFER_BIT |
        GL_DEPTH_BUFFER_BIT
    );

    glPolygonMode(
        GL_FRONT_AND_BACK,
        renderer.wireframe
            ? GL_LINE
            : GL_FILL
    );

    /*
        Apply the current FBX animation pose before drawing.
        animation_time advances in engine.cpp while Play is enabled.
    */
    if (renderer.show_model &&
        !renderer.animation_clips.empty()) {
        update_animation_pose(renderer);
    }

    const float pi =
        3.14159265358979323846f;

    Mat4 projection =
        mat4_perspective(
            60.0f * pi / 180.0f,
            (float)width /
                (float)height,
            0.1f,
            200.0f
        );

    float center[3] = {0,0,0};
    float radius = 1.0f;

    if (renderer.show_model &&
        !renderer.model_parts.empty()) {

        center[0] =
            renderer.model_center[0];

        center[1] =
            renderer.model_center[1];

        center[2] =
            renderer.model_center[2];

        radius =
            renderer.model_radius;
    } else {
        center[0] =
            renderer.cube.center[0];

        center[1] =
            renderer.cube.center[1];

        center[2] =
            renderer.cube.center[2];

        radius =
            renderer.cube.radius;
    }

    /*
        THIRD-PERSON FOLLOW CAMERA - DECOUPLED ROTATION

        Follow player X/Y/Z, but DO NOT inherit body_yaw.

        Result:
            A/D or Left/Right = rotate only character
            RMB drag          = rotate only camera

        This removes the old "character and camera turn together"
        feedback loop.
    */
    const float chase_pi =
        3.14159265358979323846f;

    const float chase_yaw =
        chase_pi +
        renderer.orbit_yaw;

    const float target_x =
        renderer.show_model
            ? renderer.collider.body_x
            : 0.0f;

    const float target_y =
        renderer.show_model
            ? renderer.collider.body_y +
                renderer.camera_target_height
            : 0.0f;

    const float target_z =
        renderer.show_model
            ? renderer.collider.body_z
            : 0.0f;

    Mat4 camera_translate =
        mat4_translation(
            0.0f,
            0.0f,
            -renderer.orbit_distance
        );

    Mat4 camera_pitch =
        mat4_rotation_x(
            renderer.orbit_pitch
        );

    Mat4 camera_yaw =
        mat4_rotation_y(
            chase_yaw
        );

    Mat4 camera_target =
        mat4_translation(
            -target_x,
            -target_y,
            -target_z
        );

    Mat4 view =
        mat4_multiply(
            camera_translate,
            camera_pitch
        );

    view =
        mat4_multiply(
            view,
            camera_yaw
        );

    view =
        mat4_multiply(
            view,
            camera_target
        );

    /*
        Keep the automatic cube rotation, but loaded models are
        controlled only by the orbit camera.
    */
    Mat4 object_rx =
        renderer.show_model
            ? mat4_identity()
            : mat4_rotation_x(
                time_seconds * 0.25f
            );

    Mat4 object_ry =
        renderer.show_model
            ? mat4_identity()
            : mat4_rotation_y(
                time_seconds * 0.55f
            );

    const float fit_scale =
        1.75f / radius;

    Mat4 scale =
        mat4_scale(
            fit_scale,
            fit_scale,
            fit_scale
        );

    Mat4 center_matrix =
        mat4_translation(
            -center[0],
            -center[1],
            -center[2]
        );

    Mat4 model =
        mat4_multiply(
            object_ry,
            object_rx
        );

    model =
        mat4_multiply(
            model,
            scale
        );

    model =
        mat4_multiply(
            model,
            center_matrix
        );

    /*
        Character/world transform:
        - skeletal animation stays local;
        - Shenmue controls rotate/move the whole body;
        - gravity supplies body_y.
    */
    Mat4 character_rotation =
        mat4_rotation_y(
            renderer.collider.body_yaw
        );

    Mat4 character_translation =
        mat4_translation(
            renderer.collider.body_x,
            renderer.collider.body_y,
            renderer.collider.body_z
        );

    model =
        mat4_multiply(
            character_rotation,
            model
        );

    model =
        mat4_multiply(
            character_translation,
            model
        );

    Mat4 view_model =
        mat4_multiply(
            view,
            model
        );

    Mat4 mvp =
        mat4_multiply(
            projection,
            view_model
        );

    /*
        100x100 green ground plane.
        It shares the same ground_y used by collision.
    */
    if (renderer.show_ground_plane) {
        Mat4 ground_translate =
            mat4_translation(
                0.0f,
                renderer.collider.ground_y,
                0.0f
            );

        Mat4 ground_scale =
            mat4_scale(
                100.0f,
                1.0f,
                100.0f
            );

        Mat4 ground_model =
            mat4_multiply(
                ground_translate,
                ground_scale
            );

        Mat4 ground_view_model =
            mat4_multiply(
                view,
                ground_model
            );

        Mat4 ground_mvp =
            mat4_multiply(
                projection,
                ground_view_model
            );

        glPolygonMode(
            GL_FRONT_AND_BACK,
            GL_FILL
        );

        draw_mesh(
            renderer,
            renderer.ground_plane,
            ground_mvp
        );

        /*
            Restore the user's requested model mode.
        */
        glPolygonMode(
            GL_FRONT_AND_BACK,
            renderer.wireframe
                ? GL_LINE
                : GL_FILL
        );
    }

    if (renderer.show_model &&
        !renderer.model_parts.empty()) {

        for (const GpuMesh& part :
             renderer.model_parts) {

            draw_mesh(
                renderer,
                part,
                mvp
            );
        }
    } else {
        draw_mesh(
            renderer,
            renderer.cube,
            mvp
        );
    }

    /*
        WAYPOINT AI MODEL INSTANCE

        Reuses the exact same loaded model_parts/VBOs.
        No second copy of the FBX geometry is uploaded.
    */
    if (renderer.show_model &&
        renderer.ai_agent.enabled &&
        !renderer.model_parts.empty()) {

        /*
            Rebuild local model transform before player world transform.
        */
        Mat4 ai_model =
            mat4_multiply(
                object_ry,
                object_rx
            );

        ai_model =
            mat4_multiply(
                ai_model,
                scale
            );

        ai_model =
            mat4_multiply(
                ai_model,
                center_matrix
            );

        Mat4 ai_rotation =
            mat4_rotation_y(
                renderer.ai_agent.yaw
            );

        /*
            Put AI on the same ground plane according to collider size.
        */
        const float ai_y =
            renderer.collider.ground_y +
            renderer.collider.size_y * 0.5f -
            renderer.collider.offset_y;

        Mat4 ai_translation =
            mat4_translation(
                renderer.ai_agent.x,
                ai_y,
                renderer.ai_agent.z
            );

        ai_model =
            mat4_multiply(
                ai_rotation,
                ai_model
            );

        ai_model =
            mat4_multiply(
                ai_translation,
                ai_model
            );

        Mat4 ai_view_model =
            mat4_multiply(
                view,
                ai_model
            );

        Mat4 ai_mvp =
            mat4_multiply(
                projection,
                ai_view_model
            );

        for (const GpuMesh& part :
             renderer.model_parts) {

            draw_mesh(
                renderer,
                part,
                ai_mvp
            );
        }
    }

    /*
        Waypoint visualization: yellow cubes on the 100x100 ground.
    */
    if (renderer.ai_agent.show_waypoints) {
        for (std::size_t i = 0;
             i < renderer.ai_waypoints.size();
             ++i) {

            const AiWaypoint& wp =
                renderer.ai_waypoints[i];

            Mat4 wp_translate =
                mat4_translation(
                    wp.x,
                    renderer.collider.ground_y +
                        0.12f,
                    wp.z
                );

            Mat4 wp_scale =
                mat4_scale(
                    0.18f,
                    0.12f,
                    0.18f
                );

            Mat4 wp_model =
                mat4_multiply(
                    wp_translate,
                    wp_scale
                );

            Mat4 wp_vm =
                mat4_multiply(
                    view,
                    wp_model
                );

            Mat4 wp_mvp =
                mat4_multiply(
                    projection,
                    wp_vm
                );

            float old_wp_color[4] = {
                renderer.cube.base_color_factor[0],
                renderer.cube.base_color_factor[1],
                renderer.cube.base_color_factor[2],
                renderer.cube.base_color_factor[3]
            };

            if ((int)i ==
                renderer.ai_agent.waypoint_index) {

                renderer.cube.base_color_factor[0] = 1.00f;
                renderer.cube.base_color_factor[1] = 0.28f;
                renderer.cube.base_color_factor[2] = 0.06f;
                renderer.cube.base_color_factor[3] = 1.00f;
            } else {
                renderer.cube.base_color_factor[0] = 1.00f;
                renderer.cube.base_color_factor[1] = 0.92f;
                renderer.cube.base_color_factor[2] = 0.08f;
                renderer.cube.base_color_factor[3] = 1.00f;
            }

            glPolygonMode(
                GL_FRONT_AND_BACK,
                GL_FILL
            );

            draw_mesh(
                renderer,
                renderer.cube,
                wp_mvp
            );

            for (int c = 0; c < 4; ++c) {
                renderer.cube.base_color_factor[c] =
                    old_wp_color[c];
            }
        }
    }

    /*
        Box collider visualization.
        renderer.cube is -1..+1, therefore scale by half extents.
    */
    if (renderer.show_model &&
        renderer.collider.show_collider) {

        /*
            Rotate collider offset with character heading so visual box
            stays attached to the character.
        */
        const float cy =
            std::cos(
                renderer.collider.body_yaw
            );

        const float sy =
            std::sin(
                renderer.collider.body_yaw
            );

        const float rotated_offset_x =
            renderer.collider.offset_x * cy -
            renderer.collider.offset_z * sy;

        const float rotated_offset_z =
            renderer.collider.offset_x * sy +
            renderer.collider.offset_z * cy;

        Mat4 collider_translate =
            mat4_translation(
                renderer.collider.body_x +
                    rotated_offset_x,
                renderer.collider.body_y +
                    renderer.collider.offset_y,
                renderer.collider.body_z +
                    rotated_offset_z
            );

        Mat4 collider_rotation =
            mat4_rotation_y(
                renderer.collider.body_yaw
            );

        Mat4 collider_scale =
            mat4_scale(
                renderer.collider.size_x * 0.5f,
                renderer.collider.size_y * 0.5f,
                renderer.collider.size_z * 0.5f
            );

        Mat4 collider_model =
            mat4_multiply(
                collider_rotation,
                collider_scale
            );

        collider_model =
            mat4_multiply(
                collider_translate,
                collider_model
            );

        Mat4 collider_view_model =
            mat4_multiply(
                view,
                collider_model
            );

        Mat4 collider_mvp =
            mat4_multiply(
                projection,
                collider_view_model
            );

        /*
            Force wireframe only for the collider.
        */
        glPolygonMode(
            GL_FRONT_AND_BACK,
            GL_LINE
        );

        /*
            Temporarily tint the shared cube mesh.
        */
        float old_color[4] = {
            renderer.cube.base_color_factor[0],
            renderer.cube.base_color_factor[1],
            renderer.cube.base_color_factor[2],
            renderer.cube.base_color_factor[3]
        };

        renderer.cube.base_color_factor[0] = 0.15f;
        renderer.cube.base_color_factor[1] = 1.00f;
        renderer.cube.base_color_factor[2] = 0.25f;
        renderer.cube.base_color_factor[3] = 1.00f;

        draw_mesh(
            renderer,
            renderer.cube,
            collider_mvp
        );

        for (int i = 0; i < 4; ++i) {
            renderer.cube.base_color_factor[i] =
                old_color[i];
        }
    }

    /*
        WEAPON / AMMO PICKUPS

        Orange spinning crate + yellow marker.
        Touching one restores +5 shots, up to 10.
    */
    for (std::size_t pickup_index = 0;
         pickup_index < renderer.ammo_pickups.size();
         ++pickup_index) {

        const AmmoPickupState& pickup =
            renderer.ammo_pickups[pickup_index];

        if (!pickup.active)
            continue;

        const float pickup_y =
            renderer.collider.ground_y +
            0.42f;

        const float spin =
            time_seconds * 1.8f +
            (float)pickup_index * 0.55f;

        Mat4 pickup_translate =
            mat4_translation(
                pickup.x,
                pickup_y,
                pickup.z
            );

        Mat4 pickup_rotate =
            mat4_rotation_y(
                spin
            );

        Mat4 pickup_scale =
            mat4_scale(
                0.42f,
                0.28f,
                0.58f
            );

        Mat4 pickup_model =
            mat4_multiply(
                pickup_rotate,
                pickup_scale
            );

        pickup_model =
            mat4_multiply(
                pickup_translate,
                pickup_model
            );

        Mat4 pickup_vm =
            mat4_multiply(
                view,
                pickup_model
            );

        Mat4 pickup_mvp =
            mat4_multiply(
                projection,
                pickup_vm
            );

        float old_pickup_color[4] = {
            renderer.cube.base_color_factor[0],
            renderer.cube.base_color_factor[1],
            renderer.cube.base_color_factor[2],
            renderer.cube.base_color_factor[3]
        };

        renderer.cube.base_color_factor[0] = 1.00f;
        renderer.cube.base_color_factor[1] = 0.32f;
        renderer.cube.base_color_factor[2] = 0.06f;
        renderer.cube.base_color_factor[3] = 1.00f;

        glPolygonMode(
            GL_FRONT_AND_BACK,
            GL_FILL
        );

        draw_mesh(
            renderer,
            renderer.cube,
            pickup_mvp
        );

        Mat4 marker_translate =
            mat4_translation(
                pickup.x,
                pickup_y + 0.42f,
                pickup.z
            );

        Mat4 marker_scale =
            mat4_scale(
                0.11f,
                0.34f,
                0.11f
            );

        Mat4 marker_model =
            mat4_multiply(
                pickup_rotate,
                marker_scale
            );

        marker_model =
            mat4_multiply(
                marker_translate,
                marker_model
            );

        Mat4 marker_vm =
            mat4_multiply(
                view,
                marker_model
            );

        Mat4 marker_mvp =
            mat4_multiply(
                projection,
                marker_vm
            );

        renderer.cube.base_color_factor[0] = 1.00f;
        renderer.cube.base_color_factor[1] = 0.92f;
        renderer.cube.base_color_factor[2] = 0.08f;
        renderer.cube.base_color_factor[3] = 1.00f;

        draw_mesh(
            renderer,
            renderer.cube,
            marker_mvp
        );

        for (int c = 0; c < 4; ++c) {
            renderer.cube.base_color_factor[c] =
                old_pickup_color[c];
        }
    }

    /*
        Direction cube:
        a small cube in front of the Box Collider.
        It behaves like a simple "which way am I facing?" marker,
        useful for Shenmue/tank controls.
    */
    if (renderer.show_model) {
        const float yaw =
            renderer.collider.body_yaw;

        const float dir_x =
            std::sin(yaw);

        const float dir_z =
            std::cos(yaw);

        const float front_distance =
            renderer.collider.size_z *
                0.5f +
            0.28f;

        const float marker_x =
            renderer.collider.body_x +
            renderer.collider.offset_x +
            dir_x *
            front_distance;

        const float marker_y =
            renderer.collider.body_y +
            renderer.collider.offset_y +
            0.10f;

        const float marker_z =
            renderer.collider.body_z +
            renderer.collider.offset_z +
            dir_z *
            front_distance;

        Mat4 marker_translate =
            mat4_translation(
                marker_x,
                marker_y,
                marker_z
            );

        Mat4 marker_rotate =
            mat4_rotation_y(
                yaw
            );

        Mat4 marker_scale =
            mat4_scale(
                0.12f,
                0.12f,
                0.28f
            );

        Mat4 marker_model =
            mat4_multiply(
                marker_rotate,
                marker_scale
            );

        marker_model =
            mat4_multiply(
                marker_translate,
                marker_model
            );

        Mat4 marker_view_model =
            mat4_multiply(
                view,
                marker_model
            );

        Mat4 marker_mvp =
            mat4_multiply(
                projection,
                marker_view_model
            );

        float old_marker_color[4] = {
            renderer.cube.base_color_factor[0],
            renderer.cube.base_color_factor[1],
            renderer.cube.base_color_factor[2],
            renderer.cube.base_color_factor[3]
        };

        renderer.cube.base_color_factor[0] = 0.10f;
        renderer.cube.base_color_factor[1] = 0.45f;
        renderer.cube.base_color_factor[2] = 1.00f;
        renderer.cube.base_color_factor[3] = 1.00f;

        glPolygonMode(
            GL_FRONT_AND_BACK,
            GL_FILL
        );

        draw_mesh(
            renderer,
            renderer.cube,
            marker_mvp
        );

        for (int i = 0; i < 4; ++i) {
            renderer.cube.base_color_factor[i] =
                old_marker_color[i];
        }
    }

    /*
        Draw all active projectile spheres in world space.
    */
    glPolygonMode(
        GL_FRONT_AND_BACK,
        GL_FILL
    );

    for (const ProjectileState& projectile :
         renderer.projectiles) {

        if (!projectile.active)
            continue;

        Mat4 projectile_translate =
            mat4_translation(
                projectile.x,
                projectile.y,
                projectile.z
            );

        Mat4 projectile_scale =
            mat4_scale(
                projectile.radius,
                projectile.radius,
                projectile.radius
            );

        Mat4 projectile_model =
            mat4_multiply(
                projectile_translate,
                projectile_scale
            );

        Mat4 projectile_view_model =
            mat4_multiply(
                view,
                projectile_model
            );

        Mat4 projectile_mvp =
            mat4_multiply(
                projection,
                projectile_view_model
            );

        /*
            Player balls = yellow/orange.
            AI hostile shots = red.
        */
        float old_projectile_color[4] = {
            renderer.projectile_sphere.base_color_factor[0],
            renderer.projectile_sphere.base_color_factor[1],
            renderer.projectile_sphere.base_color_factor[2],
            renderer.projectile_sphere.base_color_factor[3]
        };

        if (projectile.hostile) {
            renderer.projectile_sphere.base_color_factor[0] = 1.00f;
            renderer.projectile_sphere.base_color_factor[1] = 0.08f;
            renderer.projectile_sphere.base_color_factor[2] = 0.04f;
            renderer.projectile_sphere.base_color_factor[3] = 1.00f;
        } else {
            renderer.projectile_sphere.base_color_factor[0] = 1.00f;
            renderer.projectile_sphere.base_color_factor[1] = 0.72f;
            renderer.projectile_sphere.base_color_factor[2] = 0.10f;
            renderer.projectile_sphere.base_color_factor[3] = 1.00f;
        }

        draw_mesh(
            renderer,
            renderer.projectile_sphere,
            projectile_mvp
        );

        for (int c = 0; c < 4; ++c) {
            renderer.projectile_sphere.base_color_factor[c] =
                old_projectile_color[c];
        }
    }

    glPolygonMode(
        GL_FRONT_AND_BACK,
        GL_FILL
    );

    build_menu(
        renderer,
        width,
        height
    );

    ui_draw(
        renderer.ui,
        width,
        height
    );
}

void renderer_menu_click(
    Renderer& renderer,
    int x,
    int y,
    void (*open_file_callback)()
)
{
    if (y < 10 ||
        y > 42) {
        return;
    }

    if (x >= 10 &&
        x < 155) {

        open_file_callback();

    } else if (
        x >= 165 &&
        x < 310) {

        renderer_toggle_texture(
            renderer
        );

    } else if (
        x >= 320 &&
        x < 475) {

        renderer_toggle_wireframe(
            renderer
        );

    } else if (
        x >= 485 &&
        x < 635) {

        renderer_toggle_cube_model(
            renderer
        );
    }
}


void renderer_animation_play_pause(
    Renderer& renderer
)
{
    if (renderer.animation_count <= 0) {
        renderer.animation_playing = false;
        renderer.status = "NO ANIMATION";
        return;
    }

    renderer.animation_playing =
        !renderer.animation_playing;

    const std::string clip_name =
        (
            renderer.animation_index >= 0 &&
            (std::size_t)renderer.animation_index <
                renderer.animation_clips.size()
        )
        ? renderer.animation_clips[
            (std::size_t)
            renderer.animation_index
          ].name
        : std::string("ANIMATION");

    renderer.status =
        (
            renderer.animation_playing
                ? "PLAY "
                : "PAUSE "
        ) +
        clip_name;
}

void renderer_animation_next(
    Renderer& renderer
)
{
    if (renderer.animation_count <= 0) {
        renderer.status = "NO ANIMATION";
        return;
    }

    renderer.animation_index =
        (renderer.animation_index + 1) %
        renderer.animation_count;

    renderer.animation_time = 0.0f;
    renderer.animation_playing = true;

    update_animation_pose(renderer);

    renderer.status =
        "PLAY " +
        renderer.animation_clips[
            (std::size_t)
            renderer.animation_index
        ].name;
}

void renderer_animation_previous(
    Renderer& renderer
)
{
    if (renderer.animation_count <= 0) {
        renderer.status = "NO ANIMATION";
        return;
    }

    renderer.animation_index -= 1;

    if (renderer.animation_index < 0)
        renderer.animation_index =
            renderer.animation_count - 1;

    renderer.animation_time = 0.0f;
    renderer.animation_playing = true;

    update_animation_pose(renderer);

    renderer.status =
        "PLAY " +
        renderer.animation_clips[
            (std::size_t)
            renderer.animation_index
        ].name;
}

void renderer_animation_set_index(
    Renderer& renderer,
    int index
)
{
    if (renderer.animation_count <= 0) {
        renderer.animation_index = 0;
        renderer.status = "NO ANIMATION";
        return;
    }

    if (index < 0)
        index = 0;

    if (index >= renderer.animation_count)
        index = renderer.animation_count - 1;

    renderer.animation_index = index;
    renderer.animation_time = 0.0f;

    update_animation_pose(renderer);

    renderer.status =
        "ANIMATION " +
        renderer.animation_clips[
            (std::size_t)
            renderer.animation_index
        ].name;
}

void renderer_animation_set_speed(
    Renderer& renderer,
    float speed
)
{
    if (speed < 0.0f)
        speed = 0.0f;

    if (speed > 10.0f)
        speed = 10.0f;

    renderer.animation_speed = speed;

    renderer.status =
        "ANIM SPEED " +
        std::to_string(speed);
}

void renderer_animation_set_time(
    Renderer& renderer,
    float time_seconds
)
{
    if (time_seconds < 0.0f)
        time_seconds = 0.0f;

    renderer.animation_time = time_seconds;

    update_animation_pose(renderer);

    renderer.status =
        "ANIM TIME " +
        std::to_string(time_seconds);
}




void renderer_set_box_collider(
    Renderer& renderer,
    float size_x,
    float size_y,
    float size_z,
    float offset_x,
    float offset_y,
    float offset_z,
    bool gravity_enabled,
    bool show_collider
)
{
    const float min_size = 0.01f;
    const float max_size = 100.0f;

    renderer.collider.size_x =
        std::max(
            min_size,
            std::min(max_size, size_x)
        );

    renderer.collider.size_y =
        std::max(
            min_size,
            std::min(max_size, size_y)
        );

    renderer.collider.size_z =
        std::max(
            min_size,
            std::min(max_size, size_z)
        );

    renderer.collider.offset_x =
        offset_x;

    renderer.collider.offset_y =
        offset_y;

    renderer.collider.offset_z =
        offset_z;

    renderer.collider.gravity_enabled =
        gravity_enabled;

    renderer.collider.show_collider =
        show_collider;

    /*
        Re-resolve penetration immediately when dimensions/offset change.
    */
    const float bottom =
        renderer.collider.body_y +
        renderer.collider.offset_y -
        renderer.collider.size_y * 0.5f;

    if (bottom <
        renderer.collider.ground_y) {

        renderer.collider.body_y +=
            renderer.collider.ground_y -
            bottom;

        renderer.collider.vertical_velocity =
            0.0f;

        renderer.collider.grounded = true;
    }

    renderer.status =
        gravity_enabled
            ? "BOX COLLIDER - GRAVITY ON"
            : "BOX COLLIDER - GRAVITY OFF";
}

void renderer_physics_step(
    Renderer& renderer,
    float dt
)
{
    /*
        Prevent a debugger pause or window drag from causing one huge
        integration step.
    */
    if (dt < 0.0f)
        dt = 0.0f;

    if (dt > 0.05f)
        dt = 0.05f;

    /*
        Character/body gravity is optional.
    */
    if (renderer.collider.gravity_enabled) {
        const float gravity =
            -9.81f;

        renderer.collider.vertical_velocity +=
            gravity * dt;

        renderer.collider.body_y +=
            renderer.collider.vertical_velocity *
            dt;

        const float half_height =
            renderer.collider.size_y *
            0.5f;

        const float bottom =
            renderer.collider.body_y +
            renderer.collider.offset_y -
            half_height;

        if (bottom <=
            renderer.collider.ground_y) {

            renderer.collider.body_y +=
                renderer.collider.ground_y -
                bottom;

            if (renderer.collider.vertical_velocity <
                0.0f) {

                renderer.collider.vertical_velocity =
                    0.0f;
            }

            renderer.collider.grounded =
                true;
        } else {
            renderer.collider.grounded =
                false;
        }
    }

    /*
        Projectile physics always runs.
        Bolinhas have their own gravity and bounce on the 10x10 plane.
    */
    const float projectile_gravity =
        -9.81f;

    for (ProjectileState& projectile :
         renderer.projectiles) {

        if (!projectile.active)
            continue;

        projectile.lifetime +=
            dt;

        projectile.vy +=
            projectile_gravity *
            projectile.gravity_scale *
            dt;

        projectile.x +=
            projectile.vx *
            dt;

        projectile.y +=
            projectile.vy *
            dt;

        projectile.z +=
            projectile.vz *
            dt;

        /*
            AI shot -> player Box Collider hit test.
            The collision uses an AABB approximation around the
            player's configured collider.
        */
        if (projectile.hostile) {
            const float player_yaw =
                renderer.collider.body_yaw;

            const float cy =
                std::cos(player_yaw);

            const float sy =
                std::sin(player_yaw);

            const float rotated_offset_x =
                renderer.collider.offset_x * cy -
                renderer.collider.offset_z * sy;

            const float rotated_offset_z =
                renderer.collider.offset_x * sy +
                renderer.collider.offset_z * cy;

            const float center_x =
                renderer.collider.body_x +
                rotated_offset_x;

            const float center_y =
                renderer.collider.body_y +
                renderer.collider.offset_y;

            const float center_z =
                renderer.collider.body_z +
                rotated_offset_z;

            const float half_x =
                renderer.collider.size_x * 0.5f +
                projectile.radius;

            const float half_y =
                renderer.collider.size_y * 0.5f +
                projectile.radius;

            const float half_z =
                renderer.collider.size_z * 0.5f +
                projectile.radius;

            if (std::fabs(
                    projectile.x -
                    center_x) <= half_x &&
                std::fabs(
                    projectile.y -
                    center_y) <= half_y &&
                std::fabs(
                    projectile.z -
                    center_z) <= half_z) {

                renderer.player_health =
                    std::max(
                        0,
                        renderer.player_health - 10
                    );

                projectile.active =
                    false;

                renderer.status =
                    "AI HIT PLAYER - HP " +
                    std::to_string(
                        renderer.player_health
                    ) +
                    "/" +
                    std::to_string(
                        renderer.player_max_health
                    );

                continue;
            }
        }

        const bool inside_plane_x =
            projectile.x >= -50.0f &&
            projectile.x <=  50.0f;

        const bool inside_plane_z =
            projectile.z >= -50.0f &&
            projectile.z <=  50.0f;

        const float bottom =
            projectile.y -
            projectile.radius;

        if (!projectile.hostile &&
            inside_plane_x &&
            inside_plane_z &&
            bottom <=
                renderer.collider.ground_y &&
            projectile.vy < 0.0f) {

            projectile.y =
                renderer.collider.ground_y +
                projectile.radius;

            /*
                Small bounce + friction.
            */
            projectile.vy =
                -projectile.vy *
                0.45f;

            projectile.vx *=
                0.82f;

            projectile.vz *=
                0.82f;

            if (std::fabs(
                    projectile.vy) <
                0.35f) {

                projectile.vy =
                    0.0f;
            }
        }

        if (projectile.lifetime >
                8.0f ||
            projectile.y <
                renderer.collider.ground_y -
                25.0f) {

            projectile.active =
                false;
        }
    }

    /*
        Auto-pickup ammo item on contact.
        Full magazine does not consume an item.
    */
    if (renderer.ammo <
        renderer.max_ammo) {

        const float pickup_radius =
            1.15f;

        const float pickup_radius_sq =
            pickup_radius *
            pickup_radius;

        for (AmmoPickupState& pickup :
             renderer.ammo_pickups) {

            if (!pickup.active)
                continue;

            const float dx =
                renderer.collider.body_x -
                pickup.x;

            const float dz =
                renderer.collider.body_z -
                pickup.z;

            const float distance_sq =
                dx*dx +
                dz*dz;

            if (distance_sq <=
                pickup_radius_sq) {

                renderer.ammo =
                    std::min(
                        renderer.max_ammo,
                        renderer.ammo +
                            pickup.ammo_amount
                    );

                pickup.active =
                    false;

                renderer.status =
                    "AMMO PICKUP +5  (" +
                    std::to_string(
                        renderer.ammo
                    ) +
                    "/" +
                    std::to_string(
                        renderer.max_ammo
                    ) +
                    ")";

                break;
            }
        }
    }

    /*
        Compact inactive projectiles to keep the vector small.
    */
    renderer.projectiles.erase(
        std::remove_if(
            renderer.projectiles.begin(),
            renderer.projectiles.end(),
            [](
                const ProjectileState& projectile
            ) {
                return !projectile.active;
            }
        ),
        renderer.projectiles.end()
    );
}

void renderer_physics_reset(
    Renderer& renderer
)
{
    renderer.collider.body_x =
        0.0f;

    renderer.collider.body_y =
        0.0f;

    renderer.collider.body_z =
        0.0f;

    renderer.collider.body_yaw =
        0.0f;

    renderer.collider.vertical_velocity =
        0.0f;

    renderer.collider.grounded =
        false;

    renderer.player_health =
        renderer.player_max_health;

    renderer.projectiles.clear();

    renderer.status =
        "PHYSICS RESET - HP 100";
}



void renderer_move_character(
    Renderer& renderer,
    float forward_axis,
    float turn_axis,
    float dt
)
{
    if (dt <= 0.0f)
        return;

    if (dt > 0.05f)
        dt = 0.05f;

    /*
        Shenmue-style tank controls:
        left/right rotates character in place,
        up/down moves along current facing.
    */
    renderer.collider.body_yaw +=
        turn_axis *
        renderer.collider.turn_speed *
        dt;

    const float pi =
        3.14159265358979323846f;

    const float two_pi =
        pi * 2.0f;

    while (renderer.collider.body_yaw >
           pi) {

        renderer.collider.body_yaw -=
            two_pi;
    }

    while (renderer.collider.body_yaw <
           -pi) {

        renderer.collider.body_yaw +=
            two_pi;
    }

    if (std::fabs(forward_axis) <
        0.0001f) {
        return;
    }

    const float speed =
        forward_axis >= 0.0f
            ? renderer.collider.move_speed
            : renderer.collider.backward_speed;

    const float signed_speed =
        speed *
        forward_axis;

    /*
        yaw=0 means the character faces +Z.
        This matches the visible front of the loaded Mixamo model.
    */
    const float forward_x =
        std::sin(
            renderer.collider.body_yaw
        );

    const float forward_z =
        std::cos(
            renderer.collider.body_yaw
        );

    renderer.collider.body_x +=
        forward_x *
        signed_speed *
        dt;

    renderer.collider.body_z +=
        forward_z *
        signed_speed *
        dt;

    /*
        Keep character inside the 100x100 map with a small border.
    */
    renderer.collider.body_x =
        std::max(
            -49.0f,
            std::min(
                49.0f,
                renderer.collider.body_x
            )
        );

    renderer.collider.body_z =
        std::max(
            -49.0f,
            std::min(
                49.0f,
                renderer.collider.body_z
            )
        );
}


static float wrap_angle_pi(
    float angle
)
{
    const float pi =
        3.14159265358979323846f;

    const float two_pi =
        pi * 2.0f;

    while (angle > pi)
        angle -= two_pi;

    while (angle < -pi)
        angle += two_pi;

    return angle;
}

void renderer_set_ai_options(
    Renderer& renderer,
    bool enabled,
    bool show_waypoints,
    float speed
)
{
    renderer.ai_agent.enabled =
        enabled;

    renderer.ai_agent.show_waypoints =
        show_waypoints;

    renderer.ai_agent.speed =
        std::max(
            0.1f,
            std::min(
                20.0f,
                speed
            )
        );

    renderer.status =
        enabled
            ? "WAYPOINT AI ENABLED"
            : "WAYPOINT AI DISABLED";
}

void renderer_reset_ai(
    Renderer& renderer
)
{
    if (renderer.ai_waypoints.empty()) {
        renderer.ai_agent.x = -15.0f;
        renderer.ai_agent.z = -15.0f;
        renderer.ai_agent.yaw = 0.0f;
        renderer.ai_agent.waypoint_index = 0;
        return;
    }

    renderer.ai_agent.x =
        renderer.ai_waypoints[0].x;

    renderer.ai_agent.z =
        renderer.ai_waypoints[0].z;

    renderer.ai_agent.yaw =
        0.0f;

    renderer.ai_agent.attacking =
        false;

    renderer.ai_agent.fire_timer =
        0.0f;

    renderer.ai_agent.waypoint_index =
        renderer.ai_waypoints.size() > 1
            ? 1
            : 0;

    renderer.status =
        "WAYPOINT AI RESET";
}


static void renderer_fire_ai_projectile(
    Renderer& renderer
)
{
    const std::size_t max_projectiles =
        128;

    if (renderer.projectiles.size() >=
        max_projectiles) {

        renderer.projectiles.erase(
            renderer.projectiles.begin()
        );
    }

    /*
        AI center, standing on the same ground plane as the player.
    */
    const float ai_center_x =
        renderer.ai_agent.x;

    const float ai_center_y =
        renderer.collider.ground_y +
        renderer.collider.size_y *
            0.5f;

    const float ai_center_z =
        renderer.ai_agent.z;

    /*
        Target the center of the player's box collider.
    */
    const float player_yaw =
        renderer.collider.body_yaw;

    const float cy =
        std::cos(player_yaw);

    const float sy =
        std::sin(player_yaw);

    const float rotated_offset_x =
        renderer.collider.offset_x * cy -
        renderer.collider.offset_z * sy;

    const float rotated_offset_z =
        renderer.collider.offset_x * sy +
        renderer.collider.offset_z * cy;

    const float target_x =
        renderer.collider.body_x +
        rotated_offset_x;

    const float target_y =
        renderer.collider.body_y +
        renderer.collider.offset_y;

    const float target_z =
        renderer.collider.body_z +
        rotated_offset_z;

    float dx =
        target_x -
        ai_center_x;

    float dy =
        target_y -
        ai_center_y;

    float dz =
        target_z -
        ai_center_z;

    const float length =
        std::sqrt(
            dx*dx +
            dy*dy +
            dz*dz
        );

    if (length <= 0.0001f)
        return;

    const float inv_length =
        1.0f /
        length;

    dx *= inv_length;
    dy *= inv_length;
    dz *= inv_length;

    ProjectileState projectile;

    projectile.radius =
        0.11f;

    projectile.gravity_scale =
        0.0f;

    projectile.hostile =
        true;

    projectile.active =
        true;

    /*
        Spawn slightly in front of the AI.
    */
    const float muzzle_distance =
        0.75f;

    projectile.x =
        ai_center_x +
        dx * muzzle_distance;

    projectile.y =
        ai_center_y +
        dy * muzzle_distance;

    projectile.z =
        ai_center_z +
        dz * muzzle_distance;

    projectile.vx =
        dx *
        renderer.ai_agent.projectile_speed;

    projectile.vy =
        dy *
        renderer.ai_agent.projectile_speed;

    projectile.vz =
        dz *
        renderer.ai_agent.projectile_speed;

    renderer.projectiles.push_back(
        projectile
    );

    renderer.status =
        "AI FIRE - RANGE <= 20m";
}

void renderer_update_ai(
    Renderer& renderer,
    float dt
)
{
    if (!renderer.ai_agent.enabled ||
        dt <= 0.0f) {
        return;
    }

    if (dt > 0.05f)
        dt = 0.05f;

    /*
        Cooldown progresses continuously, even while patrolling.
        This lets the AI fire shortly after the player enters range.
    */
    renderer.ai_agent.fire_timer =
        std::min(
            renderer.ai_agent.fire_interval,
            renderer.ai_agent.fire_timer +
                dt
        );

    const float player_dx =
        renderer.collider.body_x -
        renderer.ai_agent.x;

    const float player_dz =
        renderer.collider.body_z -
        renderer.ai_agent.z;

    const float player_distance =
        std::sqrt(
            player_dx*player_dx +
            player_dz*player_dz
        );

    /*
        ============================================================
        ATTACK MODE
        ============================================================
        Within 20 meters/units:
        - stop waypoint translation;
        - turn toward the player;
        - shoot when reasonably aligned.
    */
    if (player_distance <=
        renderer.ai_agent.attack_range) {

        renderer.ai_agent.attacking =
            true;

        const float desired_yaw =
            std::atan2(
                player_dx,
                player_dz
            );

        float yaw_error =
            wrap_angle_pi(
                desired_yaw -
                renderer.ai_agent.yaw
            );

        const float max_turn =
            renderer.ai_agent.turn_speed *
            dt;

        const float applied_turn =
            std::max(
                -max_turn,
                std::min(
                    max_turn,
                    yaw_error
                )
            );

        renderer.ai_agent.yaw =
            wrap_angle_pi(
                renderer.ai_agent.yaw +
                applied_turn
            );

        yaw_error =
            wrap_angle_pi(
                desired_yaw -
                renderer.ai_agent.yaw
            );

        /*
            About 14 degrees aim tolerance.
        */
        const float fire_angle =
            0.25f;

        if (std::fabs(yaw_error) <=
                fire_angle &&
            renderer.ai_agent.fire_timer >=
                renderer.ai_agent.fire_interval) {

            renderer_fire_ai_projectile(
                renderer
            );

            renderer.ai_agent.fire_timer =
                0.0f;
        }

        return;
    }

    /*
        ============================================================
        WAYPOINT PATROL MODE
        ============================================================
    */
    renderer.ai_agent.attacking =
        false;

    if (renderer.ai_waypoints.empty())
        return;

    if (renderer.ai_agent.waypoint_index < 0 ||
        (std::size_t)
            renderer.ai_agent.waypoint_index >=
            renderer.ai_waypoints.size()) {

        renderer.ai_agent.waypoint_index =
            0;
    }

    const AiWaypoint& target =
        renderer.ai_waypoints[
            (std::size_t)
            renderer.ai_agent.waypoint_index
        ];

    const float dx =
        target.x -
        renderer.ai_agent.x;

    const float dz =
        target.z -
        renderer.ai_agent.z;

    const float distance =
        std::sqrt(
            dx*dx +
            dz*dz
        );

    if (distance <=
        renderer.ai_agent.reach_radius) {

        renderer.ai_agent.waypoint_index =
            (
                renderer.ai_agent.waypoint_index +
                1
            ) %
            (int)
            renderer.ai_waypoints.size();

        return;
    }

    /*
        +Z is forward, therefore atan2(x,z).
    */
    const float desired_yaw =
        std::atan2(
            dx,
            dz
        );

    float yaw_error =
        wrap_angle_pi(
            desired_yaw -
            renderer.ai_agent.yaw
        );

    const float max_turn =
        renderer.ai_agent.turn_speed *
        dt;

    yaw_error =
        std::max(
            -max_turn,
            std::min(
                max_turn,
                yaw_error
            )
        );

    renderer.ai_agent.yaw =
        wrap_angle_pi(
            renderer.ai_agent.yaw +
            yaw_error
        );

    const float forward_x =
        std::sin(
            renderer.ai_agent.yaw
        );

    const float forward_z =
        std::cos(
            renderer.ai_agent.yaw
        );

    renderer.ai_agent.x +=
        forward_x *
        renderer.ai_agent.speed *
        dt;

    renderer.ai_agent.z +=
        forward_z *
        renderer.ai_agent.speed *
        dt;

    renderer.ai_agent.x =
        std::max(
            -49.0f,
            std::min(
                49.0f,
                renderer.ai_agent.x
            )
        );

    renderer.ai_agent.z =
        std::max(
            -49.0f,
            std::min(
                49.0f,
                renderer.ai_agent.z
            )
        );
}

void renderer_set_projectile_options(
    Renderer& renderer,
    float speed,
    float radius
)
{
    renderer.projectile_speed =
        std::max(
            0.1f,
            std::min(
                100.0f,
                speed
            )
        );

    renderer.projectile_radius =
        std::max(
            0.02f,
            std::min(
                1.0f,
                radius
            )
        );

    renderer.status =
        "PROJECTILE OPTIONS UPDATED";
}

void renderer_fire_projectile(
    Renderer& renderer
)
{
    /*
        Magazine limit: 10 shots.
        When empty, player must collect an orange +5 ammo item.
    */
    if (renderer.ammo <= 0) {
        renderer.ammo = 0;

        renderer.status =
            "NO AMMO - PICK UP ORANGE ITEM (+5)";

        return;
    }

    /*
        Cap live bullets for the low-end i686 target.
    */
    const std::size_t max_projectiles =
        128;

    if (renderer.projectiles.size() >=
        max_projectiles) {

        renderer.projectiles.erase(
            renderer.projectiles.begin()
        );
    }

    ProjectileState projectile;

    /*
        Character forward:
            yaw = 0      -> +Z
            yaw = +90deg -> +X
    */
    const float yaw =
        renderer.collider.body_yaw;

    float dir_x =
        std::sin(yaw);

    float dir_y =
        0.0f;

    float dir_z =
        std::cos(yaw);

    /*
        Spawn from the Box Collider center, transformed with the
        character's world position and heading.
    */
    const float cy =
        std::cos(yaw);

    const float sy =
        std::sin(yaw);

    const float rotated_offset_x =
        renderer.collider.offset_x * cy -
        renderer.collider.offset_z * sy;

    const float rotated_offset_z =
        renderer.collider.offset_x * sy +
        renderer.collider.offset_z * cy;

    projectile.x =
        renderer.collider.body_x +
        rotated_offset_x;

    projectile.y =
        renderer.collider.body_y +
        renderer.collider.offset_y;

    projectile.z =
        renderer.collider.body_z +
        rotated_offset_z;

    /*
        Push the spawn point a little forward so the ball does not
        visually start inside the character.
    */
    const float muzzle_distance =
        renderer.collider.size_z *
            0.5f +
        renderer.projectile_radius +
        0.05f;

    projectile.x +=
        dir_x *
        muzzle_distance;

    projectile.z +=
        dir_z *
        muzzle_distance;

    projectile.radius =
        renderer.projectile_radius;

    projectile.gravity_scale =
        1.0f;

    projectile.hostile =
        false;

    projectile.vx =
        dir_x *
        renderer.projectile_speed;

    projectile.vy =
        dir_y *
        renderer.projectile_speed;

    projectile.vz =
        dir_z *
        renderer.projectile_speed;

    projectile.lifetime =
        0.0f;

    projectile.active =
        true;

    renderer.projectiles.push_back(
        projectile
    );

    --renderer.ammo;

    if (renderer.ammo < 0)
        renderer.ammo = 0;

    renderer.status =
        "SHOOT - AMMO " +
        std::to_string(
            renderer.ammo
        ) +
        "/" +
        std::to_string(
            renderer.max_ammo
        );
}

void renderer_clear_projectiles(
    Renderer& renderer
)
{
    renderer.projectiles.clear();

    renderer.status =
        "PROJECTILES CLEARED";
}


void renderer_reset_ammo_pickups(
    Renderer& renderer
)
{
    renderer.ammo =
        renderer.max_ammo;

    for (AmmoPickupState& pickup :
         renderer.ammo_pickups) {

        pickup.active =
            true;
    }

    renderer.status =
        "AMMO ITEMS RESET - 10/10";
}

void renderer_orbit_drag(
    Renderer& renderer,
    float delta_x,
    float delta_y
)
{
    /*
        Mouse sensitivity in radians per pixel.
        About 0.46 degrees/pixel.
    */
    const float sensitivity =
        0.008f;

    renderer.orbit_yaw +=
        delta_x *
        sensitivity;

    renderer.orbit_pitch +=
        delta_y *
        sensitivity;

    /*
        Prevent the orbit camera from crossing the poles and
        flipping suddenly.
    */
    const float max_pitch =
        1.553343f; /* ~89 degrees */

    if (renderer.orbit_pitch >
        max_pitch) {

        renderer.orbit_pitch =
            max_pitch;
    }

    if (renderer.orbit_pitch <
        -max_pitch) {

        renderer.orbit_pitch =
            -max_pitch;
    }

    /*
        Keep yaw numerically small during long editing sessions.
    */
    const float pi =
        3.14159265358979323846f;

    const float two_pi =
        pi * 2.0f;

    while (renderer.orbit_yaw >
           pi) {

        renderer.orbit_yaw -=
            two_pi;
    }

    while (renderer.orbit_yaw <
           -pi) {

        renderer.orbit_yaw +=
            two_pi;
    }
}

void renderer_orbit_reset(
    Renderer& renderer
)
{
    renderer.orbit_yaw = 0.0f;
    renderer.orbit_pitch = 0.10f;
    renderer.orbit_distance = 6.0f;

    renderer.status =
        "CHASE CAMERA RESET";
}
