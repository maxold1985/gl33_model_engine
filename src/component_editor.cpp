#include "component_editor.h"

#include "cpp_script_component.h"
#include "mesh_renderer_component.h"
#include "physics_components.h"

#include <commdlg.h>

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>

enum : int {
    ID_CE_APPLY = 4001,
    ID_CE_REFRESH,
    ID_CE_SCRIPT_LOAD
};

static const char* COMPONENT_EDITOR_CLASS =
    "GL33ComponentEditor";

static HWND make_label(
    HWND parent,
    const char* text,
    int x,
    int y,
    int w,
    int h
)
{
    return CreateWindowExA(
        0,
        "STATIC",
        text,
        WS_CHILD | WS_VISIBLE,
        x, y, w, h,
        parent,
        nullptr,
        GetModuleHandleA(nullptr),
        nullptr
    );
}

static HWND make_edit(
    HWND parent,
    int x,
    int y,
    int w = 72
)
{
    return CreateWindowExA(
        WS_EX_CLIENTEDGE,
        "EDIT",
        "",
        WS_CHILD |
        WS_VISIBLE |
        WS_TABSTOP |
        ES_AUTOHSCROLL,
        x, y, w, 23,
        parent,
        nullptr,
        GetModuleHandleA(nullptr),
        nullptr
    );
}

static HWND make_check(
    HWND parent,
    const char* text,
    int x,
    int y,
    int w = 100
)
{
    return CreateWindowExA(
        0,
        "BUTTON",
        text,
        WS_CHILD |
        WS_VISIBLE |
        WS_TABSTOP |
        BS_AUTOCHECKBOX,
        x, y, w, 22,
        parent,
        nullptr,
        GetModuleHandleA(nullptr),
        nullptr
    );
}

static HWND make_groupbox(
    HWND parent,
    const char* text,
    int x,
    int y,
    int w,
    int h
)
{
    return CreateWindowExA(
        0,
        "BUTTON",
        text,
        WS_CHILD |
        WS_VISIBLE |
        BS_GROUPBOX,
        x, y, w, h,
        parent,
        nullptr,
        GetModuleHandleA(nullptr),
        nullptr
    );
}

static std::string edit_text(
    HWND control
)
{
    if (!control)
        return {};

    const int length =
        GetWindowTextLengthA(control);

    std::string result(
        (std::size_t)length + 1,
        '\0'
    );

    GetWindowTextA(
        control,
        &result[0],
        length + 1
    );

    result.resize(
        (std::size_t)length
    );

    return result;
}

static float edit_float(
    HWND control,
    float fallback
)
{
    const std::string value =
        edit_text(control);

    if (value.empty())
        return fallback;

    char* end = nullptr;

    const float parsed =
        std::strtof(
            value.c_str(),
            &end
        );

    if (end == value.c_str())
        return fallback;

    return parsed;
}

static void set_float(
    HWND control,
    float value
)
{
    char text[64];

    std::snprintf(
        text,
        sizeof(text),
        "%.4f",
        value
    );

    SetWindowTextA(
        control,
        text
    );
}

static bool checked(
    HWND control
)
{
    return control &&
        SendMessageA(
            control,
            BM_GETCHECK,
            0,
            0
        ) == BST_CHECKED;
}

static void set_checked(
    HWND control,
    bool value
)
{
    if (!control)
        return;

    SendMessageA(
        control,
        BM_SETCHECK,
        value
            ? BST_CHECKED
            : BST_UNCHECKED,
        0
    );
}

static void enable_control(
    HWND control,
    bool enabled
)
{
    if (control)
        EnableWindow(
            control,
            enabled ? TRUE : FALSE
        );
}

static void enable_rigidbody(
    ComponentEditorWindow& editor,
    bool enabled
)
{
    enable_control(editor.frame_rigidbody, enabled);
    enable_control(editor.rb_enabled, enabled);
    enable_control(editor.rb_mass, enabled);
    enable_control(editor.rb_restitution, enabled);
    enable_control(editor.rb_friction, enabled);
    enable_control(editor.rb_damping, enabled);
    enable_control(editor.rb_gravity, enabled);
    enable_control(editor.rb_kinematic, enabled);
}

