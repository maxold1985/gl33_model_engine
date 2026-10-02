#pragma once

#include <functional>

struct VehicleVec3 {
    float x = 0.0f;
    float y = 0.0f;
    float z = 0.0f;
};

struct RaycastHit {
    bool hit = false;
    float distance = 0.0f;
    VehicleVec3 point{};
    VehicleVec3 normal{0.0f, 1.0f, 0.0f};
};

using RaycastFunction = std::function<
    RaycastHit(
        const VehicleVec3& origin,
        const VehicleVec3& direction,
        float max_distance
    )
>;

struct RaycastWheel {
    VehicleVec3 local_anchor{};
    float rest_length = 0.55f;
    float radius = 0.34f;
    float spring_stiffness = 26000.0f;
    float damper_stiffness = 4200.0f;
    float longitudinal_grip = 1.0f;
    float lateral_grip = 1.0f;
    bool steering = false;
    bool driven = false;

    bool grounded = false;
    float compression = 0.0f;
    float previous_compression = 0.0f;
    float suspension_force = 0.0f;
    RaycastHit hit{};
};

struct RaycastVehicle {
    VehicleVec3 position{0.0f, 2.0f, 0.0f};
    VehicleVec3 velocity{};
    VehicleVec3 angular_velocity{};

    float yaw = 0.0f;
    float mass = 1200.0f;
    float inverse_mass = 1.0f / 1200.0f;
    float gravity = 9.81f;

    float engine_force = 8500.0f;
    float brake_force = 12000.0f;
    float steering_rate = 1.6f;
    float max_steer_angle = 0.55f;
    float linear_drag = 0.35f;
    float angular_drag = 2.4f;

    float throttle = 0.0f;
    float brake = 0.0f;
    float steering = 0.0f;

    RaycastWheel wheels[4];
};

void raycast_vehicle_init(RaycastVehicle& vehicle);
void raycast_vehicle_set_input(
    RaycastVehicle& vehicle,
    float throttle,
    float brake,
    float steering
);

void raycast_vehicle_step(
    RaycastVehicle& vehicle,
    float dt,
    const RaycastFunction& raycast
);

RaycastHit raycast_vehicle_ground_plane(
    const VehicleVec3& origin,
    const VehicleVec3& direction,
    float max_distance
);
