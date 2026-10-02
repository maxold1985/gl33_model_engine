#pragma once

#include <string>
#include <vector>

struct UiVertex {
    float x, y;
    float r, g, b;
};

struct UiRenderer {
    unsigned int vao = 0;
    unsigned int vbo = 0;
    unsigned int program = 0;
    std::vector<UiVertex> vertices;
};

bool ui_init(UiRenderer& ui);
void ui_shutdown(UiRenderer& ui);

void ui_begin(UiRenderer& ui);
void ui_rect(
    UiRenderer& ui,
    float x,
    float y,
    float w,
    float h,
    float r,
    float g,
    float b
);
void ui_text(
    UiRenderer& ui,
    const std::string& text,
    float x,
    float y,
    float scale,
    float r,
    float g,
    float b
);
void ui_draw(
    UiRenderer& ui,
    int width,
    int height
);
