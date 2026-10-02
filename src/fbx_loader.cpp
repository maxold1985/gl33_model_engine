#define WIN32_LEAN_AND_MEAN
#include "fbx_loader.h"

#include <windows.h>
#include <objbase.h>
#include <wincodec.h>

#include <assimp/Importer.hpp>
#include <assimp/config.h>
#include <assimp/material.h>
#include <assimp/postprocess.h>
#include <assimp/scene.h>
#include <assimp/version.h>

#include <algorithm>
#include <cstdint>
#include <cstring>
#include <fstream>
#include <string>
#include <vector>
#include <utility>

static std::string directory_of(
    const std::string& path
)
{
    const std::size_t slash =
        path.find_last_of("\\/");

    if (slash == std::string::npos)
        return ".";

    return path.substr(0, slash);
}

static bool file_exists(
    const std::string& path
)
{
    std::ifstream f(
        path,
        std::ios::binary
    );

    return !!f;
}

static std::string normalize_slashes(
    std::string path
)
{
    for (char& c : path) {
        if (c == '/')
            c = '\\';
    }

    return path;
}

static std::string basename_of(
    const std::string& path
)
{
    const std::size_t slash =
        path.find_last_of("\\/");

    if (slash == std::string::npos)
        return path;

    return path.substr(slash + 1);
}

static std::string resolve_texture_path(
    const std::string& fbx_path,
    const std::string& texture_path
)
{
    std::string tex =
        normalize_slashes(
            texture_path
        );

    if (tex.empty())
        return {};

    /*
        Absolute Windows path.
    */
    if (tex.size() >= 2 &&
        tex[1] == ':') {

        if (file_exists(tex))
            return tex;
    }

    const std::string directory =
        directory_of(fbx_path);

    std::string candidate =
        directory + "\\" + tex;

    if (file_exists(candidate))
        return candidate;

    /*
        FBX files often store an absolute source path
        from the machine that exported them.
        Fall back to the filename next to the FBX.
    */
    candidate =
        directory + "\\" +
        basename_of(tex);

    if (file_exists(candidate))
        return candidate;

    return {};
}

static bool read_file(
    const std::string& path,
    std::vector<std::uint8_t>& out
)
{
    std::ifstream f(
        path,
        std::ios::binary
    );

    if (!f)
        return false;

    f.seekg(
        0,
        std::ios::end
    );

    const std::streamoff size =
        f.tellg();

    f.seekg(
        0,
        std::ios::beg
    );

    if (size <= 0)
        return false;

    out.resize(
        (std::size_t)size
    );

    f.read(
        (char*)out.data(),
        size
    );

    return !!f;
}

static bool decode_wic_rgba(
    const std::uint8_t* bytes,
    std::size_t byte_count,
    GlbImage& out,
    std::string& error
)
{
    if (!bytes ||
        byte_count == 0 ||
        byte_count > 0xFFFFFFFFu) {

        error =
            "invalid image data";

        return false;
    }

    IWICImagingFactory* factory =
        nullptr;

    IWICStream* stream =
        nullptr;

    IWICBitmapDecoder* decoder =
        nullptr;

    IWICBitmapFrameDecode* frame =
        nullptr;

    IWICFormatConverter* converter =
        nullptr;

    HRESULT hr =
        CoCreateInstance(
            CLSID_WICImagingFactory,
            nullptr,
            CLSCTX_INPROC_SERVER,
            IID_IWICImagingFactory,
            (void**)&factory
        );

    if (FAILED(hr)) {
        error =
            "WIC factory creation failed";

        return false;
    }

    hr =
        factory->CreateStream(
            &stream
        );

    if (SUCCEEDED(hr)) {
        hr =
            stream->InitializeFromMemory(
                (BYTE*)bytes,
                (DWORD)byte_count
            );
    }

    if (SUCCEEDED(hr)) {
        hr =
            factory->CreateDecoderFromStream(
                stream,
                nullptr,
                WICDecodeMetadataCacheOnLoad,
                &decoder
            );
    }

    if (SUCCEEDED(hr)) {
        hr =
            decoder->GetFrame(
                0,
                &frame
            );
    }

    UINT width = 0;
    UINT height = 0;

    if (SUCCEEDED(hr)) {
        hr =
            frame->GetSize(
                &width,
                &height
            );
    }

    if (SUCCEEDED(hr)) {
        hr =
            factory->CreateFormatConverter(
                &converter
            );
    }

    if (SUCCEEDED(hr)) {
        hr =
            converter->Initialize(
                frame,
                GUID_WICPixelFormat32bppRGBA,
                WICBitmapDitherTypeNone,
                nullptr,
                0.0,
                WICBitmapPaletteTypeCustom
            );
    }

    if (SUCCEEDED(hr)) {
        out.width =
            (int)width;

        out.height =
            (int)height;

        out.rgba.resize(
            (std::size_t)width *
            (std::size_t)height *
            4
        );

        hr =
            converter->CopyPixels(
                nullptr,
                width * 4,
                (UINT)out.rgba.size(),
                out.rgba.data()
            );
    }

    if (converter)
        converter->Release();

    if (frame)
        frame->Release();

    if (decoder)
        decoder->Release();

    if (stream)
        stream->Release();

    if (factory)
        factory->Release();

    if (FAILED(hr)) {
        out = GlbImage{};

        error =
            "WIC could not decode texture";

        return false;
    }

    return true;
}

