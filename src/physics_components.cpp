#include "physics_components.h"

void RigidbodyComponent::Start()
{
    physics_body_set_mass(
        body,
        mass
    );

    body.use_gravity =
        use_gravity;

    body.restitution =
        restitution;

    body.friction =
        friction;

    body.linear_damping =
        linear_damping;

    body.position = {
        transform().position.x,
        transform().position.y,
        transform().position.z
    };
}

void RigidbodyComponent::Update(
    float dt
)
{
    (void)dt;

    /*
        Scene owns integration. The component only mirrors configuration
        here so forces/impulses from other Components are accumulated
        before Scene::Update steps PhysicsWorld.
    */
    body.use_gravity =
        use_gravity;

    body.restitution =
        restitution;

    body.friction =
        friction;

    body.linear_damping =
        linear_damping;

    if (is_kinematic) {
        body.position = {
            transform().position.x,
            transform().position.y,
            transform().position.z
        };
    }
}

void RigidbodyComponent::AddForce(
    float x,
    float y,
    float z
)
{
    physics_apply_force(
        body,
        {x,y,z}
    );
}

void RigidbodyComponent::AddImpulse(
    float x,
    float y,
    float z
)
{
    physics_apply_impulse(
        body,
        {x,y,z}
    );
}

PhysicsAabb BoxColliderComponent::aabb() const
{
    PhysicsAabb result;

    const float sx =
        transform().scale.x < 0.0f
            ? -transform().scale.x
            : transform().scale.x;

    const float sy =
        transform().scale.y < 0.0f
            ? -transform().scale.y
            : transform().scale.y;

    const float sz =
        transform().scale.z < 0.0f
            ? -transform().scale.z
            : transform().scale.z;

    result.center = {
        center.x * sx,
        center.y * sy,
        center.z * sz
    };

    result.half_extents = {
        size.x * sx * 0.5f,
        size.y * sy * 0.5f,
        size.z * sz * 0.5f
    };

    return result;
}
