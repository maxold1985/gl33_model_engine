#pragma once

#include "glb_loader.h"
#include "ui.h"
#include "transform.h"

#include <string>
#include <vector>

struct GpuMesh {
    unsigned int vao = 0;
    unsigned int vbo = 0;
    unsigned int ebo = 0;
    unsigned int texture = 0;

    int index_count = 0;
    bool has_texture = false;

    /*
        CPU animation/skinning state.
    */
    std::vector<GlbVertex> bind_vertices;
    std::vector<GlbVertex> animated_vertices;
    std::vector<ModelBone> bones;
    std::vector<int> bone_node_indices;

    int node_index = -1;
    bool cpu_animated = false;

    float base_color_factor[4] = {
        1.0f, 1.0f, 1.0f, 1.0f
    };

    float metallic_factor = 0.0f;
    float roughness_factor = 1.0f;

    float center[3] = {0,0,0};
    float radius = 1.0f;
};


struct BoxColliderState {
    /*
        Collider dimensions in the normalized/display world used by
        the renderer. The loaded model is auto-fitted into this space.
    */
    float size_x = 1.0f;
    float size_y = 2.0f;
    float size_z = 1.0f;

    float offset_x = 0.0f;
    float offset_y = 0.0f;
    float offset_z = 0.0f;

    bool gravity_enabled = false;
    bool show_collider = true;

    /*
        Character/body world position.
    */
    float body_x = 0.0f;
    float body_y = 0.0f;
    float body_z = 0.0f;

    /*
        Shenmue-style facing direction.
        0 radians = forward toward +Z.
    */
    float body_yaw = 0.0f;

    float move_speed = 2.5f;
    float backward_speed = 1.6f;
    float turn_speed = 2.0943951f; /* 120 deg/s */

    /*
        Simple vertical rigid-body state.
    */
    float vertical_velocity = 0.0f;

    /*
        Fixed ground plane for this first physics module.
    */
    float ground_y = -1.75f;

    bool grounded = false;
};


struct ProjectileState {
    TransformComponent transform;

    float x = 0.0f;
    float y = 0.0f;
    float z = 0.0f;

    float vx = 0.0f;
    float vy = 0.0f;
    float vz = 0.0f;

    float radius = 0.12f;
    float lifetime = 0.0f;

    /*
        Player shots keep the old ball/gravity behavior.
        AI attack shots are straight hostile projectiles.
    */
    float gravity_scale = 1.0f;
    bool hostile = false;

    bool active = true;
};


struct AiWaypoint {
    float x = 0.0f;
    float z = 0.0f;
};

struct AiAgentState {
    TransformComponent transform;

    bool enabled = true;
    bool show_waypoints = true;

    float x = -15.0f;
    float z = -15.0f;
    float yaw = 0.0f;

    float speed = 2.0f;
    float turn_speed = 3.5f;
    float reach_radius = 0.65f;

    /*
        Combat mode.
        1 engine unit is treated as approximately 1 meter.
    */
    float attack_range = 20.0f;
    float fire_interval = 1.0f;
    float fire_timer = 0.0f;
    float projectile_speed = 18.0f;

    bool attacking = false;

    int waypoint_index = 1;
};

struct AmmoPickupState {
    TransformComponent transform;

    float x = 0.0f;
    float z = 0.0f;

    int ammo_amount = 5;

    bool active = true;
};

struct Renderer {
    unsigned int program = 0;

    int u_mvp = -1;
    int u_texture = -1;
    int u_use_texture = -1;
    int u_base_color = -1;
    int u_metallic = -1;
    int u_roughness = -1;

    GpuMesh cube;

    /*
        Procedural helper meshes.
    */
    GpuMesh ground_plane;
    GpuMesh projectile_sphere;

    std::vector<GpuMesh> model_parts;

    float model_center[3] = {0,0,0};
    float model_radius = 1.0f;

    UiRenderer ui;

    bool show_model = false;
    bool texture_enabled = true;
    bool wireframe = false;

