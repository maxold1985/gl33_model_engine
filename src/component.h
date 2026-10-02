#pragma once

class GameObject;
struct TransformComponent;

class Component {
public:
    GameObject* game_object = nullptr;
    bool enabled = true;
    bool started = false;

    virtual ~Component() {}

    virtual void Start() {}
    virtual void Update(float dt) { (void)dt; }
    virtual void OnDestroy() {}

    TransformComponent& transform();
    const TransformComponent& transform() const;
};
