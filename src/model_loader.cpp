#include "model_loader.h"

#include "fbx_loader.h"
#include "glb_loader.h"

#include <algorithm>
#include <cctype>
#include <string>

static std::string extension_of(
    const std::string& path
)
{
    const std::size_t dot =
        path.find_last_of('.');

    if (dot == std::string::npos)
        return {};

    std::string ext =
        path.substr(dot);

    std::transform(
        ext.begin(),
        ext.end(),
        ext.begin(),
        [](unsigned char c) {
            return (char)
                std::tolower(c);
        }
    );

    return ext;
}

bool model_load(
    const std::string& path,
    GlbModel& out_model,
    std::string& out_error
)
{
    const std::string ext =
        extension_of(path);

    if (ext == ".glb") {
        return glb_load(
            path,
            out_model,
            out_error
        );
    }

    if (ext == ".fbx") {
        return fbx_load(
            path,
            out_model,
            out_error
        );
    }

    out_error =
        "unsupported model format: " +
        ext;

    return false;
}
