#include "cpp_script_component.h"

CppScriptComponent::~CppScriptComponent()
{
    Unload();
}

bool CppScriptComponent::Load(
    const std::string& cpp_path
)
{
    source_path =
        cpp_path;

    return script_module_compile_and_load(
        module,
        cpp_path
    );
}

void CppScriptComponent::Unload()
{
    script_module_unload(
        module
    );
}

void CppScriptComponent::Start()
{
    if (auto_load &&
        !source_path.empty() &&
        !module.library) {

        Load(source_path);
    }
}

void CppScriptComponent::Update(
    float dt
)
{
    if (!module.library)
        return;

    ScriptContext context{};

    context.object_x =
        &transform().position.x;

    context.object_y =
        &transform().position.y;

    context.object_z =
        &transform().position.z;

    context.object_yaw =
        &transform().rotation.y;

    context.object_rotation_x =
        &transform().rotation.x;

    context.object_rotation_y =
        &transform().rotation.y;

    context.object_rotation_z =
        &transform().rotation.z;

    context.object_scale_x =
        &transform().scale.x;

    context.object_scale_y =
        &transform().scale.y;

    context.object_scale_z =
        &transform().scale.z;

    script_module_update(
        module,
        context,
        dt
    );
}

void CppScriptComponent::OnDestroy()
{
    Unload();
}
