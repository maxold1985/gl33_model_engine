#pragma once

struct Mat4 {
    float m[16];
};

Mat4 mat4_identity();
Mat4 mat4_multiply(const Mat4& a, const Mat4& b);
Mat4 mat4_translation(float x, float y, float z);
Mat4 mat4_rotation_x(float radians);
Mat4 mat4_rotation_y(float radians);
Mat4 mat4_scale(float x, float y, float z);
Mat4 mat4_perspective(float fovy_radians, float aspect, float znear, float zfar);
