#include "engine.h"
#include "gl33.h"
#include "mesh_renderer_component.h"
#include "physics_components.h"

#include <GL/gl.h>
#include <commdlg.h>
#include <windowsx.h>
#include <objbase.h>

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>

#define WGL_CONTEXT_MAJOR_VERSION_ARB 0x2091
#define WGL_CONTEXT_MINOR_VERSION_ARB 0x2092
#define WGL_CONTEXT_PROFILE_MASK_ARB  0x9126
#define WGL_CONTEXT_CORE_PROFILE_BIT_ARB 0x00000001

using WglCreateContextAttribsARB =
    HGLRC (WINAPI *)(HDC, HGLRC, const int*);

using WglSwapIntervalEXT =
    BOOL (WINAPI *)(int);

/* ------------------------------------------------------------
   Win32 command/control IDs
   ------------------------------------------------------------ */

enum : int {
    ID_FILE_OPEN = 1001,
    ID_FILE_RELOAD,
    ID_FILE_EXIT,

    ID_VIEW_TEXTURE = 1101,
    ID_VIEW_WIREFRAME,
    ID_VIEW_CUBE_MODEL,

    ID_ANIM_PLAY_PAUSE = 1201,
    ID_ANIM_PREVIOUS,
    ID_ANIM_NEXT,

    ID_HELP_ABOUT = 1301,

    ID_SCRIPT_LOAD = 1401,
    ID_SCRIPT_UNLOAD,

    ID_EDIT_MODEL_PATH = 2001,
    ID_EDIT_ANIMATION,
    ID_EDIT_SPEED,
    ID_EDIT_TIME,
    ID_BUTTON_APPLY,

    ID_EDIT_COLLIDER_SIZE_X = 2101,
    ID_EDIT_COLLIDER_SIZE_Y,
    ID_EDIT_COLLIDER_SIZE_Z,

    ID_EDIT_COLLIDER_OFFSET_X,
    ID_EDIT_COLLIDER_OFFSET_Y,
    ID_EDIT_COLLIDER_OFFSET_Z,

    ID_CHECK_GRAVITY,
    ID_CHECK_SHOW_COLLIDER,
    ID_BUTTON_PHYSICS_RESET,

    ID_EDIT_PROJECTILE_SPEED,
    ID_EDIT_PROJECTILE_RADIUS,
    ID_BUTTON_SHOOT,
    ID_BUTTON_CLEAR_PROJECTILES,

    ID_CHECK_AI_ENABLED,
    ID_CHECK_AI_WAYPOINTS,
    ID_EDIT_AI_SPEED,
    ID_BUTTON_AI_RESET
};

static Engine* g_engine = nullptr;

static void set_edit_text(
    HWND control,
    const std::string& text
)
{
    if (control)
        SetWindowTextA(
            control,
            text.c_str()
        );
}

static std::string get_edit_text(
    HWND control
)
{
    if (!control)
        return {};

    const int length =
        GetWindowTextLengthA(control);

    std::string text(
        (std::size_t)length + 1,
        '\0'
    );

    GetWindowTextA(
        control,
        &text[0],
        length + 1
    );

    text.resize(
        (std::size_t)length
    );

    return text;
}

static void sync_ui_from_renderer(
    Engine& engine
)
{
    set_edit_text(
        engine.edit_model_path,
        engine.renderer.loaded_file
    );

    set_edit_text(
        engine.edit_animation,
        std::to_string(
            engine.renderer.animation_index
        )
    );

    char temp[64];

    std::snprintf(
        temp,
        sizeof(temp),
        "%.3f",
        engine.renderer.animation_speed
    );

    set_edit_text(
        engine.edit_speed,
        temp
    );

    std::snprintf(
        temp,
        sizeof(temp),
        "%.3f",
        engine.renderer.animation_time
    );

    set_edit_text(
        engine.edit_time,
        temp
    );

    std::snprintf(
        temp,
        sizeof(temp),
        "%.3f",
        engine.renderer.collider.size_x
    );
    set_edit_text(
        engine.edit_collider_size_x,
        temp
    );

    std::snprintf(
        temp,
        sizeof(temp),
        "%.3f",
        engine.renderer.collider.size_y
    );
    set_edit_text(
        engine.edit_collider_size_y,
        temp
    );

    std::snprintf(
        temp,
        sizeof(temp),
        "%.3f",
        engine.renderer.collider.size_z
    );
    set_edit_text(
        engine.edit_collider_size_z,
        temp
    );

    std::snprintf(
        temp,
        sizeof(temp),
        "%.3f",
        engine.renderer.collider.offset_x
    );
    set_edit_text(
        engine.edit_collider_offset_x,
        temp
    );

    std::snprintf(
        temp,
        sizeof(temp),
        "%.3f",
        engine.renderer.collider.offset_y
    );
    set_edit_text(
        engine.edit_collider_offset_y,
        temp
    );

    std::snprintf(
        temp,
        sizeof(temp),
        "%.3f",
        engine.renderer.collider.offset_z
    );
    set_edit_text(
        engine.edit_collider_offset_z,
        temp
    );

    if (engine.check_gravity) {
        SendMessageA(
            engine.check_gravity,
            BM_SETCHECK,
            engine.renderer.collider.gravity_enabled
                ? BST_CHECKED
                : BST_UNCHECKED,
            0
        );
    }

    if (engine.check_show_collider) {
        SendMessageA(
            engine.check_show_collider,
            BM_SETCHECK,
            engine.renderer.collider.show_collider
                ? BST_CHECKED
                : BST_UNCHECKED,
            0
        );
    }

    std::snprintf(
        temp,
        sizeof(temp),
        "%.3f",
        engine.renderer.projectile_speed
    );

    set_edit_text(
        engine.edit_projectile_speed,
        temp
    );

    std::snprintf(
        temp,
        sizeof(temp),
        "%.3f",
        engine.renderer.projectile_radius
    );

    set_edit_text(
        engine.edit_projectile_radius,
        temp
    );

    if (engine.check_ai_enabled) {
        SendMessageA(
            engine.check_ai_enabled,
            BM_SETCHECK,
            engine.renderer.ai_agent.enabled
                ? BST_CHECKED
                : BST_UNCHECKED,
            0
        );
    }

    if (engine.check_ai_waypoints) {
        SendMessageA(
            engine.check_ai_waypoints,
            BM_SETCHECK,
            engine.renderer.ai_agent.show_waypoints
                ? BST_CHECKED
                : BST_UNCHECKED,
            0
        );
    }

    std::snprintf(
        temp,
        sizeof(temp),
        "%.3f",
        engine.renderer.ai_agent.speed
    );

    set_edit_text(
        engine.edit_ai_speed,
        temp
    );

    engine.last_ammo_display =
        -1;

    engine.last_health_display =
        -1;
}


