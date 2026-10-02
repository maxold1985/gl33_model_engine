#pragma once

#include "game_object.h"

enum class MeshRendererSource {
    Cube,
    LoadedModel
};

class MeshRendererComponent : public Component {
public:
    MeshRendererSource source =
        MeshRendererSource::Cube;

    bool visible = true;
    bool use_texture = true;

    /*
        When true, LoadedModel is normalized using the renderer's
        existing model center/radius before this object's Transform.
    */
    bool auto_fit = true;
};
