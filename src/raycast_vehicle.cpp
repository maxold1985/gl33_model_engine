#include "raycast_vehicle.h"

#include <algorithm>
#include <cmath>

namespace {

static VehicleVec3 add(
    const VehicleVec3& a,
    const VehicleVec3& b
)
{
    return {
        a.x + b.x,
        a.y + b.y,
        a.z + b.z
    };
}

static VehicleVec3 sub(
    const VehicleVec3& a,
    const VehicleVec3& b
)
{
    return {
        a.x - b.x,
        a.y - b.y,
        a.z - b.z
    };
}

static VehicleVec3 mul(
    const VehicleVec3& v,
    float s
)
{
    return {
        v.x * s,
        v.y * s,
        v.z * s
    };
}

static float dot(
    const VehicleVec3& a,
    const VehicleVec3& b
)
{
    return
        a.x * b.x +
        a.y * b.y +
        a.z * b.z;
}

static VehicleVec3 rotate_y(
    const VehicleVec3& v,
    float yaw
)
{
    const float c = std::cos(yaw);
    const float s = std::sin(yaw);

    return {
        v.x * c + v.z * s,
        v.y,
        -v.x * s + v.z * c
    };
}

static VehicleVec3 forward_from_yaw(
    float yaw
)
{
    return {
        std::sin(yaw),
        0.0f,
        std::cos(yaw)
    };
}

static VehicleVec3 right_from_yaw(
    float yaw
)
{
    return {
        std::cos(yaw),
        0.0f,
        -std::sin(yaw)
    };
}

static float clampf(
    float value,
    float min_value,
    float max_value
)
{
    return std::max(
        min_value,
        std::min(
            value,
            max_value
        )
    );
}

}

void raycast_vehicle_init(
    RaycastVehicle& vehicle
)
{
    vehicle = RaycastVehicle{};

    vehicle.inverse_mass =
        1.0f / vehicle.mass;

    vehicle.wheels[0].local_anchor =
        {-0.85f, 0.0f, 1.35f};

    vehicle.wheels[1].local_anchor =
        {0.85f, 0.0f, 1.35f};

    vehicle.wheels[2].local_anchor =
        {-0.85f, 0.0f, -1.35f};

    vehicle.wheels[3].local_anchor =
        {0.85f, 0.0f, -1.35f};

    vehicle.wheels[0].steering = true;
    vehicle.wheels[1].steering = true;

    vehicle.wheels[2].driven = true;
    vehicle.wheels[3].driven = true;
}

void raycast_vehicle_set_input(
    RaycastVehicle& vehicle,
    float throttle,
    float brake,
    float steering
)
{
    vehicle.throttle =
        clampf(
            throttle,
            -1.0f,
            1.0f
        );

    vehicle.brake =
        clampf(
            brake,
            0.0f,
            1.0f
        );

    vehicle.steering =
        clampf(
            steering,
            -1.0f,
            1.0f
        );
}