static void sync_runtime_ammo_label(
    Engine& engine
)
{
    char text[96];

    if (engine.label_ammo &&
        engine.last_ammo_display !=
            engine.renderer.ammo) {

        std::snprintf(
            text,
            sizeof(text),
            "Ammo: %d / %d",
            engine.renderer.ammo,
            engine.renderer.max_ammo
        );

        SetWindowTextA(
            engine.label_ammo,
            text
        );

        engine.last_ammo_display =
            engine.renderer.ammo;
    }

    if (engine.label_health &&
        engine.last_health_display !=
            engine.renderer.player_health) {

        std::snprintf(
            text,
            sizeof(text),
            "Health: %d / %d",
            engine.renderer.player_health,
            engine.renderer.player_max_health
        );

        SetWindowTextA(
            engine.label_health,
            text
        );

        engine.last_health_display =
            engine.renderer.player_health;
    }
}

static void sync_menu_checks(
    Engine& engine
)
{
    if (!engine.menu_bar)
        return;

    CheckMenuItem(
        engine.menu_bar,
        ID_VIEW_TEXTURE,
        MF_BYCOMMAND |
        (
            engine.renderer.texture_enabled
                ? MF_CHECKED
                : MF_UNCHECKED
        )
    );

    CheckMenuItem(
        engine.menu_bar,
        ID_VIEW_WIREFRAME,
        MF_BYCOMMAND |
        (
            engine.renderer.wireframe
                ? MF_CHECKED
                : MF_UNCHECKED
        )
    );

    CheckMenuItem(
        engine.menu_bar,
        ID_VIEW_CUBE_MODEL,
        MF_BYCOMMAND |
        (
            engine.renderer.show_model
                ? MF_CHECKED
                : MF_UNCHECKED
        )
    );
}

static bool load_model_path(
    Engine& engine,
    const std::string& path
)
{
    if (path.empty())
        return false;

    std::printf(
        "Selected file: %s\n",
        path.c_str()
    );

    const bool ok =
        renderer_load_model(
            engine.renderer,
            path
        );

    if (ok) {
        set_edit_text(
            engine.edit_model_path,
            path
        );
    }

    sync_ui_from_renderer(engine);
    sync_menu_checks(engine);

    return ok;
}

static void open_model_dialog()
{
    if (!g_engine)
        return;

    char filename[MAX_PATH] = {};

    /*
        Start from the current model path when possible.
    */
    const std::string current =
        get_edit_text(
            g_engine->edit_model_path
        );

    if (!current.empty()) {
        std::strncpy(
            filename,
            current.c_str(),
            MAX_PATH - 1
        );
    }

    OPENFILENAMEA ofn{};
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = g_engine->window;
    ofn.lpstrFilter =
        "3D Models (*.glb;*.fbx)\0*.glb;*.fbx\0"
        "glTF Binary (*.glb)\0*.glb\0"
        "FBX (*.fbx)\0*.fbx\0"
        "All files (*.*)\0*.*\0";
    ofn.lpstrFile = filename;
    ofn.nMaxFile = MAX_PATH;
    ofn.Flags =
        OFN_FILEMUSTEXIST |
        OFN_PATHMUSTEXIST;
    ofn.lpstrDefExt = "glb";

    if (GetOpenFileNameA(&ofn)) {
        load_model_path(
            *g_engine,
            filename
        );
    }
}

static void open_cpp_script_dialog()
{
    if (!g_engine)
        return;

    char filename[MAX_PATH] = {};

    OPENFILENAMEA ofn{};
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = g_engine->window;
    ofn.lpstrFilter =
        "C++ Scripts (*.cpp)\0*.cpp\0"
        "All files (*.*)\0*.*\0";
    ofn.lpstrFile = filename;
    ofn.nMaxFile = MAX_PATH;
    ofn.Flags =
        OFN_FILEMUSTEXIST |
        OFN_PATHMUSTEXIST;
    ofn.lpstrDefExt = "cpp";

    if (!GetOpenFileNameA(&ofn))
        return;

    if (!script_module_compile_and_load(
            g_engine->script_module,
            filename)) {

        MessageBoxA(
            g_engine->window,
            "Failed to compile/load C++ script.\nCheck the console for compiler errors.",
            "C++ Script",
            MB_OK | MB_ICONERROR
        );
        return;
    }

    MessageBoxA(
        g_engine->window,
        "C++ script compiled and loaded.",
        "C++ Script",
        MB_OK | MB_ICONINFORMATION
    );
}

static HMENU create_main_menu()
{
    HMENU menu_bar =
        CreateMenu();

    HMENU file_menu =
        CreatePopupMenu();

    HMENU view_menu =
        CreatePopupMenu();

    HMENU animation_menu =
        CreatePopupMenu();

    HMENU help_menu =
        CreatePopupMenu();

    HMENU script_menu =
        CreatePopupMenu();

    AppendMenuA(
        file_menu,
        MF_STRING,
        ID_FILE_OPEN,
        "&Open Model...\tCtrl+O"
    );

    AppendMenuA(
        file_menu,
        MF_STRING,
        ID_FILE_RELOAD,
        "&Reload"
    );

    AppendMenuA(
        file_menu,
        MF_SEPARATOR,
        0,
        nullptr
    );

    AppendMenuA(
        file_menu,
        MF_STRING,
        ID_FILE_EXIT,
        "E&xit"
    );

    AppendMenuA(
        view_menu,
        MF_STRING | MF_CHECKED,
        ID_VIEW_TEXTURE,
        "&Texture"
    );

    AppendMenuA(
        view_menu,
        MF_STRING,
        ID_VIEW_WIREFRAME,
        "&Wireframe"
    );

    AppendMenuA(
        view_menu,
        MF_STRING,
        ID_VIEW_CUBE_MODEL,
        "Show &Model"
    );

    AppendMenuA(
        animation_menu,
        MF_STRING,
        ID_ANIM_PLAY_PAUSE,
        "&Play / Pause"
    );

    AppendMenuA(
        animation_menu,
        MF_STRING,
        ID_ANIM_PREVIOUS,
        "&Previous Clip"
    );

    AppendMenuA(
        animation_menu,
        MF_STRING,
        ID_ANIM_NEXT,
        "&Next Clip"
    );

    AppendMenuA(
        script_menu,
        MF_STRING,
        ID_SCRIPT_LOAD,
        "&Load C++ Script..."
    );

    AppendMenuA(
        script_menu,
        MF_STRING,
        ID_SCRIPT_UNLOAD,
        "&Unload Script"
    );

    AppendMenuA(
        help_menu,
        MF_STRING,
        ID_HELP_ABOUT,
        "&About"
    );

    AppendMenuA(
        menu_bar,
        MF_POPUP,
        (UINT_PTR)file_menu,
        "&File"
    );

    AppendMenuA(
        menu_bar,
        MF_POPUP,
        (UINT_PTR)view_menu,
        "&View"
    );

    AppendMenuA(
        menu_bar,
        MF_POPUP,
        (UINT_PTR)animation_menu,
        "&Animation"
    );

    AppendMenuA(
        menu_bar,
        MF_POPUP,
        (UINT_PTR)script_menu,
        "&Script"
    );

    AppendMenuA(
        menu_bar,
        MF_POPUP,
        (UINT_PTR)help_menu,
        "&Help"
    );

    return menu_bar;
}

