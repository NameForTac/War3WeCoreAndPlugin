#include "w3e.h"
#include <cstring>
#include <stdexcept>

namespace w3e {

// Convert 4-byte char code to std::string
static std::string fourcc_to_string(uint32_t code) {
    char buf[5];
    buf[0] = (char)(code & 0xFF);
    buf[1] = (char)((code >> 8) & 0xFF);
    buf[2] = (char)((code >> 16) & 0xFF);
    buf[3] = (char)((code >> 24) & 0xFF);
    buf[4] = '\0';
    return std::string(buf);
}

static uint32_t string_to_fourcc(const std::string& s) {
    uint32_t code = 0;
    for (size_t i = 0; i < 4 && i < s.size(); ++i) {
        code |= (uint8_t)s[i] << (i * 8);
    }
    return code;
}

Terrain read(Buffer& buf) {
    // Magic "W3E!"
    uint32_t magic = buf.read_u32();
    if (magic != 0x21453357) // "W3E!" little-endian
        throw std::runtime_error("w3e: bad magic");

    Terrain t;
    t.version = buf.read_u32();
    t.tileset = (char)buf.read_u8();
    t.custom_tileset = (buf.read_u32() != 0);

    // Ground tiles
    uint32_t num_ground = buf.read_u32();
    t.ground_tiles.reserve(num_ground);
    for (uint32_t i = 0; i < num_ground; ++i) {
        t.ground_tiles.push_back(fourcc_to_string(buf.read_u32()));
    }

    // Cliff tiles
    uint32_t num_cliff = buf.read_u32();
    t.cliff_tiles.reserve(num_cliff);
    for (uint32_t i = 0; i < num_cliff; ++i) {
        t.cliff_tiles.push_back(fourcc_to_string(buf.read_u32()));
    }

    // Dimensions
    t.tile_width  = buf.read_u32(); // Mx = map_width + 1
    t.tile_height = buf.read_u32(); // My = map_height + 1
    t.center_offset_x = buf.read_f32();
    t.center_offset_y = buf.read_f32();

    // Validate dimensions
    if (t.tile_width <= 0 || t.tile_height <= 0)
        throw std::runtime_error("w3e: invalid dimensions");

    // Tile points (7 bytes each)
    int32_t count = t.tile_width * t.tile_height;
    t.tilepoints.reserve(count);

    for (int32_t i = 0; i < count; ++i) {
        TilePoint tp;

        // Bytes 0-1: ground height (int16)
        int16_t raw_height = static_cast<int16_t>(buf.read_u16());
        tp.height = static_cast<float>(raw_height);

        // Bytes 2-3: water height + map edge flag
        uint16_t raw_water = buf.read_u16();
        tp.water_height = static_cast<float>(raw_water & 0x7FFF);
        tp.map_edge = (raw_water >> 15) != 0;

        // Byte 4: flags (low 4 bits) + ground_texture (high 4 bits)
        uint8_t byte4 = buf.read_u8();
        tp.flags = byte4 & 0x0F;
        tp.ground_texture = (byte4 >> 4) & 0x0F;

        // Byte 5: texture detail
        tp.texture_detail = buf.read_u8();

        // Byte 6: cliff_texture (low 4 bits) + layer_height (high 4 bits)
        uint8_t byte6 = buf.read_u8();
        tp.cliff_texture = byte6 & 0x0F;
        tp.layer_height = (byte6 >> 4) & 0x0F;

        t.tilepoints.push_back(tp);
    }

    return t;
}

void write(Buffer& buf, const Terrain& t) {
    // Magic "W3E!"
    buf.write_u32(0x21453357);
    buf.write_u32(t.version);
    buf.write_u8(static_cast<uint8_t>(t.tileset));
    buf.write_u32(t.custom_tileset ? 1 : 0);

    // Ground tiles
    buf.write_u32(static_cast<uint32_t>(t.ground_tiles.size()));
    for (auto& s : t.ground_tiles) {
        buf.write_u32(string_to_fourcc(s));
    }

    // Cliff tiles
    buf.write_u32(static_cast<uint32_t>(t.cliff_tiles.size()));
    for (auto& s : t.cliff_tiles) {
        buf.write_u32(string_to_fourcc(s));
    }

    // Dimensions
    buf.write_u32(static_cast<uint32_t>(t.tile_width));
    buf.write_u32(static_cast<uint32_t>(t.tile_height));
    buf.write_f32(t.center_offset_x);
    buf.write_f32(t.center_offset_y);

    // Tile points
    for (auto& tp : t.tilepoints) {
        // Bytes 0-1: ground height
        buf.write_u16(static_cast<uint16_t>(static_cast<int16_t>(tp.height)));

        // Bytes 2-3: water height + map edge flag
        uint16_t raw_water = (static_cast<uint16_t>(tp.water_height) & 0x7FFF)
                           | (tp.map_edge ? 0x8000 : 0);
        buf.write_u16(raw_water);

        // Byte 4: flags + ground_texture
        buf.write_u8((tp.flags & 0x0F) | ((tp.ground_texture & 0x0F) << 4));

        // Byte 5: texture detail
        buf.write_u8(tp.texture_detail);

        // Byte 6: cliff_texture + layer_height
        buf.write_u8((tp.cliff_texture & 0x0F) | ((tp.layer_height & 0x0F) << 4));
    }
}

} // namespace w3e