static bool load_assimp_embedded_texture(
    const aiTexture* texture,
    GlbImage& out,
    std::string& error
)
{
    if (!texture) {
        error =
            "null embedded texture";

        return false;
    }

    /*
        mHeight == 0:
        compressed PNG/JPEG/etc.
        mWidth is byte count.
    */
    if (texture->mHeight == 0) {
        return decode_wic_rgba(
            (const std::uint8_t*)
                texture->pcData,
            (std::size_t)
                texture->mWidth,
            out,
            error
        );
    }

    /*
        Uncompressed Assimp texture.
        aiTexel is BGRA.
    */
    out.width =
        (int)texture->mWidth;

    out.height =
        (int)texture->mHeight;

    out.rgba.resize(
        (std::size_t)out.width *
        (std::size_t)out.height *
        4
    );

    for (unsigned y = 0;
         y < texture->mHeight;
         ++y) {

        for (unsigned x = 0;
             x < texture->mWidth;
             ++x) {

            const aiTexel& texel =
                texture->pcData[
                    (std::size_t)y *
                    texture->mWidth +
                    x
                ];

            const std::size_t dst =
                (
                    (std::size_t)y *
                    texture->mWidth +
                    x
                ) * 4;

            out.rgba[dst + 0] =
                texel.r;

            out.rgba[dst + 1] =
                texel.g;

            out.rgba[dst + 2] =
                texel.b;

            out.rgba[dst + 3] =
                texel.a;
        }
    }

    return true;
}

static bool load_material_texture(
    const aiScene* scene,
    const aiMaterial* material,
    const std::string& fbx_path,
    GlbImage& out_image
)
{
    if (!scene ||
        !material) {
        return false;
    }

    aiString texture_path;
    aiTextureType texture_type =
        aiTextureType_DIFFUSE;

#if ASSIMP_VERSION_MAJOR >= 5
    if (material->GetTextureCount(
            aiTextureType_BASE_COLOR) > 0) {
        texture_type =
            aiTextureType_BASE_COLOR;
    } else
#endif
    if (material->GetTextureCount(
            aiTextureType_DIFFUSE) == 0) {
        return false;
    }

    if (material->GetTexture(
            texture_type,
            0,
            &texture_path) != AI_SUCCESS) {
        return false;
    }

    const char* raw =
        texture_path.C_Str();

    if (!raw ||
        !raw[0]) {
        return false;
    }

    /*
        Assimp understands "*0" references and,
        on modern versions, embedded filenames.
    */
    if (const aiTexture* embedded =
            scene->GetEmbeddedTexture(raw)) {

        std::string error;

        return
            load_assimp_embedded_texture(
                embedded,
                out_image,
                error
            );
    }

    const std::string resolved =
        resolve_texture_path(
            fbx_path,
            raw
        );

    if (resolved.empty())
        return false;

    std::vector<std::uint8_t> bytes;

    if (!read_file(
            resolved,
            bytes)) {
        return false;
    }

    std::string error;

    return
        decode_wic_rgba(
            bytes.data(),
            bytes.size(),
            out_image,
            error
        );
}