static HWND make_static(
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
        WS_CHILD |
        WS_VISIBLE,
        x,
        y,
        w,
        h,
        parent,
        nullptr,
        GetModuleHandleA(nullptr),
        nullptr
    );
}

static HWND make_edit(
    HWND parent,
    int id,
    int x,
    int y,
    int w,
    int h,
    bool read_only
)
{
    DWORD style =
        WS_CHILD |
        WS_VISIBLE |
        WS_BORDER |
        ES_AUTOHSCROLL;

    if (read_only)
        style |= ES_READONLY;

    return CreateWindowExA(
        WS_EX_CLIENTEDGE,
        "EDIT",
        "",
        style,
        x,
        y,
        w,
        h,
        parent,
        (HMENU)(INT_PTR)id,
        GetModuleHandleA(nullptr),
        nullptr
    );
}


static LRESULT CALLBACK physics_panel_proc(
    HWND hwnd,
    UINT msg,
    WPARAM wparam,
    LPARAM lparam
)
{
    switch (msg) {
        case WM_COMMAND:
            /*
                Edit boxes / checkboxes / buttons send WM_COMMAND to
                their immediate parent. Forward it to the main window.
            */
            if (GetParent(hwnd)) {
                return SendMessageA(
                    GetParent(hwnd),
                    WM_COMMAND,
                    wparam,
                    lparam
                );
            }
            return 0;

        case WM_ERASEBKGND: {
            RECT rc{};
            GetClientRect(
                hwnd,
                &rc
            );

            FillRect(
                (HDC)wparam,
                &rc,
                GetSysColorBrush(
                    COLOR_BTNFACE
                )
            );

            return 1;
        }

        case WM_PAINT: {
            PAINTSTRUCT ps{};

            HDC dc =
                BeginPaint(
                    hwnd,
                    &ps
                );

            RECT client{};
            GetClientRect(
                hwnd,
                &client
            );

            FillRect(
                dc,
                &client,
                GetSysColorBrush(
                    COLOR_BTNFACE
                )
            );

            HFONT font =
                (HFONT)GetStockObject(
                    DEFAULT_GUI_FONT
                );

            HFONT old_font =
                (HFONT)SelectObject(
                    dc,
                    font
                );

            SetBkMode(
                dc,
                TRANSPARENT
            );

            SetTextColor(
                dc,
                GetSysColor(
                    COLOR_BTNTEXT
                )
            );

            /*
                Explicit GDI frame. This is NOT drawn on the OpenGL DC,
                so SwapBuffers cannot erase it.
            */
            RECT frame{
                5,
                10,
                client.right - 5,
                client.bottom - 5
            };

            HPEN pen =
                CreatePen(
                    PS_SOLID,
                    1,
                    GetSysColor(
                        COLOR_3DSHADOW
                    )
                );

            HPEN old_pen =
                (HPEN)SelectObject(
                    dc,
                    pen
                );

            HBRUSH old_brush =
                (HBRUSH)SelectObject(
                    dc,
                    GetStockObject(
                        NULL_BRUSH
                    )
                );

            Rectangle(
                dc,
                frame.left,
                frame.top,
                frame.right,
                frame.bottom
            );

            SelectObject(
                dc,
                old_brush
            );

            SelectObject(
                dc,
                old_pen
            );

            DeleteObject(
                pen
            );

            /*
                Paint a small BTNFACE patch behind the title so the
                top border looks like a classic Group Box.
            */
            RECT title_bg{
                14,
                3,
                145,
                19
            };

            FillRect(
                dc,
                &title_bg,
                GetSysColorBrush(
                    COLOR_BTNFACE
                )
            );

            TextOutA(
                dc,
                18,
                3,
                "Box Collider / Physics",
                22
            );

            SelectObject(
                dc,
                old_font
            );

            EndPaint(
                hwnd,
                &ps
            );

            return 0;
        }
    }

    return DefWindowProcA(
        hwnd,
        msg,
        wparam,
        lparam
    );
}

