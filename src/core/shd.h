#pragma once

#include "buffer.h"
#include <cstdint>
#include <vector>

// ============================================================
// war3map.shd — Shadow map
//
// No header. File size = 16 × map_width × map_height bytes.
// Each byte: 0x00 = no shadow, 0xFF = shadow.
// ============================================================
struct ShadowMap {
    int32_t width = 0;  // map_width × 4
    int32_t height = 0; // map_height × 4
    std::vector<uint8_t> data; // width × height bytes

    bool has_shadow(int x, int y) const {
        if (x < 0 || x >= width || y < 0 || y >= height) return false;
        return data[y * width + x] != 0;
    }
};

namespace shd {

ShadowMap read(Buffer& buf, int32_t width, int32_t height);
void write(Buffer& buf, const ShadowMap& sm);

} // namespace shd