static void enable_box(
    ComponentEditorWindow& editor,
    bool enabled
)
{
    enable_control(editor.frame_box_collider, enabled);
    enable_control(editor.box_enabled, enabled);
    enable_control(editor.box_center_x, enabled);
    enable_control(editor.box_center_y, enabled);
    enable_control(editor.box_center_z, enabled);
    enable_control(editor.box_size_x, enabled);
    enable_control(editor.box_size_y, enabled);
    enable_control(editor.box_size_z, enabled);
    enable_control(editor.box_trigger, enabled);
}

static void enable_mesh(
    ComponentEditorWindow& editor,
    bool enabled
)
{
    enable_control(editor.frame_mesh_renderer, enabled);
    enable_control(editor.mesh_enabled, enabled);
    enable_control(editor.mesh_source, enabled);
    enable_control(editor.mesh_visible, enabled);
    enable_control(editor.mesh_texture, enabled);
    enable_control(editor.mesh_auto_fit, enabled);
}

static void enable_script(
    ComponentEditorWindow& editor,
    bool enabled
)
{
    enable_control(editor.frame_cpp_script, enabled);
    enable_control(editor.script_enabled, enabled);
    enable_control(editor.script_path, enabled);
    enable_control(editor.script_load, enabled);
}

void component_editor_refresh(
    ComponentEditorWindow& editor
)
{
    GameObject* object =
        editor.object;

    if (!object)
        return;

    if (editor.label_object) {
        const std::string title =
            "Selected: " +
            object->name;

        SetWindowTextA(
            editor.label_object,
            title.c_str()
        );
    }

    set_float(
        editor.pos_x,
        object->transform.position.x
    );

    set_float(
        editor.pos_y,
        object->transform.position.y
    );

    set_float(
        editor.pos_z,
        object->transform.position.z
    );

    set_float(
        editor.rot_x,
        object->transform.rotation.x
    );

    set_float(
        editor.rot_y,
        object->transform.rotation.y
    );

    set_float(
        editor.rot_z,
        object->transform.rotation.z
    );

    set_float(
        editor.scale_x,
        object->transform.scale.x
    );

    set_float(
        editor.scale_y,
        object->transform.scale.y
    );

    set_float(
        editor.scale_z,
        object->transform.scale.z
    );

    RigidbodyComponent* rb =
        object->GetComponent<
            RigidbodyComponent
        >();

    enable_rigidbody(
        editor,
        rb != nullptr
    );

    if (rb) {
        set_checked(
            editor.rb_enabled,
            rb->enabled
        );

        set_float(
            editor.rb_mass,
            rb->mass
        );

        set_float(
            editor.rb_restitution,
            rb->restitution
        );

        set_float(
            editor.rb_friction,
            rb->friction
        );

        set_float(
            editor.rb_damping,
            rb->linear_damping
        );

        set_checked(
            editor.rb_gravity,
            rb->use_gravity
        );

        set_checked(
            editor.rb_kinematic,
            rb->is_kinematic
        );
    }

    BoxColliderComponent* box =
        object->GetComponent<
            BoxColliderComponent
        >();

    enable_box(
        editor,
        box != nullptr
    );

    if (box) {
        set_checked(
            editor.box_enabled,
            box->enabled
        );

        set_float(
            editor.box_center_x,
            box->center.x
        );

        set_float(
            editor.box_center_y,
            box->center.y
        );

        set_float(
            editor.box_center_z,
            box->center.z
        );

        set_float(
            editor.box_size_x,
            box->size.x
        );

        set_float(
            editor.box_size_y,
            box->size.y
        );

        set_float(
            editor.box_size_z,
            box->size.z
        );

        set_checked(
            editor.box_trigger,
            box->is_trigger
        );
    }

    MeshRendererComponent* mesh =
        object->GetComponent<
            MeshRendererComponent
        >();

    enable_mesh(
        editor,
        mesh != nullptr
    );

    if (mesh) {
        set_checked(
            editor.mesh_enabled,
            mesh->enabled
        );

        SendMessageA(
            editor.mesh_source,
            CB_SETCURSEL,
            mesh->source ==
                MeshRendererSource::LoadedModel
                ? 1
                : 0,
            0
        );

        set_checked(
            editor.mesh_visible,
            mesh->visible
        );

        set_checked(
            editor.mesh_texture,
            mesh->use_texture
        );

        set_checked(
            editor.mesh_auto_fit,
            mesh->auto_fit
        );
    }

    CppScriptComponent* script =
        object->GetComponent<
            CppScriptComponent
        >();

    enable_script(
        editor,
        script != nullptr
    );

    if (script) {
        set_checked(
            editor.script_enabled,
            script->enabled
        );

        SetWindowTextA(
            editor.script_path,
            script->source_path.c_str()
        );
    } else if (editor.script_path) {
        SetWindowTextA(
            editor.script_path,
            ""
        );
    }
}