static void create_controls(
    Engine& engine
)
{
    HFONT font =
        (HFONT)GetStockObject(
            DEFAULT_GUI_FONT
        );

    engine.label_model =
        make_static(
            engine.window,
            "Model:",
            10, 8,
            45, 20
        );

    engine.edit_model_path =
        make_edit(
            engine.window,
            ID_EDIT_MODEL_PATH,
            58, 5,
            650, 24,
            false
        );

    engine.label_animation =
        make_static(
            engine.window,
            "Anim:",
            10, 39,
            40, 20
        );

    engine.edit_animation =
        make_edit(
            engine.window,
            ID_EDIT_ANIMATION,
            58, 35,
            65, 24,
            false
        );

    engine.label_speed =
        make_static(
            engine.window,
            "Speed:",
            135, 39,
            45, 20
        );

    engine.edit_speed =
        make_edit(
            engine.window,
            ID_EDIT_SPEED,
            183, 35,
            75, 24,
            false
        );

    engine.label_time =
        make_static(
            engine.window,
            "Time:",
            270, 39,
            40, 20
        );

    engine.edit_time =
        make_edit(
            engine.window,
            ID_EDIT_TIME,
            313, 35,
            85, 24,
            false
        );

    engine.button_apply =
        CreateWindowExA(
            0,
            "BUTTON",
            "Apply",
            WS_CHILD |
            WS_VISIBLE |
            BS_PUSHBUTTON,
            410, 34,
            75, 26,
            engine.window,
            (HMENU)(INT_PTR)
                ID_BUTTON_APPLY,
            GetModuleHandleA(nullptr),
            nullptr
        );


    /*
        Dedicated GDI child window.
        It has its own DC/paint cycle, separate from the OpenGL parent.
    */
    engine.group_collider =
        CreateWindowExA(
            WS_EX_CLIENTEDGE,
            "GL33PhysicsPanel",
            "",
            WS_CHILD |
            WS_VISIBLE |
            WS_CLIPCHILDREN |
            WS_CLIPSIBLINGS,
            10, 68,
            720, 230,
            engine.window,
            nullptr,
            GetModuleHandleA(nullptr),
            nullptr
        );

    engine.label_collider_size =
        make_static(
            engine.group_collider,
            "Size X/Y/Z:",
            14, 31,
            78, 20
        );

    engine.edit_collider_size_x =
        make_edit(
            engine.group_collider,
            ID_EDIT_COLLIDER_SIZE_X,
            92, 27,
            62, 24,
            false
        );

    engine.edit_collider_size_y =
        make_edit(
            engine.group_collider,
            ID_EDIT_COLLIDER_SIZE_Y,
            160, 27,
            62, 24,
            false
        );

    engine.edit_collider_size_z =
        make_edit(
            engine.group_collider,
            ID_EDIT_COLLIDER_SIZE_Z,
            228, 27,
            62, 24,
            false
        );

    engine.label_collider_offset =
        make_static(
            engine.group_collider,
            "Offset X/Y/Z:",
            14, 64,
            78, 20
        );

    engine.edit_collider_offset_x =
        make_edit(
            engine.group_collider,
            ID_EDIT_COLLIDER_OFFSET_X,
            92, 60,
            62, 24,
            false
        );

    engine.edit_collider_offset_y =
        make_edit(
            engine.group_collider,
            ID_EDIT_COLLIDER_OFFSET_Y,
            160, 60,
            62, 24,
            false
        );

    engine.edit_collider_offset_z =
        make_edit(
            engine.group_collider,
            ID_EDIT_COLLIDER_OFFSET_Z,
            228, 60,
            62, 24,
            false
        );

    engine.check_gravity =
        CreateWindowExA(
            0,
            "BUTTON",
            "Gravity",
            WS_CHILD |
            WS_VISIBLE |
            BS_AUTOCHECKBOX,
            310, 28,
            90, 22,
            engine.group_collider,
            (HMENU)(INT_PTR)
                ID_CHECK_GRAVITY,
            GetModuleHandleA(nullptr),
            nullptr
        );

    engine.check_show_collider =
        CreateWindowExA(
            0,
            "BUTTON",
            "Show Collider",
            WS_CHILD |
            WS_VISIBLE |
            BS_AUTOCHECKBOX,
            410, 28,
            110, 22,
            engine.group_collider,
            (HMENU)(INT_PTR)
                ID_CHECK_SHOW_COLLIDER,
            GetModuleHandleA(nullptr),
            nullptr
        );

    engine.button_physics_reset =
        CreateWindowExA(
            0,
            "BUTTON",
            "Reset Physics",
            WS_CHILD |
            WS_VISIBLE |
            BS_PUSHBUTTON,
            310, 59,
            105, 26,
            engine.group_collider,
            (HMENU)(INT_PTR)
                ID_BUTTON_PHYSICS_RESET,
            GetModuleHandleA(nullptr),
            nullptr
        );


    engine.label_projectile_speed =
        make_static(
            engine.group_collider,
            "Bullet Speed:",
            14, 98,
            78, 20
        );

    engine.edit_projectile_speed =
        make_edit(
            engine.group_collider,
            ID_EDIT_PROJECTILE_SPEED,
            92, 94,
            62, 24,
            false
        );

    engine.label_projectile_radius =
        make_static(
            engine.group_collider,
            "Radius:",
            166, 98,
            48, 20
        );

    engine.edit_projectile_radius =
        make_edit(
            engine.group_collider,
            ID_EDIT_PROJECTILE_RADIUS,
            218, 94,
            62, 24,
            false
        );

    engine.button_shoot =
        CreateWindowExA(
            0,
            "BUTTON",
            "Shoot [F]",
            WS_CHILD |
            WS_VISIBLE |
            BS_PUSHBUTTON,
            310, 93,
            90, 26,
            engine.group_collider,
            (HMENU)(INT_PTR)
                ID_BUTTON_SHOOT,
            GetModuleHandleA(nullptr),
            nullptr
        );

    engine.button_clear_projectiles =
        CreateWindowExA(
            0,
            "BUTTON",
            "Clear Balls",
            WS_CHILD |
            WS_VISIBLE |
            BS_PUSHBUTTON,
            410, 93,
            100, 26,
            engine.group_collider,
            (HMENU)(INT_PTR)
                ID_BUTTON_CLEAR_PROJECTILES,
            GetModuleHandleA(nullptr),
            nullptr
        );


    engine.check_ai_enabled =
        CreateWindowExA(
            0,
            "BUTTON",
            "AI Waypoint",
            WS_CHILD |
            WS_VISIBLE |
            BS_AUTOCHECKBOX,
            14, 128,
            100, 22,
            engine.group_collider,
            (HMENU)(INT_PTR)
                ID_CHECK_AI_ENABLED,
            GetModuleHandleA(nullptr),
            nullptr
        );

    engine.check_ai_waypoints =
        CreateWindowExA(
            0,
            "BUTTON",
            "Show Path",
            WS_CHILD |
            WS_VISIBLE |
            BS_AUTOCHECKBOX,
            122, 128,
            92, 22,
            engine.group_collider,
            (HMENU)(INT_PTR)
                ID_CHECK_AI_WAYPOINTS,
            GetModuleHandleA(nullptr),
            nullptr
        );

    engine.label_ai_speed =
        make_static(
            engine.group_collider,
            "AI Speed:",
            224, 130,
            58, 20
        );

    engine.edit_ai_speed =
        make_edit(
            engine.group_collider,
            ID_EDIT_AI_SPEED,
            282, 125,
            62, 24,
            false
        );

    engine.button_ai_reset =
        CreateWindowExA(
            0,
            "BUTTON",
            "Reset AI",
            WS_CHILD |
            WS_VISIBLE |
            BS_PUSHBUTTON,
            356, 124,
            90, 26,
            engine.group_collider,
            (HMENU)(INT_PTR)
                ID_BUTTON_AI_RESET,
            GetModuleHandleA(nullptr),
            nullptr
        );


    engine.label_ammo =
        make_static(
            engine.group_collider,
            "Ammo: 10 / 10",
            14, 160,
            120, 22
        );

    engine.label_ammo_hint =
        make_static(
            engine.group_collider,
            "Orange weapon item = +5 shots (max 10)",
            142, 160,
            280, 22
        );


    engine.label_health =
        make_static(
            engine.group_collider,
            "Health: 100 / 100",
            14, 184,
            130, 22
        );

    engine.label_ai_attack =
        make_static(
            engine.group_collider,
            "AI Attack: <= 20m | red bullets = hostile",
            150, 184,
            300, 22
        );

    HWND controls[] = {
        engine.label_model,
        engine.edit_model_path,
        engine.label_animation,
        engine.edit_animation,
        engine.label_speed,
        engine.edit_speed,
        engine.label_time,
        engine.edit_time,
        engine.button_apply,

        engine.group_collider,
        engine.label_collider_size,
        engine.edit_collider_size_x,
        engine.edit_collider_size_y,
        engine.edit_collider_size_z,
        engine.label_collider_offset,
        engine.edit_collider_offset_x,
        engine.edit_collider_offset_y,
        engine.edit_collider_offset_z,
        engine.check_gravity,
        engine.check_show_collider,
        engine.button_physics_reset,

        engine.label_projectile_speed,
        engine.edit_projectile_speed,
        engine.label_projectile_radius,
        engine.edit_projectile_radius,
        engine.button_shoot,
        engine.button_clear_projectiles,

        engine.check_ai_enabled,
        engine.check_ai_waypoints,
        engine.label_ai_speed,
        engine.edit_ai_speed,
        engine.button_ai_reset,

        engine.label_ammo,
        engine.label_ammo_hint,
        engine.label_health,
        engine.label_ai_attack
    };

    for (HWND control : controls) {
        if (control) {
            SendMessageA(
                control,
                WM_SETFONT,
                (WPARAM)font,
                TRUE
            );
        }
    }

    SetWindowTextA(
        engine.edit_animation,
        "0"
    );

    SetWindowTextA(
        engine.edit_speed,
        "1.0"
    );

    SetWindowTextA(
        engine.edit_time,
        "0.0"
    );

    SetWindowTextA(
        engine.edit_collider_size_x,
        "1.0"
    );

    SetWindowTextA(
        engine.edit_collider_size_y,
        "2.0"
    );

    SetWindowTextA(
        engine.edit_collider_size_z,
        "1.0"
    );

    SetWindowTextA(
        engine.edit_collider_offset_x,
        "0.0"
    );

    SetWindowTextA(
        engine.edit_collider_offset_y,
        "0.0"
    );

    SetWindowTextA(
        engine.edit_collider_offset_z,
        "0.0"
    );

    SendMessageA(
        engine.check_gravity,
        BM_SETCHECK,
        BST_UNCHECKED,
        0
    );

    SendMessageA(
        engine.check_show_collider,
        BM_SETCHECK,
        BST_CHECKED,
        0
    );

    SetWindowTextA(
        engine.edit_projectile_speed,
        "12.0"
    );

    SetWindowTextA(
        engine.edit_projectile_radius,
        "0.12"
    );

    SendMessageA(
        engine.check_ai_enabled,
        BM_SETCHECK,
        BST_CHECKED,
        0
    );

    SendMessageA(
        engine.check_ai_waypoints,
        BM_SETCHECK,
        BST_CHECKED,
        0
    );

    SetWindowTextA(
        engine.edit_ai_speed,
        "2.0"
    );
}

