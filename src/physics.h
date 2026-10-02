#pragma once

#include <vector>

struct PhysicsVec3 {
    float x = 0.0f, y = 0.0f, z = 0.0f;
};

struct PhysicsRay {
    PhysicsVec3 origin;
    PhysicsVec3 direction;
    float max_distance = 100.0f;
};

struct PhysicsRayHit {
    bool hit = false;
    float distance = 0.0f;
    PhysicsVec3 point;
    PhysicsVec3 normal;
};

struct PhysicsAabb {
    PhysicsVec3 center;
    PhysicsVec3 half_extents{0.5f,0.5f,0.5f};
};

struct PhysicsBody {
    PhysicsVec3 position;
    PhysicsVec3 velocity;
    PhysicsVec3 force;

    float mass = 1.0f;
    float inverse_mass = 1.0f;
    float restitution = 0.15f;
    float linear_damping = 0.15f;

    PhysicsAabb collider;
    bool use_gravity = true;
    bool is_static = false;
    bool grounded = false;
};

struct PhysicsWorld {
    PhysicsVec3 gravity{0.0f,-9.81f,0.0f};
    float ground_y = 0.0f;
    std::vector<PhysicsBody*> bodies;
};

void physics_body_set_mass(PhysicsBody& body, float mass);
void physics_apply_force(PhysicsBody& body, const PhysicsVec3& force);
void physics_apply_impulse(PhysicsBody& body, const PhysicsVec3& impulse);
void physics_world_add_body(PhysicsWorld& world, PhysicsBody& body);
void physics_world_remove_body(PhysicsWorld& world, PhysicsBody& body);
void physics_world_step(PhysicsWorld& world, float dt);

bool physics_aabb_overlap(
    const PhysicsAabb& a,
    const PhysicsVec3& a_position,
    const PhysicsAabb& b,
    const PhysicsVec3& b_position
);

bool physics_raycast_ground(
    const PhysicsWorld& world,
    const PhysicsRay& ray,
    PhysicsRayHit& hit
);
