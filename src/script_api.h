#pragma once

#ifdef _WIN32
#define GL33_SCRIPT_EXPORT extern "C" __declspec(dllexport)
#else
#define GL33_SCRIPT_EXPORT extern "C"
#endif

struct ScriptContext {
    float delta_time;
    float total_time;

    float* object_x;
    float* object_y;
    float* object_z;
    float* object_yaw;
};

using ScriptStartFn = void (*)(ScriptContext*);
using ScriptUpdateFn = void (*)(ScriptContext*);

#define GL33_SCRIPT_BEGIN() \
    GL33_SCRIPT_EXPORT void ScriptStart(ScriptContext* context)

#define GL33_SCRIPT_UPDATE() \
    GL33_SCRIPT_EXPORT void ScriptUpdate(ScriptContext* context)