static void layout_controls(
    Engine& engine,
    int width
)
{
    const int margin = 10;

    int model_edit_width =
        width - 58 - margin;

    if (model_edit_width < 200)
        model_edit_width = 200;

    MoveWindow(
        engine.edit_model_path,
        58,
        5,
        model_edit_width,
        24,
        TRUE
    );

    if (engine.group_collider) {
        int group_width =
            width - 20;

        if (group_width < 520)
            group_width = 520;

        MoveWindow(
            engine.group_collider,
            10,
            68,
            group_width,
            230,
            TRUE
        );
    }
}

static void apply_edit_boxes(
    Engine& engine
)
{
    const std::string path =
        get_edit_text(
            engine.edit_model_path
        );

    if (!path.empty() &&
        path != engine.renderer.loaded_file) {
        load_model_path(
            engine,
            path
        );
    }

    const std::string anim =
        get_edit_text(
            engine.edit_animation
        );

    const std::string speed =
        get_edit_text(
            engine.edit_speed
        );

    const std::string time =
        get_edit_text(
            engine.edit_time
        );

    renderer_animation_set_index(
        engine.renderer,
        std::atoi(
            anim.c_str()
        )
    );

    renderer_animation_set_speed(
        engine.renderer,
        (float)std::atof(
            speed.c_str()
        )
    );

    renderer_animation_set_time(
        engine.renderer,
        (float)std::atof(
            time.c_str()
        )
    );

    const float size_x =
        (float)std::atof(
            get_edit_text(
                engine.edit_collider_size_x
            ).c_str()
        );

    const float size_y =
        (float)std::atof(
            get_edit_text(
                engine.edit_collider_size_y
            ).c_str()
        );

    const float size_z =
        (float)std::atof(
            get_edit_text(
                engine.edit_collider_size_z
            ).c_str()
        );

    const float offset_x =
        (float)std::atof(
            get_edit_text(
                engine.edit_collider_offset_x
            ).c_str()
        );

    const float offset_y =
        (float)std::atof(
            get_edit_text(
                engine.edit_collider_offset_y
            ).c_str()
        );

    const float offset_z =
        (float)std::atof(
            get_edit_text(
                engine.edit_collider_offset_z
            ).c_str()
        );

    const bool gravity_enabled =
        SendMessageA(
            engine.check_gravity,
            BM_GETCHECK,
            0,
            0
        ) == BST_CHECKED;

    const bool show_collider =
        SendMessageA(
            engine.check_show_collider,
            BM_GETCHECK,
            0,
            0
        ) == BST_CHECKED;

    renderer_set_box_collider(
        engine.renderer,
        size_x,
        size_y,
        size_z,
        offset_x,
        offset_y,
        offset_z,
        gravity_enabled,
        show_collider
    );

    const float projectile_speed =
        (float)std::atof(
            get_edit_text(
                engine.edit_projectile_speed
            ).c_str()
        );

    const float projectile_radius =
        (float)std::atof(
            get_edit_text(
                engine.edit_projectile_radius
            ).c_str()
        );

    renderer_set_projectile_options(
        engine.renderer,
        projectile_speed,
        projectile_radius
    );

    const bool ai_enabled =
        SendMessageA(
            engine.check_ai_enabled,
            BM_GETCHECK,
            0,
            0
        ) == BST_CHECKED;

    const bool show_ai_waypoints =
        SendMessageA(
            engine.check_ai_waypoints,
            BM_GETCHECK,
            0,
            0
        ) == BST_CHECKED;

    const float ai_speed =
        (float)std::atof(
            get_edit_text(
                engine.edit_ai_speed
            ).c_str()
        );

    renderer_set_ai_options(
        engine.renderer,
        ai_enabled,
        show_ai_waypoints,
        ai_speed
    );

    sync_ui_from_renderer(engine);
}