    /*
        Third-person follow/orbit camera.

        It follows the CHARACTER POSITION only.
        Character body_yaw does NOT rotate the camera anymore.
        RMB controls orbit_yaw/orbit_pitch independently.
    */
    float orbit_yaw = 0.0f;
    float orbit_pitch = 0.10f;
    float orbit_distance = 6.0f;
    float camera_target_height = 0.85f;

    /*
        Box collider + simple gravity body.
    */
    BoxColliderState collider;

    /* Unity-style transform for the primary loaded object. */
    TransformComponent transform;

    /*
        Projectile system.
    */
    std::vector<ProjectileState> projectiles;

    float projectile_speed = 12.0f;
    float projectile_radius = 0.12f;

    /*
        Ammo magazine.
        Starts at 10; pickup items restore +5, clamped to 10.
    */
    int ammo = 10;
    int max_ammo = 10;

    /*
        AI hostile projectiles damage this health value.
    */
    int player_health = 100;
    int player_max_health = 100;

    std::vector<AmmoPickupState> ammo_pickups;

    bool show_ground_plane = true;

    /*
        Simple waypoint AI. It renders another instance of the same
        model currently loaded by the user.
    */
    AiAgentState ai_agent;
    std::vector<AiWaypoint> ai_waypoints;

    /*
        Win32 UI state for animation controls.
        Playback data is read from FBX; visual skinning is still a later step.
    */
    int animation_count = 0;
    int animation_index = 0;
    float animation_speed = 1.0f;
    float animation_time = 0.0f;
    bool animation_playing = false;

    /*
        Animation data copied from the loaded FBX.
    */
    std::vector<ModelNode> animation_nodes;
    std::vector<ModelAnimationClip> animation_clips;

    /*
        inverse(scene root transform), copied from the FBX loader.
    */
    float animation_global_inverse[16] = {
        1,0,0,0,
        0,1,0,0,
        0,0,1,0,
        0,0,0,1
    };

    std::string status = "READY";
    std::string loaded_file;
};

bool renderer_init(Renderer& renderer);
void renderer_shutdown(Renderer& renderer);

bool renderer_load_model(
    Renderer& renderer,
    const std::string& path
);

void renderer_toggle_texture(Renderer& renderer);
void renderer_toggle_wireframe(Renderer& renderer);
void renderer_toggle_cube_model(Renderer& renderer);

void renderer_draw(
    Renderer& renderer,
    int width,
    int height,
    float time_seconds
);

void renderer_menu_click(
    Renderer& renderer,
    int x,
    int y,
    void (*open_file_callback)()
);


void renderer_animation_play_pause(Renderer& renderer);
void renderer_animation_next(Renderer& renderer);
void renderer_animation_previous(Renderer& renderer);
void renderer_animation_set_index(Renderer& renderer, int index);
void renderer_animation_set_speed(Renderer& renderer, float speed);
void renderer_animation_set_time(Renderer& renderer, float time_seconds);


void renderer_orbit_drag(
    Renderer& renderer,
    float delta_x,
    float delta_y
);

void renderer_orbit_reset(
    Renderer& renderer
);


void renderer_set_box_collider(
    Renderer& renderer,
    float size_x,
    float size_y,
    float size_z,
    float offset_x,
    float offset_y,
    float offset_z,
    bool gravity_enabled,
    bool show_collider
);

void renderer_physics_step(
    Renderer& renderer,
    float dt
);

void renderer_physics_reset(
    Renderer& renderer
);


void renderer_set_projectile_options(
    Renderer& renderer,
    float speed,
    float radius
);

void renderer_fire_projectile(
    Renderer& renderer
);

void renderer_clear_projectiles(
    Renderer& renderer
);


void renderer_move_character(
    Renderer& renderer,
    float forward_axis,
    float turn_axis,
    float dt
);


void renderer_set_ai_options(
    Renderer& renderer,
    bool enabled,
    bool show_waypoints,
    float speed
);

void renderer_reset_ai(
    Renderer& renderer
);

void renderer_update_ai(
    Renderer& renderer,
    float dt
);


void renderer_reset_ammo_pickups(
    Renderer& renderer
);