static void apply_editor(
    ComponentEditorWindow& editor
)
{
    GameObject* object =
        editor.object;

    if (!object)
        return;

    object->transform.position.x =
        edit_float(
            editor.pos_x,
            object->transform.position.x
        );

    object->transform.position.y =
        edit_float(
            editor.pos_y,
            object->transform.position.y
        );

    object->transform.position.z =
        edit_float(
            editor.pos_z,
            object->transform.position.z
        );

    object->transform.rotation.x =
        edit_float(
            editor.rot_x,
            object->transform.rotation.x
        );

    object->transform.rotation.y =
        edit_float(
            editor.rot_y,
            object->transform.rotation.y
        );

    object->transform.rotation.z =
        edit_float(
            editor.rot_z,
            object->transform.rotation.z
        );

    object->transform.scale.x =
        edit_float(
            editor.scale_x,
            object->transform.scale.x
        );

    object->transform.scale.y =
        edit_float(
            editor.scale_y,
            object->transform.scale.y
        );

    object->transform.scale.z =
        edit_float(
            editor.scale_z,
            object->transform.scale.z
        );

    RigidbodyComponent* rb =
        object->GetComponent<
            RigidbodyComponent
        >();

    if (rb) {
        rb->enabled =
            checked(editor.rb_enabled);

        rb->mass =
            edit_float(
                editor.rb_mass,
                rb->mass
            );

        rb->restitution =
            edit_float(
                editor.rb_restitution,
                rb->restitution
            );

        rb->friction =
            edit_float(
                editor.rb_friction,
                rb->friction
            );

        rb->linear_damping =
            edit_float(
                editor.rb_damping,
                rb->linear_damping
            );

        rb->use_gravity =
            checked(editor.rb_gravity);

        rb->is_kinematic =
            checked(editor.rb_kinematic);

        /*
            Editing Transform position in the Inspector teleports the
            Rigidbody too, matching the expected editor behavior.
        */
        rb->body.position = {
            object->transform.position.x,
            object->transform.position.y,
            object->transform.position.z
        };
    }

    BoxColliderComponent* box =
        object->GetComponent<
            BoxColliderComponent
        >();

    if (box) {
        box->enabled =
            checked(editor.box_enabled);

        box->center.x =
            edit_float(
                editor.box_center_x,
                box->center.x
            );

        box->center.y =
            edit_float(
                editor.box_center_y,
                box->center.y
            );

        box->center.z =
            edit_float(
                editor.box_center_z,
                box->center.z
            );

        box->size.x =
            edit_float(
                editor.box_size_x,
                box->size.x
            );

        box->size.y =
            edit_float(
                editor.box_size_y,
                box->size.y
            );

        box->size.z =
            edit_float(
                editor.box_size_z,
                box->size.z
            );

        box->is_trigger =
            checked(editor.box_trigger);
    }

    MeshRendererComponent* mesh =
        object->GetComponent<
            MeshRendererComponent
        >();

    if (mesh) {
        mesh->enabled =
            checked(editor.mesh_enabled);

        const int source =
            (int)SendMessageA(
                editor.mesh_source,
                CB_GETCURSEL,
                0,
                0
            );

        mesh->source =
            source == 1
                ? MeshRendererSource::LoadedModel
                : MeshRendererSource::Cube;

        mesh->visible =
            checked(editor.mesh_visible);

        mesh->use_texture =
            checked(editor.mesh_texture);

        mesh->auto_fit =
            checked(editor.mesh_auto_fit);
    }

    CppScriptComponent* script =
        object->GetComponent<
            CppScriptComponent
        >();

    if (script) {
        script->enabled =
            checked(editor.script_enabled);
    }

    component_editor_refresh(
        editor
    );
}

