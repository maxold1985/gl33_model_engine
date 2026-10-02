#pragma once

#include <cstdint>
#include <string>
#include <vector>

struct ModelVec3Key {
    double time_ticks = 0.0;
    float x = 0.0f;
    float y = 0.0f;
    float z = 0.0f;
};

struct ModelQuatKey {
    double time_ticks = 0.0;

    /*
        Quaternion order:
        x, y, z, w
    */
    float x = 0.0f;
    float y = 0.0f;
    float z = 0.0f;
    float w = 1.0f;
};

struct ModelAnimationChannel {
    std::string node_name;

    std::vector<ModelVec3Key> position_keys;
    std::vector<ModelQuatKey> rotation_keys;
    std::vector<ModelVec3Key> scaling_keys;
};

struct ModelAnimationClip {
    std::string name;

    double duration_ticks = 0.0;
    double ticks_per_second = 25.0;
    double duration_seconds = 0.0;

    std::vector<ModelAnimationChannel> channels;
};

struct ModelBoneWeight {
    std::uint32_t vertex_index = 0;
    float weight = 0.0f;
};

struct ModelBone {
    std::string name;

    /*
        Assimp offset matrix, row-major:
        a1 a2 a3 a4
        b1 b2 b3 b4
        c1 c2 c3 c4
        d1 d2 d3 d4
    */
    float offset_matrix[16] = {
        1,0,0,0,
        0,1,0,0,
        0,0,1,0,
        0,0,0,1
    };

    std::vector<ModelBoneWeight> weights;
};

struct ModelNode {
    std::string name;
    int parent_index = -1;

    /*
        Local node transform from the FBX hierarchy.
        Stored row-major using Assimp's matrix layout.
    */
    float local_transform[16] = {
        1,0,0,0,
        0,1,0,0,
        0,0,1,0,
        0,0,0,1
    };

    /*
        Indices into ModelData/GlbModel::primitives.
        One mesh can appear on more than one node, so each node instance
        becomes a primitive entry in the loaded model.
    */
    std::vector<int> primitive_indices;
};