static LRESULT CALLBACK wnd_proc(
    HWND hwnd,
    UINT msg,
    WPARAM wparam,
    LPARAM lparam
)
{
    Engine* engine =
        (Engine*)GetWindowLongPtrA(
            hwnd,
            GWLP_USERDATA
        );

    switch (msg) {
        case WM_NCCREATE: {
            CREATESTRUCTA* cs =
                (CREATESTRUCTA*)lparam;

            SetWindowLongPtrA(
                hwnd,
                GWLP_USERDATA,
                (LONG_PTR)cs->lpCreateParams
            );

            return TRUE;
        }

        case WM_SIZE:
            if (engine) {
                engine->width =
                    LOWORD(lparam);

                engine->height =
                    HIWORD(lparam);

                layout_controls(
                    *engine,
                    engine->width
                );

                if (engine->group_collider) {
                    RedrawWindow(
                        engine->group_collider,
                        nullptr,
                        nullptr,
                        RDW_INVALIDATE |
                        RDW_ERASE |
                        RDW_UPDATENOW |
                        RDW_ALLCHILDREN
                    );
                }
            }
            return 0;

        case WM_COMMAND:
            if (!engine)
                break;

            switch (LOWORD(wparam)) {
                case ID_FILE_OPEN:
                    open_model_dialog();
                    return 0;

                case ID_FILE_RELOAD:
                    if (!engine->renderer.loaded_file.empty()) {
                        load_model_path(
                            *engine,
                            engine->renderer.loaded_file
                        );
                    }
                    return 0;

                case ID_FILE_EXIT:
                    DestroyWindow(hwnd);
                    return 0;

                case ID_VIEW_TEXTURE:
                    renderer_toggle_texture(
                        engine->renderer
                    );
                    sync_menu_checks(*engine);
                    return 0;

                case ID_VIEW_WIREFRAME:
                    renderer_toggle_wireframe(
                        engine->renderer
                    );
                    sync_menu_checks(*engine);
                    return 0;

                case ID_VIEW_CUBE_MODEL:
                    renderer_toggle_cube_model(
                        engine->renderer
                    );
                    sync_menu_checks(*engine);
                    return 0;

                case ID_ANIM_PLAY_PAUSE:
                    renderer_animation_play_pause(
                        engine->renderer
                    );
                    sync_ui_from_renderer(*engine);
                    return 0;

                case ID_ANIM_PREVIOUS:
                    renderer_animation_previous(
                        engine->renderer
                    );
                    sync_ui_from_renderer(*engine);
                    return 0;

                case ID_ANIM_NEXT:
                    renderer_animation_next(
                        engine->renderer
                    );
                    sync_ui_from_renderer(*engine);
                    return 0;

                case ID_BUTTON_APPLY:
                    apply_edit_boxes(*engine);
                    return 0;


                case ID_CHECK_GRAVITY:
                case ID_CHECK_SHOW_COLLIDER:
                case ID_CHECK_AI_ENABLED:
                case ID_CHECK_AI_WAYPOINTS:
                    /*
                        Apply checkboxes immediately, while preserving
                        current edit-box values.
                    */
                    apply_edit_boxes(*engine);
                    return 0;

                case ID_BUTTON_PHYSICS_RESET:
                    renderer_physics_reset(
                        engine->renderer
                    );
                    sync_ui_from_renderer(*engine);
                    return 0;


                case ID_BUTTON_SHOOT:
                    /*
                        Read the latest speed/radius before firing.
                    */
                    apply_edit_boxes(*engine);

                    renderer_fire_projectile(
                        engine->renderer
                    );

                    sync_runtime_ammo_label(
                        *engine
                    );

                    return 0;

                case ID_BUTTON_CLEAR_PROJECTILES:
                    renderer_clear_projectiles(
                        engine->renderer
                    );

                    return 0;


                case ID_BUTTON_AI_RESET:
                    renderer_reset_ai(
                        engine->renderer
                    );

                    sync_ui_from_renderer(
                        *engine
                    );

                    return 0;

                case ID_SCRIPT_LOAD:
                    open_cpp_script_dialog();
                    return 0;

                case ID_SCRIPT_UNLOAD:
                    script_module_unload(
                        engine->script_module
                    );
                    return 0;

                case ID_HELP_ABOUT:
                    MessageBoxA(
                        hwnd,
                        "OpenGL 3.3 Model Engine\n"
                        "GLB + FBX\n"
                        "Win32 menus and Edit controls\n"
                        "FBX animation data reader\n"
                        "F = Shoot (character forward)\n"
                        "W/Up = Forward\n"
                        "S/Down = Backward\n"
                        "A/Left = Turn Left\n"
                        "D/Right = Turn Right\n"
                        "RMB Drag = Orbit Camera\n"
                        "Ground plane = 100x100\n"
                        "Waypoint AI uses loaded model\n"
                        "Follow camera tracks position only\n"
                        "RMB rotates camera independently\n"
                        "AI attacks player at <=20m\n"
                        "Ammo max 10; orange item restores +5",
                        "About",
                        MB_OK |
                        MB_ICONINFORMATION
                    );
                    return 0;
            }
            break;

        case WM_RBUTTONDOWN:
            if (!engine)
                break;

            engine->orbit_dragging = true;

            engine->orbit_last_x =
                GET_X_LPARAM(lparam);

            engine->orbit_last_y =
                GET_Y_LPARAM(lparam);

            /*
                Continue receiving mouse moves even if the cursor
                leaves the client area during an orbit drag.
            */
            SetCapture(hwnd);

            SetFocus(hwnd);

            return 0;

        case WM_MOUSEMOVE:
            if (!engine)
                break;

            if (engine->orbit_dragging &&
                (wparam & MK_RBUTTON)) {

                const int x =
                    GET_X_LPARAM(lparam);

                const int y =
                    GET_Y_LPARAM(lparam);

                const int dx =
                    x -
                    engine->orbit_last_x;

                const int dy =
                    y -
                    engine->orbit_last_y;

                engine->orbit_last_x = x;
                engine->orbit_last_y = y;

                renderer_orbit_drag(
                    engine->renderer,
                    (float)dx,
                    (float)dy
                );

                return 0;
            }
            break;

        case WM_RBUTTONUP:
            if (!engine)
                break;

            if (engine->orbit_dragging) {
                engine->orbit_dragging =
                    false;

                if (GetCapture() == hwnd)
                    ReleaseCapture();
            }

            return 0;

        case WM_CAPTURECHANGED:
            if (engine) {
                engine->orbit_dragging =
                    false;
            }
            break;

        case WM_RBUTTONDBLCLK:
            if (!engine)
                break;

            renderer_orbit_reset(
                engine->renderer
            );

            return 0;

        case WM_KEYDOWN:
            if (!engine)
                break;

            if (wparam == VK_ESCAPE) {
                DestroyWindow(hwnd);
                return 0;
            }

            if (wparam == VK_SPACE) {
                renderer_animation_play_pause(
                    engine->renderer
                );
                sync_ui_from_renderer(*engine);
                return 0;
            }

            /*
                F is polled with GetAsyncKeyState() in engine_run().
                This keeps shooting independent from RMB/orbit and from
                which child GDI control currently owns keyboard focus.
            */
            if (wparam == 'F') {
                return 0;
            }

            if (wparam == 'W' ||
                wparam == VK_UP) {
                engine->move_forward = true;
                return 0;
            }

            if (wparam == 'S' ||
                wparam == VK_DOWN) {
                engine->move_backward = true;
                return 0;
            }

            if (wparam == 'A' ||
                wparam == VK_LEFT) {
                engine->turn_left = true;
                return 0;
            }

            if (wparam == 'D' ||
                wparam == VK_RIGHT) {
                engine->turn_right = true;
                return 0;
            }

            if (wparam == VK_F1) {
                open_model_dialog();
                return 0;
            }

            if (wparam == VK_F2) {
                renderer_toggle_texture(
                    engine->renderer
                );
                sync_menu_checks(*engine);
                return 0;
            }

            if (wparam == VK_F3) {
                renderer_toggle_wireframe(
                    engine->renderer
                );
                sync_menu_checks(*engine);
                return 0;
            }

            if (wparam == VK_F4) {
                renderer_toggle_cube_model(
                    engine->renderer
                );
                sync_menu_checks(*engine);
                return 0;
            }
            break;

        case WM_KEYUP:
            if (!engine)
                break;

            if (wparam == 'W' ||
                wparam == VK_UP) {
                engine->move_forward = false;
                return 0;
            }

            if (wparam == 'S' ||
                wparam == VK_DOWN) {
                engine->move_backward = false;
                return 0;
            }

            if (wparam == 'A' ||
                wparam == VK_LEFT) {
                engine->turn_left = false;
                return 0;
            }

            if (wparam == 'D' ||
                wparam == VK_RIGHT) {
                engine->turn_right = false;
                return 0;
            }

            break;

        case WM_KILLFOCUS:
            if (engine) {
                engine->move_forward = false;
                engine->move_backward = false;
                engine->turn_left = false;
                engine->turn_right = false;
                engine->fire_key_down = false;
            }
            break;

        case WM_CLOSE:
            DestroyWindow(hwnd);
            return 0;

        case WM_DESTROY:
            if (engine)
                engine->running = false;

            PostQuitMessage(0);
            return 0;
    }

    return DefWindowProcA(
        hwnd,
        msg,
        wparam,
        lparam
    );
}

