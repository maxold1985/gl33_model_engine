#pragma once

#include "glb_loader.h"

#include <string>

bool model_load(
    const std::string& path,
    GlbModel& out_model,
    std::string& out_error
);
