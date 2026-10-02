#pragma once

#include "game_object.h"
#include "physics.h"

#include <memory>
#include <string>
#include <vector>

class Scene {
public:
    PhysicsWorld physics;

    GameObject* CreateGameObject(
        const std::string& name = "GameObject"
    );

    bool DestroyGameObject(
        GameObject* object
    );

    GameObject* Find(
        const std::string& name
    );

    const GameObject* Find(
        const std::string& name
    ) const;

    void Update(float dt);
    void Clear();

    const std::vector<std::unique_ptr<GameObject>>&
    objects() const
    {
        return objects_;
    }

private:
    std::vector<std::unique_ptr<GameObject>> objects_;
};