static bool set_pixel_format(
    HDC dc
)
{
    PIXELFORMATDESCRIPTOR pfd{};

    pfd.nSize = sizeof(pfd);
    pfd.nVersion = 1;

    pfd.dwFlags =
        PFD_DRAW_TO_WINDOW |
        PFD_SUPPORT_OPENGL |
        PFD_DOUBLEBUFFER;

    pfd.iPixelType =
        PFD_TYPE_RGBA;

    pfd.cColorBits = 32;
    pfd.cDepthBits = 24;
    pfd.cStencilBits = 8;
    pfd.iLayerType =
        PFD_MAIN_PLANE;

    const int format =
        ChoosePixelFormat(
            dc,
            &pfd
        );

    if (!format)
        return false;

    return SetPixelFormat(
        dc,
        format,
        &pfd
    ) != FALSE;
}

static bool create_context(
    Engine& engine
)
{
    HGLRC temporary =
        wglCreateContext(
            engine.dc
        );

    if (!temporary)
        return false;

    if (!wglMakeCurrent(
            engine.dc,
            temporary)) {

        wglDeleteContext(
            temporary
        );

        return false;
    }

    PROC raw =
        wglGetProcAddress(
            "wglCreateContextAttribsARB"
        );

    WglCreateContextAttribsARB create =
        nullptr;

    if (raw) {
        static_assert(
            sizeof(raw) ==
            sizeof(create),
            "pointer size mismatch"
        );

        std::memcpy(
            &create,
            &raw,
            sizeof(create)
        );
    }

    if (!create) {
        wglMakeCurrent(
            nullptr,
            nullptr
        );

        wglDeleteContext(
            temporary
        );

        return false;
    }

    const int attribs[] = {
        WGL_CONTEXT_MAJOR_VERSION_ARB, 3,
        WGL_CONTEXT_MINOR_VERSION_ARB, 3,
        WGL_CONTEXT_PROFILE_MASK_ARB,
        WGL_CONTEXT_CORE_PROFILE_BIT_ARB,
        0
    };

    engine.gl_context =
        create(
            engine.dc,
            nullptr,
            attribs
        );

    wglMakeCurrent(
        nullptr,
        nullptr
    );

    wglDeleteContext(
        temporary
    );

    if (!engine.gl_context)
        return false;

    return wglMakeCurrent(
        engine.dc,
        engine.gl_context
    ) != FALSE;
}

static void try_vsync()
{
    PROC raw =
        wglGetProcAddress(
            "wglSwapIntervalEXT"
        );

    WglSwapIntervalEXT swap =
        nullptr;

    if (!raw)
        return;

    static_assert(
        sizeof(raw) ==
        sizeof(swap),
        "pointer size mismatch"
    );

    std::memcpy(
        &swap,
        &raw,
        sizeof(swap)
    );

    if (swap)
        swap(1);
}

bool engine_init(
    Engine& engine,
    HINSTANCE instance,
    int width,
    int height
)
{
    HRESULT com_hr =
        CoInitializeEx(
            nullptr,
            COINIT_APARTMENTTHREADED
        );

    if (FAILED(com_hr) &&
        com_hr != RPC_E_CHANGED_MODE) {
        return false;
    }

    engine.instance = instance;
    engine.width = width;
    engine.height = height;

    WNDCLASSA wc{};
    wc.style = CS_OWNDC;
    wc.lpfnWndProc = wnd_proc;
    wc.hInstance = instance;
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
        "GL33ModelEngineWindow";

    if (!RegisterClassA(&wc)) {
        if (GetLastError() !=
            ERROR_CLASS_ALREADY_EXISTS) {
            return false;
        }
    }

    /*
        Separate GDI class for the physics panel.
        This window never receives an OpenGL pixel format.
    */
    WNDCLASSA panel_wc{};
    panel_wc.style =
        CS_HREDRAW |
        CS_VREDRAW;

    panel_wc.lpfnWndProc =
        physics_panel_proc;

    panel_wc.hInstance =
        instance;

    panel_wc.hCursor =
        LoadCursorA(
            nullptr,
            IDC_ARROW
        );

    panel_wc.hbrBackground =
        (HBRUSH)(
            COLOR_BTNFACE + 1
        );

    panel_wc.lpszClassName =
        "GL33PhysicsPanel";

    if (!RegisterClassA(
            &panel_wc)) {

        if (GetLastError() !=
            ERROR_CLASS_ALREADY_EXISTS) {
            return false;
        }
    }

    RECT rect{
        0,
        0,
        width,
        height
    };

    const DWORD main_window_style =
        WS_OVERLAPPEDWINDOW |
        WS_CLIPCHILDREN |
        WS_CLIPSIBLINGS;

    AdjustWindowRect(
        &rect,
        main_window_style,
        TRUE
    );

    engine.window =
        CreateWindowExA(
            0,
            wc.lpszClassName,
            "OpenGL 3.3 Model Engine",
            main_window_style,
            CW_USEDEFAULT,
            CW_USEDEFAULT,
            rect.right -
                rect.left,
            rect.bottom -
                rect.top,
            nullptr,
            nullptr,
            instance,
            &engine
        );

    if (!engine.window)
        return false;

    engine.menu_bar =
        create_main_menu();

    SetMenu(
        engine.window,
        engine.menu_bar
    );

    create_controls(
        engine
    );

    engine.dc =
        GetDC(
            engine.window
        );

    if (!engine.dc)
        return false;

    if (!set_pixel_format(
            engine.dc)) {
        return false;
    }

    if (!create_context(
            engine)) {
        return false;
    }

    if (!gl33_load())
        return false;

    std::printf(
        "Vendor   : %s\n"
        "Renderer : %s\n"
        "OpenGL   : %s\n"
        "GLSL     : %s\n",
        (const char*)
            glGetString(GL_VENDOR),
        (const char*)
            glGetString(GL_RENDERER),
        (const char*)
            glGetString(GL_VERSION),
        (const char*)
            glGetString(
                GL_SHADING_LANGUAGE_VERSION
            )
    );

    try_vsync();

    if (!renderer_init(
            engine.renderer)) {
        return false;
    }

    /*
        Component scene is independent from the legacy renderer/player
        state. MeshRenderer instances share renderer GPU resources.
    */
    engine.renderer.scene =
        &engine.scene;

    engine.scene.physics.ground_y =
        engine.renderer.collider.ground_y;

    GameObject* physics_cube =
        engine.scene.CreateGameObject(
            "PhysicsCube"
        );

    physics_cube->transform.position = {
        4.0f,
        6.0f,
        0.0f
    };

    physics_cube->transform.scale = {
        0.60f,
        0.60f,
        0.60f
    };

    MeshRendererComponent* cube_renderer =
        physics_cube->AddComponent<
            MeshRendererComponent
        >();

    cube_renderer->source =
        MeshRendererSource::Cube;

    RigidbodyComponent* cube_body =
        physics_cube->AddComponent<
            RigidbodyComponent
        >();

    cube_body->mass =
        2.0f;

    cube_body->use_gravity =
        true;

    BoxColliderComponent* cube_collider =
        physics_cube->AddComponent<
            BoxColliderComponent
        >();

    cube_collider->size = {
        1.20f,
        1.20f,
        1.20f
    };

    g_engine = &engine;

    sync_ui_from_renderer(
        engine
    );

    sync_runtime_ammo_label(
        engine
    );

    sync_menu_checks(
        engine
    );

    ShowWindow(
        engine.window,
        SW_SHOW
    );

    UpdateWindow(
        engine.window
    );

    if (engine.group_collider) {
        ShowWindow(
            engine.group_collider,
            SW_SHOW
        );

        RedrawWindow(
            engine.group_collider,
            nullptr,
            nullptr,
            RDW_INVALIDATE |
            RDW_ERASE |
            RDW_UPDATENOW |
            RDW_ALLCHILDREN
        );
    }

    engine.running = true;

    /*
        Optional default sample.
        Ignore error if the file does not exist.
    */
    load_model_path(
        engine,
        "assets\\sample.glb"
    );

    return true;
}