static void load_component_script(
    ComponentEditorWindow& editor
)
{
    if (!editor.object)
        return;

    CppScriptComponent* script =
        editor.object->GetComponent<
            CppScriptComponent
        >();

    if (!script)
        return;

    char filename[MAX_PATH] = {};

    OPENFILENAMEA ofn{};
    ofn.lStructSize =
        sizeof(ofn);

    ofn.hwndOwner =
        editor.window;

    ofn.lpstrFilter =
        "C++ Scripts (*.cpp)\0*.cpp\0"
        "All files (*.*)\0*.*\0";

    ofn.lpstrFile =
        filename;

    ofn.nMaxFile =
        MAX_PATH;

    ofn.Flags =
        OFN_FILEMUSTEXIST |
        OFN_PATHMUSTEXIST;

    ofn.lpstrDefExt =
        "cpp";

    if (!GetOpenFileNameA(&ofn))
        return;

    if (!script->Load(filename)) {
        MessageBoxA(
            editor.window,
            "Failed to compile/load component script.",
            "CppScriptComponent",
            MB_OK | MB_ICONERROR
        );
        return;
    }

    component_editor_refresh(
        editor
    );
}

static BOOL CALLBACK set_child_font_proc(
    HWND child,
    LPARAM param
)
{
    SendMessageA(
        child,
        WM_SETFONT,
        (WPARAM)param,
        TRUE
    );

    return TRUE;
}

