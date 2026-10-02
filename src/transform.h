#pragma once

#include "math3d.h"
#include "component.h"

struct TransformVec3 {
    float x = 0.0f;
    float y = 0.0f;
    float z = 0.0f;
};

/*
    Unity-style transform component.
    Rotation is stored as Euler angles in radians.
*/
struct TransformComponent : public Component {
    TransformVec3 position{0.0f, 0.0f, 0.0f};
    TransformVec3 rotation{0.0f, 0.0f, 0.0f};
    TransformVec3 scale{1.0f, 1.0f, 1.0f};
};

void transform_reset(TransformComponent& transform);
Mat4 transform_matrix(const TransformComponent& transform);
