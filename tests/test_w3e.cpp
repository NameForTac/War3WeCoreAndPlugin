#include "../src/core/w3e.h"
#include <cstring>
#include <cmath>

void test_w3e() {
    // ── Build a Terrain, write, read back, verify ──

    Terrain original;
    original.version = 11;
    original.tileset = 'L'; // Lordaeron Summer
    original.custom_tileset = false;
    original.ground_tiles = {"Ldrt", "Lgrs", "Ldir"};
    original.cliff_tiles  = {"CLdi", "CLgr"};
    original.tile_width  = 4;  // 3x3 map tiles → 4×4 tile points
    original.tile_height = 4;
    original.center_offset_x = -192.0f;
    original.center_offset_y = -192.0f;

    // Create tile points (4×4 = 16)
    original.tilepoints.reserve(16);
    for (int i = 0; i < 16; ++i) {
        TilePoint tp;
        tp.height = static_cast<float>(0x2000 + i * 64);
        tp.water_height = static_cast<float>(i * 10);
        tp.map_edge = (i == 0);
        tp.flags = (i % 4 == 0) ? 1 : 0; // ramp on every 4th
        tp.ground_texture = i % 4;
        tp.texture_detail = static_cast<uint8_t>(i * 17);
        tp.cliff_texture = (i / 4) % 4;
        tp.layer_height = static_cast<uint8_t>(i / 4);
        original.tilepoints.push_back(tp);
    }

    // Write
    Buffer buf;
    w3e::write(buf, original);
    auto data = buf.take_data();

    // Read back
    Buffer read_buf(std::move(data));
    auto result = w3e::read(read_buf);

    // Verify header
    if (result.version != original.version)             throw "version mismatch";
    if (result.tileset != original.tileset)             throw "tileset mismatch";
    if (result.custom_tileset != original.custom_tileset) throw "custom_tileset mismatch";

    // Verify tile lists
    if (result.ground_tiles.size() != 3)  throw "ground_tiles count";
    if (result.ground_tiles[0] != "Ldrt") throw "ground_tiles[0]";
    if (result.ground_tiles[1] != "Lgrs") throw "ground_tiles[1]";
    if (result.ground_tiles[2] != "Ldir") throw "ground_tiles[2]";
    if (result.cliff_tiles.size() != 2)   throw "cliff_tiles count";
    if (result.cliff_tiles[0] != "CLdi")  throw "cliff_tiles[0]";
    if (result.cliff_tiles[1] != "CLgr")  throw "cliff_tiles[1]";

    // Verify dimensions
    if (result.tile_width  != 4) throw "tile_width";
    if (result.tile_height != 4) throw "tile_height";
    if (std::fabs(result.center_offset_x - (-192.0f)) > 0.001f) throw "center_offset_x";
    if (std::fabs(result.center_offset_y - (-192.0f)) > 0.001f) throw "center_offset_y";

    // Verify tile points (spot-check)
    if (result.tilepoints.size() != 16) throw "tilepoints count";

    // Check first tile point
    if (std::fabs(result.tilepoints[0].height - static_cast<float>(0x2000)) > 0.001f)
        throw "tilepoints[0].height";
    if (result.tilepoints[0].map_edge != true) throw "tilepoints[0].map_edge";
    if (result.tilepoints[0].flags != 1)       throw "tilepoints[0].flags";
    if (result.tilepoints[0].ground_texture != 0) throw "tilepoints[0].ground_texture";

    // Check tile point at index 5
    if (std::fabs(result.tilepoints[5].height - static_cast<float>(0x2000 + 5 * 64)) > 0.001f)
        throw "tilepoints[5].height";
    if (result.tilepoints[5].ground_texture != 1) throw "tilepoints[5].ground_texture";
    if (result.tilepoints[5].cliff_texture != 1)  throw "tilepoints[5].cliff_texture";
    if (result.tilepoints[5].layer_height != 1)   throw "tilepoints[5].layer_height";

    // Verify end of buffer
    if (!read_buf.eof()) throw "not all data consumed";
}
