#pragma once

struct FlyingShip {
    float x = 0.0f;
    float y = 18.0f;
    float z = 0.0f;

    float yaw = 0.0f;
    float pitch = 0.0f;
    float roll = 0.0f;

    float speed = 14.0f;
    float time = 0.0f;
    float orbit_radius = 32.0f;
    float base_height = 18.0f;
};

void flying_ship_init(FlyingShip& ship);
void flying_ship_update(FlyingShip& ship, float dt);
