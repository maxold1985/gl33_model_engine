#pragma once

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include <string>

#include "script_api.h"

struct ScriptModule {
    HMODULE library = nullptr;
    ScriptStartFn start = nullptr;
    ScriptUpdateFn update = nullptr;

    std::string source_path;
    std::string dll_path;

    float total_time = 0.0f;
    bool started = false;
};

bool script_module_compile_and_load(
    ScriptModule& module,
    const std::string& cpp_path
);

void script_module_update(
    ScriptModule& module,
    ScriptContext& context,
    float dt
);

void script_module_unload(
    ScriptModule& module
);