static GlbMaterial convert_material(
    const aiScene* scene,
    const aiMaterial* material,
    const std::string& fbx_path
)
{
    GlbMaterial out;

    if (!material)
        return out;

    aiColor4D color(
        1.0f,
        1.0f,
        1.0f,
        1.0f
    );

#ifdef AI_MATKEY_BASE_COLOR
    if (material->Get(
            AI_MATKEY_BASE_COLOR,
            color) != AI_SUCCESS) {
#endif
        aiColor3D diffuse(
            1.0f,
            1.0f,
            1.0f
        );

        if (material->Get(
                AI_MATKEY_COLOR_DIFFUSE,
                diffuse) == AI_SUCCESS) {

            color.r = diffuse.r;
            color.g = diffuse.g;
            color.b = diffuse.b;
        }

        float opacity = 1.0f;

        if (material->Get(
                AI_MATKEY_OPACITY,
                opacity) == AI_SUCCESS) {
            color.a = opacity;
        }
#ifdef AI_MATKEY_BASE_COLOR
    }
#endif

    out.base_color_factor[0] =
        color.r;

    out.base_color_factor[1] =
        color.g;

    out.base_color_factor[2] =
        color.b;

    out.base_color_factor[3] =
        color.a;

#ifdef AI_MATKEY_METALLIC_FACTOR
    material->Get(
        AI_MATKEY_METALLIC_FACTOR,
        out.metallic_factor
    );
#else
    out.metallic_factor = 0.0f;
#endif

#ifdef AI_MATKEY_ROUGHNESS_FACTOR
    material->Get(
        AI_MATKEY_ROUGHNESS_FACTOR,
        out.roughness_factor
    );
#else
    /*
        FBX/Phong shininess -> roughness approximation.
    */
    float shininess = 0.0f;

    if (material->Get(
            AI_MATKEY_SHININESS,
            shininess) == AI_SUCCESS) {

        out.roughness_factor =
            std::max(
                0.05f,
                std::min(
                    1.0f,
                    1.0f -
                    shininess / 256.0f
                )
            );
    }
#endif

    load_material_texture(
        scene,
        material,
        fbx_path,
        out.base_color_image
    );

    return out;
}


static void copy_ai_matrix(
    const aiMatrix4x4& m,
    float out[16]
)
{
    out[0]  = m.a1; out[1]  = m.a2; out[2]  = m.a3; out[3]  = m.a4;
    out[4]  = m.b1; out[5]  = m.b2; out[6]  = m.b3; out[7]  = m.b4;
    out[8]  = m.c1; out[9]  = m.c2; out[10] = m.c3; out[11] = m.c4;
    out[12] = m.d1; out[13] = m.d2; out[14] = m.d3; out[15] = m.d4;
}

static void read_bones(
    const aiMesh* mesh,
    GlbPrimitive& primitive
)
{
    if (!mesh || !mesh->HasBones())
        return;

    primitive.bones.reserve(mesh->mNumBones);

    for (unsigned bone_index = 0;
         bone_index < mesh->mNumBones;
         ++bone_index) {

        const aiBone* bone =
            mesh->mBones[bone_index];

        if (!bone)
            continue;

        ModelBone dst;
        dst.name = bone->mName.C_Str();

        copy_ai_matrix(
            bone->mOffsetMatrix,
            dst.offset_matrix
        );

        dst.weights.reserve(
            bone->mNumWeights
        );

        for (unsigned weight_index = 0;
             weight_index < bone->mNumWeights;
             ++weight_index) {

            const aiVertexWeight& weight =
                bone->mWeights[weight_index];

            if (weight.mVertexId >= mesh->mNumVertices)
                continue;

            ModelBoneWeight out_weight;
            out_weight.vertex_index =
                (std::uint32_t)weight.mVertexId;
            out_weight.weight = weight.mWeight;

            dst.weights.push_back(out_weight);
        }

        primitive.bones.push_back(
            std::move(dst)
        );
    }
}

