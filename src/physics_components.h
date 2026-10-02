#pragma once

#include "game_object.h"
#include "physics.h"

class RigidbodyComponent : public Component {
public:
    PhysicsBody body;

    float mass = 1.0f;
    bool use_gravity = true;
    bool is_kinematic = false;

    void Start() override;
    void Update(float dt) override;

    void AddForce(
        float x,
        float y,
        float z
    );

    void AddImpulse(
        float x,
        float y,
        float z
    );
};

class BoxColliderComponent : public Component {
public:
    TransformVec3 center{0.0f,0.0f,0.0f};
    TransformVec3 size{1.0f,1.0f,1.0f};

    bool is_trigger = false;

    PhysicsAabb aabb() const;
};
