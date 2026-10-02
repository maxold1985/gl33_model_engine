#include "script_api.h"

#include <cmath>

GL33_SCRIPT_BEGIN()
{
    if (!context)
        return;

    if (context->object_y)
        *context->object_y = 12.0f;
}

GL33_SCRIPT_UPDATE()
{
    if (!context)
        return;

    const float t =
        context->total_time;

    if (context->object_x)
        *context->object_x =
            std::sin(t * 0.7f) * 20.0f;

    if (context->object_z)
        *context->object_z =
            std::cos(t * 0.7f) * 20.0f;

    if (context->object_y)
        *context->object_y =
            12.0f +
            std::sin(t * 1.8f) * 4.0f;

    if (context->object_yaw)
        *context->object_yaw =
            t * 0.7f;
}
