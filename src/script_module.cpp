#include "script_module.h"

#include <cstdio>
#include <cstdlib>

static unsigned long g_script_instance_id = 0;

static std::string quote(
    const std::string& value
)
{
    return "\"" + value + "\"";
}

void script_module_unload(
    ScriptModule& module
)
{
    const std::string old_dll_path =
        module.dll_path;

    if (module.library)
        FreeLibrary(module.library);

    module = ScriptModule{};

    if (!old_dll_path.empty())
        DeleteFileA(
            old_dll_path.c_str()
        );
}

bool script_module_compile_and_load(
    ScriptModule& module,
    const std::string& cpp_path
)
{
    script_module_unload(module);

    char temp_path[MAX_PATH] = {};
    GetTempPathA(
        MAX_PATH,
        temp_path
    );

    const unsigned long instance_id =
        ++g_script_instance_id;

    const std::string dll_path =
        std::string(temp_path) +
        "gl33_runtime_script_" +
        std::to_string(instance_id) +
        ".dll";

    char exe_path[MAX_PATH] = {};
    GetModuleFileNameA(
        nullptr,
        exe_path,
        MAX_PATH
    );

    std::string exe_dir =
        exe_path;

    const std::size_t slash =
        exe_dir.find_last_of(
            "\\/"
        );

    if (slash != std::string::npos)
        exe_dir.resize(slash);

    /*
        build/model_engine.exe -> project root is one level above build.
        The script includes script_api.h from ../src.
    */
    const std::string include_dir =
        exe_dir + "\\..\\src";

    const std::string command =
        "g++ -shared -std=c++17 -O2 " +
        quote(cpp_path) +
        " -I" +
        quote(include_dir) +
        " -o " +
        quote(dll_path);

    std::printf(
        "Compiling C++ script:\n%s\n",
        command.c_str()
    );

    const int result =
        std::system(
            command.c_str()
        );

    if (result != 0) {
        std::fprintf(
            stderr,
            "C++ script compilation failed.\n"
        );
        return false;
    }

    HMODULE library =
        LoadLibraryA(
            dll_path.c_str()
        );

    if (!library) {
        std::fprintf(
            stderr,
            "Could not load script DLL. Win32 error=%lu\n",
            (unsigned long)GetLastError()
        );
        return false;
    }

    ScriptStartFn start =
        reinterpret_cast<ScriptStartFn>(
            GetProcAddress(
                library,
                "ScriptStart"
            )
        );

    ScriptUpdateFn update =
        reinterpret_cast<ScriptUpdateFn>(
            GetProcAddress(
                library,
                "ScriptUpdate"
            )
        );

    if (!start || !update) {
        std::fprintf(
            stderr,
            "Script must export ScriptStart and ScriptUpdate.\n"
        );

        FreeLibrary(library);
        return false;
    }

    module.library = library;
    module.start = start;
    module.update = update;
    module.source_path = cpp_path;
    module.dll_path = dll_path;
    module.total_time = 0.0f;
    module.started = false;

    std::printf(
        "C++ script loaded: %s\n",
        cpp_path.c_str()
    );

    return true;
}

void script_module_update(
    ScriptModule& module,
    ScriptContext& context,
    float dt
)
{
    if (!module.library ||
        !module.start ||
        !module.update) {
        return;
    }

    module.total_time += dt;

    context.delta_time = dt;
    context.total_time =
        module.total_time;

    if (!module.started) {
        module.start(
            &context
        );

        module.started = true;
    }

    module.update(
        &context
    );
}
