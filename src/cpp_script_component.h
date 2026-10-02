#pragma once

#include "game_object.h"
#include "script_module.h"

#include <string>

class CppScriptComponent : public Component {
public:
    ScriptModule module;
    std::string source_path;
    bool auto_load = false;

    ~CppScriptComponent() override;

    bool Load(
        const std::string& cpp_path
    );

    void Unload();

    void Start() override;
    void Update(float dt) override;
    void OnDestroy() override;
};
