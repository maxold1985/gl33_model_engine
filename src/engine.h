#pragma once

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include "renderer.h"
#include "script_module.h"

struct Engine {
    HINSTANCE instance = nullptr;
    HWND window = nullptr;
    HDC dc = nullptr;
    HGLRC gl_context = nullptr;

    int width = 1280;
    int height = 720;
    bool running = false;

    /*
        Right-mouse orbit drag state.
    */
    bool orbit_dragging = false;
    int orbit_last_x = 0;
    int orbit_last_y = 0;

    /*
        Shenmue-style tank controls.
        Up/Down = move along character facing.
        Left/Right = rotate character.
    */
    bool move_forward = false;
    bool move_backward = false;
    bool turn_left = false;
    bool turn_right = false;

    /*
        Used to make F fire once per press instead of Windows key-repeat.
    */
    bool fire_key_down = false;

    /*
        Native Win32 UI controls.
    */
    HWND edit_model_path = nullptr;
    HWND edit_animation = nullptr;
    HWND edit_speed = nullptr;
    HWND edit_time = nullptr;
    HWND button_apply = nullptr;

    HWND label_model = nullptr;
    HWND label_animation = nullptr;
    HWND label_speed = nullptr;
    HWND label_time = nullptr;

    /*
        Native GDI/Win32 collider controls.
    */
    HWND group_collider = nullptr;

    HWND label_collider_size = nullptr;
    HWND edit_collider_size_x = nullptr;
    HWND edit_collider_size_y = nullptr;
    HWND edit_collider_size_z = nullptr;

    HWND label_collider_offset = nullptr;
    HWND edit_collider_offset_x = nullptr;
    HWND edit_collider_offset_y = nullptr;
    HWND edit_collider_offset_z = nullptr;

    HWND check_gravity = nullptr;
    HWND check_show_collider = nullptr;
    HWND button_physics_reset = nullptr;

    /*
        Projectile controls inside the same GDI physics panel.
    */
    HWND label_projectile_speed = nullptr;
    HWND edit_projectile_speed = nullptr;

    HWND label_projectile_radius = nullptr;
    HWND edit_projectile_radius = nullptr;

    HWND button_shoot = nullptr;
    HWND button_clear_projectiles = nullptr;

    /*
        Waypoint AI controls.
    */
    HWND check_ai_enabled = nullptr;
    HWND check_ai_waypoints = nullptr;
    HWND label_ai_speed = nullptr;
    HWND edit_ai_speed = nullptr;
    HWND button_ai_reset = nullptr;

    /*
        Runtime ammo HUD inside the GDI panel.
    */
    HWND label_ammo = nullptr;
    HWND label_ammo_hint = nullptr;

    HWND label_health = nullptr;
    HWND label_ai_attack = nullptr;

    int last_ammo_display = -1;
    int last_health_display = -1;

    HMENU menu_bar = nullptr;

    Renderer renderer;

    /* Runtime C++ component script. */
    ScriptModule script_module;
    float script_object_x = 0.0f;
    float script_object_y = 0.0f;
    float script_object_z = 0.0f;
    float script_object_yaw = 0.0f;
};

bool engine_init(
    Engine& engine,
    HINSTANCE instance,
    int width,
    int height
);

int engine_run(Engine& engine);
void engine_shutdown(Engine& engine);