void raycast_vehicle_step(
    RaycastVehicle& vehicle,
    float dt,
    const RaycastFunction& raycast
)
{
    if (dt <= 0.0f || !raycast)
        return;

    dt = std::min(dt, 0.05f);

    VehicleVec3 total_force{
        0.0f,
        -vehicle.mass * vehicle.gravity,
        0.0f
    };

    const VehicleVec3 body_forward =
        forward_from_yaw(vehicle.yaw);

    const VehicleVec3 body_right =
        right_from_yaw(vehicle.yaw);

    int grounded_wheels = 0;
    float front_support = 0.0f;
    float rear_support = 0.0f;

    for (int i = 0; i < 4; ++i) {
        RaycastWheel& wheel =
            vehicle.wheels[i];

        const VehicleVec3 anchor =
            add(
                vehicle.position,
                rotate_y(
                    wheel.local_anchor,
                    vehicle.yaw
                )
            );

        const float cast_length =
            wheel.rest_length +
            wheel.radius;

        wheel.hit =
            raycast(
                anchor,
                {0.0f, -1.0f, 0.0f},
                cast_length
            );

        wheel.grounded =
            wheel.hit.hit;

        wheel.previous_compression =
            wheel.compression;

        if (!wheel.grounded) {
            wheel.compression = 0.0f;
            wheel.suspension_force = 0.0f;
            continue;
        }

        ++grounded_wheels;

        const float suspension_length =
            std::max(
                0.0f,
                wheel.hit.distance -
                wheel.radius
            );

        wheel.compression =
            clampf(
                wheel.rest_length -
                suspension_length,
                0.0f,
                wheel.rest_length
            );

        const float compression_speed =
            (
                wheel.compression -
                wheel.previous_compression
            ) / dt;

        float spring_force =
            wheel.compression *
            wheel.spring_stiffness;

        float damping_force =
            compression_speed *
            wheel.damper_stiffness;

        wheel.suspension_force =
            std::max(
                0.0f,
                spring_force +
                damping_force
            );

        total_force.y +=
            wheel.suspension_force;

        if (wheel.local_anchor.z >= 0.0f)
            front_support += wheel.suspension_force;
        else
            rear_support += wheel.suspension_force;

        if (wheel.driven) {
            total_force =
                add(
                    total_force,
                    mul(
                        body_forward,
                        vehicle.throttle *
                        vehicle.engine_force *
                        wheel.longitudinal_grip *
                        0.5f
                    )
                );
        }

        const float lateral_speed =
            dot(
                vehicle.velocity,
                body_right
            );

        total_force =
            add(
                total_force,
                mul(
                    body_right,
                    -lateral_speed *
                    vehicle.mass *
                    2.5f *
                    wheel.lateral_grip *
                    0.25f
                )
            );
    }

    if (grounded_wheels > 0 &&
        vehicle.brake > 0.0f) {

        const float forward_speed =
            dot(
                vehicle.velocity,
                body_forward
            );

        const float sign =
            forward_speed >= 0.0f
                ? -1.0f
                : 1.0f;

        total_force =
            add(
                total_force,
                mul(
                    body_forward,
                    sign *
                    vehicle.brake_force *
                    vehicle.brake
                )
            );
    }

    total_force =
        add(
            total_force,
            mul(
                vehicle.velocity,
                -vehicle.linear_drag *
                vehicle.mass
            )
        );

    const VehicleVec3 acceleration =
        mul(
            total_force,
            vehicle.inverse_mass
        );

    vehicle.velocity =
        add(
            vehicle.velocity,
            mul(
                acceleration,
                dt
            )
        );

    vehicle.position =
        add(
            vehicle.position,
            mul(
                vehicle.velocity,
                dt
            )
        );

    const float forward_speed =
        dot(
            vehicle.velocity,
            body_forward
        );

    if (grounded_wheels >= 2) {
        const float steer_angle =
            vehicle.steering *
            vehicle.max_steer_angle;

        const float wheel_base =
            2.70f;

        const float desired_yaw_rate =
            std::tan(steer_angle) *
            forward_speed /
            wheel_base;

        const float yaw_error =
            desired_yaw_rate -
            vehicle.angular_velocity.y;

        vehicle.angular_velocity.y +=
            yaw_error *
            std::min(
                1.0f,
                vehicle.steering_rate *
                dt * 4.0f
            );
    }

    const float support_difference =
        front_support -
        rear_support;

    vehicle.angular_velocity.y *=
        std::max(
            0.0f,
            1.0f -
            vehicle.angular_drag * dt
        );

    vehicle.yaw +=
        vehicle.angular_velocity.y *
        dt;

    if (std::fabs(support_difference) < 0.001f &&
        grounded_wheels == 0) {
        vehicle.angular_velocity.y *=
            0.999f;
    }
}

RaycastHit raycast_vehicle_ground_plane(
    const VehicleVec3& origin,
    const VehicleVec3& direction,
    float max_distance
)
{
    RaycastHit result{};

    if (max_distance <= 0.0f)
        return result;

    if (direction.y >= -0.0001f)
        return result;

    const float t =
        -origin.y /
        direction.y;

    if (t < 0.0f ||
        t > max_distance) {
        return result;
    }

    result.hit = true;
    result.distance = t;
    result.point =
        add(
            origin,
            mul(
                direction,
                t
            )
        );

    result.normal =
        {0.0f, 1.0f, 0.0f};

    return result;
}
