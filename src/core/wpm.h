#pragma once

#include "buffer.h"
#include <cstdint>
#include <vector>

// ============================================================
// war3map.wpm — Pathing map
// ============================================================
struct PathingMap {
    uint32_t version = 0;
    int32_t width = 0;   // map_width × 4
    int32_t height = 0;  // map_height × 4
    std::vector<uint8_t> data; // width × height bytes

    // Per-byte flags
    static constexpr uint8_t FLAG_UNWALKABLE  = 0x02;
    static constexpr uint8_t FLAG_UNFLYABLE   = 0x04;
    static constexpr uint8_t FLAG_UNBUILDABLE = 0x08;
    static constexpr uint8_t FLAG_BLIGHT      = 0x20;
    static constexpr uint8_t FLAG_NO_WATER    = 0x40;
    static constexpr uint8_t FLAG_UNKNOWN     = 0x80;

    bool is_walkable(int x, int y) const {
        if (x < 0 || x >= width || y < 0 || y >= height) return false;
        return !(data[y * width + x] & FLAG_UNWALKABLE);
    }
    bool is_flyable(int x, int y) const {
        if (x < 0 || x >= width || y < 0 || y >= height) return false;
        return !(data[y * width + x] & FLAG_UNFLYABLE);
    }
    bool is_buildable(int x, int y) const {
        if (x < 0 || x >= width || y < 0 || y >= height) return false;
        return !(data[y * width + x] & FLAG_UNBUILDABLE);
    }
};

namespace wpm {

PathingMap read(Buffer& buf);
void write(Buffer& buf, const PathingMap& pm);

} // namespace wpm