static void create_editor_controls(
    ComponentEditorWindow& e
)
{
    HWND w = e.window;

    HFONT font =
        (HFONT)GetStockObject(
            DEFAULT_GUI_FONT
        );

    e.label_object =
        make_label(
            w,
            "Selected:",
            12, 10, 450, 22
        );

    /*
        Unity-style visual component frames.
        Frames are created first so all edit/check controls are placed
        above them in the Win32 sibling Z-order.
    */
    e.frame_transform =
        make_groupbox(
            w,
            "Transform",
            8, 34,
            470, 120
        );

    e.frame_rigidbody =
        make_groupbox(
            w,
            "Rigidbody",
            8, 158,
            470, 118
        );

    e.frame_box_collider =
        make_groupbox(
            w,
            "Box Collider",
            8, 280,
            470, 112
        );

    e.frame_mesh_renderer =
        make_groupbox(
            w,
            "Mesh Renderer",
            8, 396,
            470, 88
        );

    e.frame_cpp_script =
        make_groupbox(
            w,
            "C++ Script",
            8, 488,
            470, 68
        );

    make_label(w, "Position", 20, 66, 70, 20);
    make_label(w, "X", 96, 66, 14, 20);
    e.pos_x = make_edit(w, 112, 63);
    make_label(w, "Y", 192, 66, 14, 20);
    e.pos_y = make_edit(w, 208, 63);
    make_label(w, "Z", 288, 66, 14, 20);
    e.pos_z = make_edit(w, 304, 63);

    make_label(w, "Rotation", 20, 96, 70, 20);
    e.rot_x = make_edit(w, 112, 93);
    e.rot_y = make_edit(w, 208, 93);
    e.rot_z = make_edit(w, 304, 93);

    make_label(w, "Scale", 20, 126, 70, 20);
    e.scale_x = make_edit(w, 112, 123);
    e.scale_y = make_edit(w, 208, 123);
    e.scale_z = make_edit(w, 304, 123);

    e.rb_enabled = make_check(w, "Enabled", 110, 162, 80);
    make_label(w, "Mass", 20, 188, 70, 20);
    e.rb_mass = make_edit(w, 112, 185);
    make_label(w, "Bounce", 200, 188, 58, 20);
    e.rb_restitution = make_edit(w, 265, 185);
    make_label(w, "Friction", 20, 218, 70, 20);
    e.rb_friction = make_edit(w, 112, 215);
    make_label(w, "Damping", 200, 218, 60, 20);
    e.rb_damping = make_edit(w, 265, 215);
    e.rb_gravity = make_check(w, "Use Gravity", 20, 246, 105);
    e.rb_kinematic = make_check(w, "Kinematic", 140, 246, 95);

    e.box_enabled = make_check(w, "Enabled", 110, 284, 80);
    make_label(w, "Center", 20, 310, 70, 20);
    e.box_center_x = make_edit(w, 112, 307);
    e.box_center_y = make_edit(w, 208, 307);
    e.box_center_z = make_edit(w, 304, 307);
    make_label(w, "Size", 20, 340, 70, 20);
    e.box_size_x = make_edit(w, 112, 337);
    e.box_size_y = make_edit(w, 208, 337);
    e.box_size_z = make_edit(w, 304, 337);
    e.box_trigger = make_check(w, "Is Trigger", 20, 367, 95);

    e.mesh_enabled = make_check(w, "Enabled", 110, 400, 80);

    make_label(w, "Source", 20, 430, 60, 20);
    e.mesh_source =
        CreateWindowExA(
            0,
            "COMBOBOX",
            "",
            WS_CHILD |
            WS_VISIBLE |
            WS_TABSTOP |
            CBS_DROPDOWNLIST,
            112, 426, 160, 200,
            w,
            nullptr,
            GetModuleHandleA(nullptr),
            nullptr
        );

    SendMessageA(
        e.mesh_source,
        CB_ADDSTRING,
        0,
        (LPARAM)"Cube"
    );

    SendMessageA(
        e.mesh_source,
        CB_ADDSTRING,
        0,
        (LPARAM)"LoadedModel"
    );

    e.mesh_visible =
        make_check(
            w,
            "Visible",
            20, 458, 80
        );

    e.mesh_texture =
        make_check(
            w,
            "Texture",
            110, 458, 80
        );

    e.mesh_auto_fit =
        make_check(
            w,
            "Auto Fit",
            200, 458, 80
        );

    e.script_enabled = make_check(w, "Enabled", 110, 492, 80);
    make_label(w, "Path", 20, 522, 50, 20);

    e.script_path =
        CreateWindowExA(
            WS_EX_CLIENTEDGE,
            "EDIT",
            "",
            WS_CHILD |
            WS_VISIBLE |
            ES_AUTOHSCROLL |
            ES_READONLY,
            72, 519, 315, 23,
            w,
            nullptr,
            GetModuleHandleA(nullptr),
            nullptr
        );

    e.script_load =
        CreateWindowExA(
            0,
            "BUTTON",
            "Load...",
            WS_CHILD |
            WS_VISIBLE |
            WS_TABSTOP |
            BS_PUSHBUTTON,
            395, 518, 75, 25,
            w,
            (HMENU)(INT_PTR)
                ID_CE_SCRIPT_LOAD,
            GetModuleHandleA(nullptr),
            nullptr
        );

    CreateWindowExA(
        0,
        "BUTTON",
        "Apply",
        WS_CHILD |
        WS_VISIBLE |
        WS_TABSTOP |
        BS_DEFPUSHBUTTON,
        292, 565, 85, 28,
        w,
        (HMENU)(INT_PTR)
            ID_CE_APPLY,
        GetModuleHandleA(nullptr),
        nullptr
    );

    CreateWindowExA(
        0,
        "BUTTON",
        "Refresh",
        WS_CHILD |
        WS_VISIBLE |
        WS_TABSTOP |
        BS_PUSHBUTTON,
        385, 565, 85, 28,
        w,
        (HMENU)(INT_PTR)
            ID_CE_REFRESH,
        GetModuleHandleA(nullptr),
        nullptr
    );

    EnumChildWindows(
        w,
        set_child_font_proc,
        (LPARAM)font
    );
}

