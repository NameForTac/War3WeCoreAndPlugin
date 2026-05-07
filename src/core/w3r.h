#pragma once

#include "buffer.h"
#include <string>
#include <vector>

// ============================================================
// war3map.w3r — Regions
// ============================================================
struct Region {
    float left = 0, right = 0, bottom = 0, top = 0;
    std::string name;
    int32_t index = 0;
    std::string weather_id; // 4-char code, e.g. "RLlr", all zeros = disabled
    std::string ambient_sound;
    uint8_t color_b = 0, color_g = 0, color_r = 0;
};

namespace w3r {

std::vector<Region> read(Buffer& buf);
void write(Buffer& buf, const std::vector<Region>& regions);

} // namespace w3r
