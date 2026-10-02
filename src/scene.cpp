#include "scene.h"

#include "physics_components.h"

#include <algorithm>

GameObject* Scene::CreateGameObject(
    const std::string& name
)
{
    std::unique_ptr<GameObject> object(
        new GameObject(name)
    );

    GameObject* result =
        object.get();

    objects_.push_back(
        std::move(object)
    );

    return result;
}

bool Scene::DestroyGameObject(
    GameObject* object
)
{
    if (!object)
        return false;

    for (auto it = objects_.begin();
         it != objects_.end();
         ++it) {

        if (it->get() == object) {
            objects_.erase(it);
            return true;
        }
    }

    return false;
}

GameObject* Scene::Find(
    const std::string& name
)
{
    for (const auto& object : objects_) {
        if (object &&
            object->name == name) {
            return object.get();
        }
    }

    return nullptr;
}

const GameObject* Scene::Find(
    const std::string& name
) const
{
    for (const auto& object : objects_) {
        if (object &&
            object->name == name) {
            return object.get();
        }
    }

    return nullptr;
}

void Scene::Update(
    float dt
)
{
    if (dt <= 0.0f)
        return;

    if (dt > 0.05f)
        dt = 0.05f;

    /*
        First run component lifecycle. Scripts and gameplay components
        can modify transforms, forces and Rigidbody settings here.
    */
    for (const auto& object : objects_) {
        if (object)
            object->update_components(dt);
    }

    physics.bodies.clear();

    /*
        Build the physics world from GameObjects that own a Rigidbody.
        A BoxCollider on the same object supplies the collision shape.
    */
    for (const auto& object : objects_) {
        if (!object || !object->active)
            continue;

        RigidbodyComponent* rigidbody =
            object->GetComponent<RigidbodyComponent>();

        if (!rigidbody ||
            !rigidbody->enabled) {
            continue;
        }

        BoxColliderComponent* collider =
            object->GetComponent<BoxColliderComponent>();

        rigidbody->body.use_gravity =
            rigidbody->use_gravity;

        if (collider &&
            collider->enabled) {
            rigidbody->body.collider =
                collider->aabb();
        }

        if (rigidbody->is_kinematic) {
            rigidbody->body.position = {
                object->transform.position.x,
                object->transform.position.y,
                object->transform.position.z
            };

            rigidbody->body.inverse_mass =
                0.0f;

            rigidbody->body.is_static =
                true;
        } else {
            physics_body_set_mass(
                rigidbody->body,
                rigidbody->mass
            );
        }

        physics_world_add_body(
            physics,
            rigidbody->body
        );
    }

    physics_world_step(
        physics,
        dt
    );

    /*
        Dynamic Rigidbody owns position after simulation.
        Kinematic bodies keep the Transform supplied by gameplay code.
    */
    for (const auto& object : objects_) {
        if (!object || !object->active)
            continue;

        RigidbodyComponent* rigidbody =
            object->GetComponent<RigidbodyComponent>();

        if (!rigidbody ||
            !rigidbody->enabled ||
            rigidbody->is_kinematic) {
            continue;
        }

        object->transform.position.x =
            rigidbody->body.position.x;

        object->transform.position.y =
            rigidbody->body.position.y;

        object->transform.position.z =
            rigidbody->body.position.z;
    }
}

void Scene::Clear()
{
    physics.bodies.clear();
    objects_.clear();
}