static GlbPrimitive convert_mesh_instance(
    const aiMesh* mesh,
    const aiMatrix4x4& global_transform
)
{
    GlbPrimitive primitive;

    if (!mesh || !mesh->HasPositions())
        return primitive;

    primitive.material_index =
        (int)mesh->mMaterialIndex;

    primitive.vertices.resize(
        mesh->mNumVertices
    );

    primitive.bind_vertices.resize(
        mesh->mNumVertices
    );

    aiMatrix3x3 normal_transform(
        global_transform
    );

    normal_transform.Inverse();
    normal_transform.Transpose();

    for (unsigned i = 0;
         i < mesh->mNumVertices;
         ++i) {

        GlbVertex& dst =
            primitive.vertices[i];

        GlbVertex& bind =
            primitive.bind_vertices[i];

        const aiVector3D& raw_position =
            mesh->mVertices[i];

        bind.px = raw_position.x;
        bind.py = raw_position.y;
        bind.pz = raw_position.z;

        const aiVector3D p =
            global_transform *
            raw_position;

        dst.px = p.x;
        dst.py = p.y;
        dst.pz = p.z;

        if (mesh->HasNormals()) {
            const aiVector3D& raw_normal =
                mesh->mNormals[i];

            bind.nx = raw_normal.x;
            bind.ny = raw_normal.y;
            bind.nz = raw_normal.z;

            aiVector3D n =
                normal_transform *
                raw_normal;

            n.Normalize();

            dst.nx = n.x;
            dst.ny = n.y;
            dst.nz = n.z;
        } else {
            bind.nx = 0.0f;
            bind.ny = 1.0f;
            bind.nz = 0.0f;

            dst.nx = 0.0f;
            dst.ny = 1.0f;
            dst.nz = 0.0f;
        }

        if (mesh->HasTextureCoords(0)) {
            const aiVector3D& uv =
                mesh->mTextureCoords[0][i];

            bind.u = uv.x;
            bind.v = uv.y;

            dst.u = uv.x;
            dst.v = uv.y;
        } else {
            bind.u = 0.0f;
            bind.v = 0.0f;

            dst.u = 0.0f;
            dst.v = 0.0f;
        }
    }

    primitive.indices.reserve(
        (std::size_t)mesh->mNumFaces * 3
    );

    for (unsigned face_index = 0;
         face_index < mesh->mNumFaces;
         ++face_index) {

        const aiFace& face =
            mesh->mFaces[face_index];

        if (face.mNumIndices != 3)
            continue;

        primitive.indices.push_back(
            face.mIndices[0]
        );
        primitive.indices.push_back(
            face.mIndices[1]
        );
        primitive.indices.push_back(
            face.mIndices[2]
        );
    }

    read_bones(mesh, primitive);

    return primitive;
}

static void read_node_recursive(
    const aiScene* scene,
    const aiNode* node,
    const aiMatrix4x4& parent_global,
    const aiMatrix4x4& global_inverse_root,
    int parent_index,
    GlbModel& out_model
)
{
    if (!scene || !node)
        return;

    const aiMatrix4x4 global =
        parent_global *
        node->mTransformation;

    ModelNode out_node;
    out_node.name = node->mName.C_Str();
    out_node.parent_index = parent_index;

    copy_ai_matrix(
        node->mTransformation,
        out_node.local_transform
    );

    const int this_node_index =
        (int)out_model.nodes.size();

    out_model.nodes.push_back(
        std::move(out_node)
    );

    for (unsigned i = 0;
         i < node->mNumMeshes;
         ++i) {

        const unsigned mesh_index =
            node->mMeshes[i];

        if (mesh_index >= scene->mNumMeshes)
            continue;

        /*
            Keep static pose in the SAME model space used by animated
            bones. FBX root nodes often contain axis/unit conversion.
        */
        const aiMatrix4x4 model_global =
            global_inverse_root *
            global;

        GlbPrimitive primitive =
            convert_mesh_instance(
                scene->mMeshes[mesh_index],
                model_global
            );

        if (primitive.vertices.empty() ||
            primitive.indices.empty()) {
            continue;
        }

        primitive.node_index =
            this_node_index;

        const int primitive_index =
            (int)out_model.primitives.size();

        out_model.primitives.push_back(
            std::move(primitive)
        );

        out_model.nodes[
            (std::size_t)this_node_index
        ].primitive_indices.push_back(
            primitive_index
        );
    }

    for (unsigned child = 0;
         child < node->mNumChildren;
         ++child) {

        read_node_recursive(
            scene,
            node->mChildren[child],
            global,
            global_inverse_root,
            this_node_index,
            out_model
        );
    }
}

