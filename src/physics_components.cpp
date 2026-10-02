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
    (void)dt;

    /*
        Scene owns integration. The component only mirrors configuration
        here so forces/impulses from other Components are accumulated
        before Scene::Update steps PhysicsWorld.
    */
    body.use_gravity =
        use_gravity;

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
