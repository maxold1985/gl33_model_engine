#include "physics_components.h"

void RigidbodyComponent::Start()
{
    physics_body_set_mass(
        body,
        mass
    );

    body.use_gravity =
        use_gravity;

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
    if (dt <= 0.0f)
        return;

    body.use_gravity =
        use_gravity;

    if (is_kinematic) {
        body.position = {
            transform().position.x,
            transform().position.y,
            transform().position.z
        };
        return;
    }

    /*
        Local single-body integration. A Scene/PhysicsWorld can take
        ownership of stepping later when multiple-body contacts are used.
    */
    if (body.use_gravity) {
        physics_apply_force(
            body,
            {0.0f, -9.81f * body.mass, 0.0f}
        );
    }

    const float step =
        dt > 0.05f ? 0.05f : dt;

    body.velocity.x +=
        body.force.x *
        body.inverse_mass *
        step;

    body.velocity.y +=
        body.force.y *
        body.inverse_mass *
        step;

    body.velocity.z +=
        body.force.z *
        body.inverse_mass *
        step;

    body.position.x +=
        body.velocity.x * step;

    body.position.y +=
        body.velocity.y * step;

    body.position.z +=
        body.velocity.z * step;

    body.force = {};

    transform().position.x =
        body.position.x;

    transform().position.y =
        body.position.y;

    transform().position.z =
        body.position.z;
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

    result.center = {
        center.x,
        center.y,
        center.z
    };

    result.half_extents = {
        size.x * 0.5f,
        size.y * 0.5f,
        size.z * 0.5f
    };

    return result;
}
