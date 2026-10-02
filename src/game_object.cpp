#include "game_object.h"

TransformComponent& Component::transform()
{
    return game_object->transform;
}

const TransformComponent& Component::transform() const
{
    return game_object->transform;
}

void GameObject::update_components(
    float dt
)
{
    if (!active)
        return;

    for (const auto& component : components_) {
        if (!component || !component->enabled)
            continue;

        if (!component->started) {
            component->Start();
            component->started = true;
        }

        component->Update(dt);
    }
}

void GameObject::clear_components()
{
    for (const auto& component : components_) {
        if (component)
            component->OnDestroy();
    }

    components_.clear();
}