int engine_run(
    Engine& engine
)
{
    LARGE_INTEGER frequency{};
    LARGE_INTEGER previous{};

    QueryPerformanceFrequency(
        &frequency
    );

    QueryPerformanceCounter(
        &previous
    );

    MSG msg{};

    while (engine.running) {
        while (PeekMessageA(
            &msg,
            nullptr,
            0,
            0,
            PM_REMOVE
        )) {
            if (msg.message ==
                WM_QUIT) {
                engine.running =
                    false;
                break;
            }

            TranslateMessage(
                &msg
            );

            DispatchMessageA(
                &msg
            );
        }

        if (!engine.running)
            break;

        if (IsIconic(
                engine.window)) {
            Sleep(16);
            continue;
        }

        LARGE_INTEGER now{};
        QueryPerformanceCounter(
            &now
        );

        const float dt =
            (float)(
                (double)(
                    now.QuadPart -
                    previous.QuadPart
                ) /
                (double)
                    frequency.QuadPart
            );

        previous = now;

        if (engine.renderer.animation_playing) {
            engine.renderer.animation_time +=
                dt *
                engine.renderer.animation_speed;
        }

        /*
            Global-to-this-window F polling.
            No right-click/orbit interaction is required to shoot.
            It also works while a child Edit Box has focus.
        */
        const bool app_foreground =
            GetForegroundWindow() ==
            engine.window;

        const bool f_pressed =
            app_foreground &&
            ((GetAsyncKeyState('F') &
              0x8000) != 0);

        if (f_pressed &&
            !engine.fire_key_down) {

            renderer_fire_projectile(
                engine.renderer
            );
        }

        engine.fire_key_down =
            f_pressed;

        float forward_axis = 0.0f;
        float turn_axis = 0.0f;

        if (engine.move_forward)
            forward_axis += 1.0f;

        if (engine.move_backward)
            forward_axis -= 1.0f;

        if (engine.turn_left)
            turn_axis += 1.0f;

        if (engine.turn_right)
            turn_axis -= 1.0f;

        renderer_move_character(
            engine.renderer,
            forward_axis,
            turn_axis,
            dt
        );

        renderer_update_ai(
            engine.renderer,
            dt
        );

        /*
            Runtime C++ scripts operate directly on the transform used
            to draw the loaded player/model. This makes ScriptUpdate()
            immediately visible in the OpenGL scene.
        */
        ScriptContext script_context{};
        script_context.object_x =
            &engine.renderer.collider.body_x;
        script_context.object_y =
            &engine.renderer.collider.body_y;
        script_context.object_z =
            &engine.renderer.collider.body_z;
        script_context.object_yaw =
            &engine.renderer.collider.body_yaw;

        script_module_update(
            engine.script_module,
            script_context,
            dt
        );

        /*
            Keep the legacy script transform mirrors synchronized for
            debugging/UI code that may still inspect Engine directly.
        */
        engine.script_object_x =
            engine.renderer.collider.body_x;
        engine.script_object_y =
            engine.renderer.collider.body_y;
        engine.script_object_z =
            engine.renderer.collider.body_z;
        engine.script_object_yaw =
            engine.renderer.collider.body_yaw;

        renderer_physics_step(
            engine.renderer,
            dt
        );

        /*
            Unity-style component update:
            Component::Start/Update -> PhysicsWorld -> Transform sync.
        */
        engine.scene.Update(
            dt
        );

        sync_runtime_ammo_label(
            engine
        );

        renderer_draw(
            engine.renderer,
            engine.width,
            engine.height,
            engine.renderer.animation_time
        );

        SwapBuffers(
            engine.dc
        );
    }

    return 0;
}

void engine_shutdown(
    Engine& engine
)
{
    g_engine = nullptr;

    script_module_unload(
        engine.script_module
    );

    engine.scene.Clear();
    engine.renderer.scene =
        nullptr;

    if (engine.gl_context) {
        wglMakeCurrent(
            engine.dc,
            engine.gl_context
        );

        renderer_shutdown(
            engine.renderer
        );

        wglMakeCurrent(
            nullptr,
            nullptr
        );

        wglDeleteContext(
            engine.gl_context
        );

        engine.gl_context =
            nullptr;
    }

    if (engine.dc &&
        engine.window) {
        ReleaseDC(
            engine.window,
            engine.dc
        );

        engine.dc =
            nullptr;
    }

    if (engine.window &&
        IsWindow(
            engine.window)) {
        DestroyWindow(
            engine.window
        );
    }

    engine.window =
        nullptr;

    if (engine.menu_bar) {
        DestroyMenu(
            engine.menu_bar
        );
        engine.menu_bar =
            nullptr;
    }

    if (engine.instance) {
        UnregisterClassA(
            "GL33PhysicsPanel",
            engine.instance
        );

        UnregisterClassA(
            "GL33ModelEngineWindow",
            engine.instance
        );
    }

    CoUninitialize();
}
