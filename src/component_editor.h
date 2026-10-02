#pragma once

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include "game_object.h"

struct ComponentEditorWindow {
    HWND window = nullptr;
    GameObject* object = nullptr;

    HWND label_object = nullptr;

    /* Visual component frames (Unity-style inspector sections). */
    HWND frame_transform = nullptr;
    HWND frame_rigidbody = nullptr;
    HWND frame_box_collider = nullptr;
    HWND frame_mesh_renderer = nullptr;
    HWND frame_cpp_script = nullptr;

    HWND pos_x = nullptr;
    HWND pos_y = nullptr;
    HWND pos_z = nullptr;
    HWND rot_x = nullptr;
    HWND rot_y = nullptr;
    HWND rot_z = nullptr;
    HWND scale_x = nullptr;
    HWND scale_y = nullptr;
    HWND scale_z = nullptr;

    HWND rb_enabled = nullptr;
    HWND rb_mass = nullptr;
    HWND rb_restitution = nullptr;
    HWND rb_friction = nullptr;
    HWND rb_damping = nullptr;
    HWND rb_gravity = nullptr;
    HWND rb_kinematic = nullptr;

    HWND box_enabled = nullptr;
    HWND box_center_x = nullptr;
    HWND box_center_y = nullptr;
    HWND box_center_z = nullptr;
    HWND box_size_x = nullptr;
    HWND box_size_y = nullptr;
    HWND box_size_z = nullptr;
    HWND box_trigger = nullptr;

    HWND mesh_enabled = nullptr;
    HWND mesh_source = nullptr;
    HWND mesh_visible = nullptr;
    HWND mesh_texture = nullptr;
    HWND mesh_auto_fit = nullptr;

    HWND script_enabled = nullptr;
    HWND script_path = nullptr;
    HWND script_load = nullptr;
};

bool component_editor_register_class(
    HINSTANCE instance
);

void component_editor_unregister_class(
    HINSTANCE instance
);

void component_editor_open(
    ComponentEditorWindow& editor,
    HWND owner,
    HINSTANCE instance,
    GameObject* object
);

void component_editor_refresh(
    ComponentEditorWindow& editor
);

void component_editor_close(
    ComponentEditorWindow& editor
);
