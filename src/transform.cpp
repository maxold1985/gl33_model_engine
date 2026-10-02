#include "transform.h"

void transform_reset(
    TransformComponent& transform
)
{
    GameObject* owner =
        transform.game_object;

    transform = TransformComponent{};

    transform.game_object =
        owner;

    transform.started =
        true;
}

Mat4 transform_matrix(
    const TransformComponent& transform
)
{
    const Mat4 translation =
        mat4_translation(
            transform.position.x,
            transform.position.y,
            transform.position.z
        );

    const Mat4 rotation_x =
        mat4_rotation_x(
            transform.rotation.x
        );

    const Mat4 rotation_y =
        mat4_rotation_y(
            transform.rotation.y
        );

    const Mat4 rotation_z =
        mat4_rotation_z(
            transform.rotation.z
        );

    const Mat4 scale =
        mat4_scale(
            transform.scale.x,
            transform.scale.y,
            transform.scale.z
        );

    /*
        T * Rz * Ry * Rx * S
        gives each object independent XYZ position, rotation and scale.
    */
    return mat4_multiply(
        translation,
        mat4_multiply(
            rotation_z,
            mat4_multiply(
                rotation_y,
                mat4_multiply(
                    rotation_x,
                    scale
                )
            )
        )
    );
}
