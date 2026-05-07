#pragma once

#include "buffer.h"
#include <string>
#include <vector>

// ============================================================
// war3map.w3e — Terrain / environment file
// ============================================================
struct TilePoint {
    float height = 0;           // ground height
    float water_height = 0;     // water surface height
    bool  map_edge = false;     // map edge flag (bit 15 of water word)
    uint8_t flags = 0;          // bit0=ramp, bit1=blight, bit2=water, bit3=boundary
    uint8_t ground_texture = 0; // ground texture index (4 bits)
    uint8_t texture_detail = 0;
    uint8_t cliff_texture = 0;  // cliff texture index (4 bits)
    uint8_t layer_height = 0;   // layer height (4 bits)
};

struct Terrain {
    uint32_t version = 11;
    char tileset = 'A';
    bool custom_tileset = false;
    std::vector<std::string> ground_tiles; // 4-char tile IDs
    std::vector<std::string> cliff_tiles;  // 4-char tile IDs
    int32_t tile_width = 0;    // cols = map_width + 1
    int32_t tile_height = 0;   // rows = map_height + 1
    float center_offset_x = 0;
    float center_offset_y = 0;
    std::vector<TilePoint> tilepoints; // tile_width × tile_height, row-major
};

// Height: raw int16 stored in file, 0x2000 = ground level 0

namespace w3e {

Terrain read(Buffer& buf);
void write(Buffer& buf, const Terrain& terrain);

} // namespace w3e
