#include "math3d.h"
#include <cmath>
#include <cstring>

Mat4 mat4_identity()
{
    Mat4 r{};
    r.m[0] = 1.0f;
    r.m[5] = 1.0f;
    r.m[10] = 1.0f;
    r.m[15] = 1.0f;
    return r;
}

Mat4 mat4_multiply(const Mat4& a, const Mat4& b)
{
    Mat4 r{};

    for (int col = 0; col < 4; ++col) {
        for (int row = 0; row < 4; ++row) {
            for (int k = 0; k < 4; ++k) {
                r.m[col * 4 + row] +=
                    a.m[k * 4 + row] *
                    b.m[col * 4 + k];
            }
        }
    }

    return r;
}

Mat4 mat4_translation(float x, float y, float z)
{
    Mat4 r = mat4_identity();
    r.m[12] = x;
    r.m[13] = y;
    r.m[14] = z;
    return r;
}

Mat4 mat4_rotation_x(float a)
{
    Mat4 r = mat4_identity();
    const float c = std::cos(a);
    const float s = std::sin(a);

    r.m[5] = c;
    r.m[6] = s;
    r.m[9] = -s;
    r.m[10] = c;

    return r;
}

Mat4 mat4_rotation_y(float a)
{
    Mat4 r = mat4_identity();
    const float c = std::cos(a);
    const float s = std::sin(a);

    r.m[0] = c;
    r.m[2] = -s;
    r.m[8] = s;
    r.m[10] = c;

    return r;
}


Mat4 mat4_rotation_z(float radians)
{
    Mat4 r = mat4_identity();
    const float c = std::cos(radians);
    const float s = std::sin(radians);

    r.m[0] = c;
    r.m[1] = s;
    r.m[4] = -s;
    r.m[5] = c;
    return r;
}

Mat4 mat4_scale(float x, float y, float z)
{
    Mat4 r = mat4_identity();
    r.m[0] = x;
    r.m[5] = y;
    r.m[10] = z;
    return r;
}

Mat4 mat4_perspective(
    float fovy_radians,
    float aspect,
    float znear,
    float zfar
)
{
    Mat4 r{};
    const float f = 1.0f / std::tan(fovy_radians * 0.5f);

    r.m[0] = f / aspect;
    r.m[5] = f;
    r.m[10] = (zfar + znear) / (znear - zfar);
    r.m[11] = -1.0f;
    r.m[14] = (2.0f * zfar * znear) / (znear - zfar);

    return r;
}