static void read_animations(
    const aiScene* scene,
    GlbModel& out_model
)
{
    if (!scene || !scene->HasAnimations())
        return;

    out_model.animations.reserve(
        scene->mNumAnimations
    );

    for (unsigned animation_index = 0;
         animation_index < scene->mNumAnimations;
         ++animation_index) {

        const aiAnimation* animation =
            scene->mAnimations[
                animation_index
            ];

        if (!animation)
            continue;

        ModelAnimationClip clip;

        if (animation->mName.length > 0) {
            clip.name =
                animation->mName.C_Str();
        } else {
            clip.name =
                "Animation_" +
                std::to_string(
                    animation_index
                );
        }

        clip.duration_ticks =
            animation->mDuration;

        clip.ticks_per_second =
            animation->mTicksPerSecond > 0.0
                ? animation->mTicksPerSecond
                : 25.0;

        clip.duration_seconds =
            clip.ticks_per_second > 0.0
                ? clip.duration_ticks /
                  clip.ticks_per_second
                : 0.0;

        clip.channels.reserve(
            animation->mNumChannels
        );

        for (unsigned channel_index = 0;
             channel_index < animation->mNumChannels;
             ++channel_index) {

            const aiNodeAnim* channel =
                animation->mChannels[
                    channel_index
                ];

            if (!channel)
                continue;

            ModelAnimationChannel dst;
            dst.node_name =
                channel->mNodeName.C_Str();

            dst.position_keys.reserve(
                channel->mNumPositionKeys
            );

            for (unsigned key_index = 0;
                 key_index < channel->mNumPositionKeys;
                 ++key_index) {

                const aiVectorKey& key =
                    channel->mPositionKeys[
                        key_index
                    ];

                ModelVec3Key out_key;
                out_key.time_ticks = key.mTime;
                out_key.x = key.mValue.x;
                out_key.y = key.mValue.y;
                out_key.z = key.mValue.z;

                dst.position_keys.push_back(
                    out_key
                );
            }

            dst.rotation_keys.reserve(
                channel->mNumRotationKeys
            );

            for (unsigned key_index = 0;
                 key_index < channel->mNumRotationKeys;
                 ++key_index) {

                const aiQuatKey& key =
                    channel->mRotationKeys[
                        key_index
                    ];

                ModelQuatKey out_key;
                out_key.time_ticks = key.mTime;
                out_key.x = key.mValue.x;
                out_key.y = key.mValue.y;
                out_key.z = key.mValue.z;
                out_key.w = key.mValue.w;

                dst.rotation_keys.push_back(
                    out_key
                );
            }

            dst.scaling_keys.reserve(
                channel->mNumScalingKeys
            );

            for (unsigned key_index = 0;
                 key_index < channel->mNumScalingKeys;
                 ++key_index) {

                const aiVectorKey& key =
                    channel->mScalingKeys[
                        key_index
                    ];

                ModelVec3Key out_key;
                out_key.time_ticks = key.mTime;
                out_key.x = key.mValue.x;
                out_key.y = key.mValue.y;
                out_key.z = key.mValue.z;

                dst.scaling_keys.push_back(
                    out_key
                );
            }

            clip.channels.push_back(
                std::move(dst)
            );
        }

        std::printf(
            "FBX animation[%u]: %s duration=%.3fs ticks=%.3f tps=%.3f channels=%u\n",
            animation_index,
            clip.name.c_str(),
            clip.duration_seconds,
            clip.duration_ticks,
            clip.ticks_per_second,
            (unsigned)clip.channels.size()
        );

        out_model.animations.push_back(
            std::move(clip)
        );
    }
}