static LRESULT CALLBACK component_editor_proc(
    HWND hwnd,
    UINT msg,
    WPARAM wparam,
    LPARAM lparam
)
{
    ComponentEditorWindow* editor =
        (ComponentEditorWindow*)
        GetWindowLongPtrA(
            hwnd,
            GWLP_USERDATA
        );

    switch (msg) {
        case WM_NCCREATE: {
            CREATESTRUCTA* cs =
                (CREATESTRUCTA*)lparam;

            editor =
                (ComponentEditorWindow*)
                cs->lpCreateParams;

            SetWindowLongPtrA(
                hwnd,
                GWLP_USERDATA,
                (LONG_PTR)editor
            );

            if (editor)
                editor->window = hwnd;

            return TRUE;
        }

        case WM_CREATE:
            if (editor) {
                create_editor_controls(
                    *editor
                );

                component_editor_refresh(
                    *editor
                );
            }
            return 0;

        case WM_COMMAND:
            if (!editor)
                break;

            switch (LOWORD(wparam)) {
                case ID_CE_APPLY:
                    apply_editor(
                        *editor
                    );
                    return 0;

                case ID_CE_REFRESH:
                    component_editor_refresh(
                        *editor
                    );
                    return 0;

                case ID_CE_SCRIPT_LOAD:
                    load_component_script(
                        *editor
                    );
                    return 0;
            }
            break;

        case WM_CLOSE:
            ShowWindow(
                hwnd,
                SW_HIDE
            );
            return 0;

        case WM_DESTROY:
            if (editor &&
                editor->window == hwnd) {
                editor->window = nullptr;
            }
            return 0;
    }

    return DefWindowProcA(
        hwnd,
        msg,
        wparam,
        lparam
    );
}

bool component_editor_register_class(
    HINSTANCE instance
)
{
    WNDCLASSA wc{};
    wc.style =
        CS_HREDRAW |
        CS_VREDRAW;

    wc.lpfnWndProc =
        component_editor_proc;

    wc.hInstance =
        instance;

    wc.hCursor =
        LoadCursorA(
            nullptr,
            IDC_ARROW
        );

    wc.hbrBackground =
        (HBRUSH)(
            COLOR_BTNFACE + 1
        );

    wc.lpszClassName =
        COMPONENT_EDITOR_CLASS;

    if (RegisterClassA(&wc))
        return true;

    return GetLastError() ==
        ERROR_CLASS_ALREADY_EXISTS;
}

void component_editor_unregister_class(
    HINSTANCE instance
)
{
    UnregisterClassA(
        COMPONENT_EDITOR_CLASS,
        instance
    );
}

void component_editor_open(
    ComponentEditorWindow& editor,
    HWND owner,
    HINSTANCE instance,
    GameObject* object
)
{
    editor.object =
        object;

    if (!object) {
        MessageBoxA(
            owner,
            "No GameObject is selected.",
            "Components",
            MB_OK | MB_ICONINFORMATION
        );
        return;
    }

    if (editor.window &&
        IsWindow(editor.window)) {

        component_editor_refresh(
            editor
        );

        ShowWindow(
            editor.window,
            SW_SHOW
        );

        SetForegroundWindow(
            editor.window
        );

        return;
    }

    editor.window =
        CreateWindowExA(
            WS_EX_TOOLWINDOW,
            COMPONENT_EDITOR_CLASS,
            "Component Inspector",
            WS_OVERLAPPED |
            WS_CAPTION |
            WS_SYSMENU |
            WS_THICKFRAME,
            CW_USEDEFAULT,
            CW_USEDEFAULT,
            505,
            640,
            owner,
            nullptr,
            instance,
            &editor
        );

    if (!editor.window)
        return;

    ShowWindow(
        editor.window,
        SW_SHOW
    );

    UpdateWindow(
        editor.window
    );
}

void component_editor_close(
    ComponentEditorWindow& editor
)
{
    if (editor.window &&
        IsWindow(editor.window)) {

        DestroyWindow(
            editor.window
        );
    }

    editor.window = nullptr;
    editor.object = nullptr;
}
