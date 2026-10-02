#pragma once

#include "animation_data.h"

#include <cstdint>
#include <string>
#include <vector>

struct GlbVertex {
    float px, py, pz;
    float nx, ny, nz;
    float u, v;
};

struct GlbImage {
    int width = 0;
    int height = 0;
    std::vector<std::uint8_t> rgba;
};

struct GlbMaterial {
    GlbImage base_color_image;

    float base_color_factor[4] = {
        1.0f, 1.0f, 1.0f, 1.0f
    };

    float metallic_factor = 1.0f;
    float roughness_factor = 1.0f;
};

struct GlbPrimitive {
    /*
        vertices:
        display/baked vertices used for the initial static pose.

        bind_vertices:
        original FBX mesh-local bind-pose vertices. These are required
        for node animation and skeletal skinning.
    */
    std::vector<GlbVertex> vertices;
    std::vector<GlbVertex> bind_vertices;

    std::vector<std::uint32_t> indices;

    int material_index = -1;

    /*
        FBX node that owns this primitive.
        -1 for GLB/current non-FBX loaders.
    */
    int node_index = -1;

    /*
        Filled by the FBX loader when the mesh has a skeleton.
    */
    std::vector<ModelBone> bones;
};

struct GlbModel {
    std::vector<GlbPrimitive> primitives;
    std::vector<GlbMaterial> materials;

    /*
        FBX hierarchy + animations. These vectors are empty for formats
        whose loader does not implement animation yet.
    */
    std::vector<ModelNode> nodes;
    std::vector<ModelAnimationClip> animations;

    /*
        Assimp model-space conversion:
            inverse(scene->mRootNode->mTransformation)

        Required by skeletal animation:
            finalBone =
                global_inverse_transform *
                globalBone *
                boneOffset
    */
    float global_inverse_transform[16] = {
        1,0,0,0,
        0,1,0,0,
        0,0,1,0,
        0,0,0,1
    };

    std::string source_path;
};

bool glb_load(
    const std::string& path,
    GlbModel& out_model,
    std::string& out_error
);