bool fbx_load(
    const std::string& path,
    GlbModel& out_model,
    std::string& out_error
)
{
    out_model = GlbModel{};
    out_error.clear();

    Assimp::Importer importer;

    /*
        MIXAMO / FBX PIVOT FIX

        Assimp normally preserves FBX pivots by creating helper nodes
        such as:

            _$AssimpFbx$_PreRotation
            _$AssimpFbx$_PostRotation
            _$AssimpFbx$_RotationPivot

        For Mixamo skeletal animation this can complicate a custom
        animation player and cause incorrect bone orientation.

        Setting this property to false makes Assimp evaluate/fold
        those pivot transforms into the node hierarchy.

        IMPORTANT:
        This MUST be set before importer.ReadFile().
    */
    importer.SetPropertyBool(
        AI_CONFIG_IMPORT_FBX_PRESERVE_PIVOTS,
        false
    );

    std::printf(
        "FBX importer: PRESERVE_PIVOTS=FALSE (Mixamo mode)\n"
    );

    if (!importer.IsExtensionSupported(".fbx")) {
        out_error =
            "Assimp was built without FBX importer support";
        return false;
    }

    /*
        IMPORTANT:
        aiProcess_PreTransformVertices is intentionally NOT used.
        It destroys the hierarchy needed by animation channels.
    */
    const unsigned flags =
        aiProcess_Triangulate |
        aiProcess_JoinIdenticalVertices |
        aiProcess_GenSmoothNormals |
        aiProcess_ImproveCacheLocality |
        aiProcess_SortByPType |
        aiProcess_FlipUVs |
        aiProcess_LimitBoneWeights;

    const aiScene* scene =
        importer.ReadFile(
            path,
            flags
        );

    if (!scene) {
        const std::string first_error =
            importer.GetErrorString();

        importer.FreeScene();

        const unsigned fallback_flags =
            aiProcess_Triangulate |
            aiProcess_GenSmoothNormals |
            aiProcess_FlipUVs |
            aiProcess_SortByPType;

        scene =
            importer.ReadFile(
                path,
                fallback_flags
            );

        if (!scene) {
            out_error =
                std::string(
                    "Assimp FBX error: "
                ) +
                importer.GetErrorString();

            if (!first_error.empty()) {
                out_error +=
                    " | first attempt: " +
                    first_error;
            }

            return false;
        }
    }

    std::printf(
        "FBX Assimp: meshes=%u materials=%u textures=%u animations=%u\n",
        scene->mNumMeshes,
        scene->mNumMaterials,
        scene->mNumTextures,
        scene->mNumAnimations
    );

    if (!scene->HasMeshes()) {
        out_error =
            "FBX contains no meshes";
        return false;
    }

    out_model.materials.reserve(
        scene->mNumMaterials
    );

    for (unsigned i = 0;
         i < scene->mNumMaterials;
         ++i) {

        out_model.materials.push_back(
            convert_material(
                scene,
                scene->mMaterials[i],
                path
            )
        );
    }

    aiMatrix4x4 identity;

    /*
        Assimp's standard skeletal-animation model-space conversion.
        FBX often places coordinate-system conversion on the root node.
    */
    aiMatrix4x4 global_inverse_root =
        scene->mRootNode->mTransformation;

    global_inverse_root.Inverse();

    copy_ai_matrix(
        global_inverse_root,
        out_model.global_inverse_transform
    );

    read_node_recursive(
        scene,
        scene->mRootNode,
        identity,
        global_inverse_root,
        -1,
        out_model
    );

    read_animations(
        scene,
        out_model
    );

    if (out_model.primitives.empty()) {
        out_error =
            "FBX has no drawable triangle meshes";
        return false;
    }

    std::size_t bone_count = 0;
    std::size_t weight_count = 0;

    for (const GlbPrimitive& primitive :
         out_model.primitives) {

        bone_count +=
            primitive.bones.size();

        for (const ModelBone& bone :
             primitive.bones) {
            weight_count +=
                bone.weights.size();
        }
    }

    std::printf(
        "FBX hierarchy: nodes=%u bones=%u weights=%u animation_clips=%u\n",
        (unsigned)out_model.nodes.size(),
        (unsigned)bone_count,
        (unsigned)weight_count,
        (unsigned)out_model.animations.size()
    );

    out_model.source_path = path;
    return true;
}
