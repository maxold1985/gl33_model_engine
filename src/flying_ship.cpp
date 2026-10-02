#include "flying_ship.h"

#include <algorithm>
#include <cmath>

void flying_ship_init(
    FlyingShip& ship
)
{
    ship = FlyingShip{};
}

void flying_ship_update(
    FlyingShip& ship,
    float dt
)
{
    if (dt <= 0.0f)
        return;

    dt = std::min(dt, 0.05f);
    ship.time += dt;

    /*
        The ship flies a wide figure-eight above the scene.
        This keeps it visible while continuously performing
        banking, climbing and diving manoeuvres.
    */
    const float phase =
        ship.time *
        ship.speed /
        ship.orbit_radius;

    const float previous_x =
        ship.x;

    const float previous_y =
        ship.y;

    const float previous_z =
        ship.z;

    ship.x =
        std::sin(phase) *
        ship.orbit_radius;

    ship.z =
        std::sin(phase * 2.0f) *
        ship.orbit_radius *
        0.55f;

    ship.y =
        ship.base_height +
        std::sin(phase * 1.7f) *
        6.0f +
        std::sin(phase * 3.4f) *
        1.5f;

    const float vx =
        (ship.x - previous_x) / dt;

    const float vy =
        (ship.y - previous_y) / dt;

    const float vz =
        (ship.z - previous_z) / dt;

    const float horizontal_speed =
        std::sqrt(
            vx * vx +
            vz * vz
        );

    ship.yaw =
        std::atan2(
            vx,
            vz
        );

    ship.pitch =
        -std::atan2(
            vy,
            std::max(
                horizontal_speed,
                0.001f
            )
        );

    /*
        Banking follows the changing turn direction.
        Extra sinusoidal roll periodically makes the
        aircraft perform a more visible aerial manoeuvre.
    */
    const float turn_bank =
        -std::sin(phase) *
        0.65f;

    const float stunt_roll =
        std::sin(phase * 0.55f) *
        0.45f;

    ship.roll =
        turn_bank +
        stunt_roll;
}
