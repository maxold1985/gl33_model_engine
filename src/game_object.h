#pragma once

#include "transform.h"

#include <algorithm>
#include <memory>
#include <string>
#include <type_traits>
#include <typeinfo>
#include <utility>
#include <vector>

class GameObject;

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

class GameObject {
public:
    std::string name = "GameObject";
    bool active = true;

    /* Unity behavior: every GameObject always owns a Transform. */
    TransformComponent transform;

    explicit GameObject(
        const std::string& object_name = "GameObject"
    )
        : name(object_name)
    {
    }

    ~GameObject()
    {
        clear_components();
    }

    template<typename T, typename... Args>
    T* AddComponent(Args&&... args)
    {
        static_assert(
            std::is_base_of<Component, T>::value,
            "T must derive from Component"
        );

        std::unique_ptr<T> component(
            new T(
                std::forward<Args>(args)...
            )
        );

        component->game_object = this;

        T* result = component.get();

        components_.push_back(
            std::move(component)
        );

        return result;
    }

    template<typename T>
    T* GetComponent()
    {
        static_assert(
            std::is_base_of<Component, T>::value,
            "T must derive from Component"
        );

        for (const auto& component : components_) {
            if (T* result =
                    dynamic_cast<T*>(component.get())) {
                return result;
            }
        }

        return nullptr;
    }

    template<typename T>
    const T* GetComponent() const
    {
        static_assert(
            std::is_base_of<Component, T>::value,
            "T must derive from Component"
        );

        for (const auto& component : components_) {
            if (const T* result =
                    dynamic_cast<const T*>(component.get())) {
                return result;
            }
        }

        return nullptr;
    }

    template<typename T>
    bool RemoveComponent()
    {
        static_assert(
            std::is_base_of<Component, T>::value,
            "T must derive from Component"
        );

        for (auto it = components_.begin();
             it != components_.end();
             ++it) {

            if (dynamic_cast<T*>(it->get())) {
                (*it)->OnDestroy();
                components_.erase(it);
                return true;
            }
        }

        return false;
    }

    void update_components(float dt);
    void clear_components();

    std::size_t component_count() const
    {
        return components_.size();
    }

private:
    std::vector<std::unique_ptr<Component>> components_;
};
