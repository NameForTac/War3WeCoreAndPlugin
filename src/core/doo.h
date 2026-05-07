#pragma once

#include "buffer.h"
#include <string>
#include <vector>
#include <cstdint>

// ============================================================
// war3map.doo — Placed Doodads
// ============================================================
struct DropItem {
    std::string item_id; // 4-char code
    int32_t chance = 100;
};

struct DropItemSet {
    std::vector<DropItem> items;
};

struct PlacedDoodad {
    std::string type_id;     // 4-char code
    int32_t variation = 0;
    float x = 0, y = 0, z = 0;
    float rotation = 0;      // radians
    float scale_x = 1, scale_y = 1, scale_z = 1;
    uint8_t flags = 2;       // 0=invisible+no_coll, 1=visible+no_coll, 2=normal
    uint8_t life_percent = 100; // 0x64 = 100%
    int32_t item_table_ptr = -1; // -1 = none
    std::vector<DropItemSet> item_sets;
    int32_t editor_id = 0;
};

// Special doodad (from the extra section at end of file)
struct SpecialDoodad {
    std::string type_id; // 4-char code
    float z = 0, x = 0, y = 0;
};

struct PlacedDoodads {
    uint32_t version = 8;
    uint32_t subversion = 0x0B;
    std::vector<PlacedDoodad> doodads;
    std::vector<SpecialDoodad> special;
};

namespace doo {

PlacedDoodads read(Buffer& buf);
void write(Buffer& buf, const PlacedDoodads& doodads);

} // namespace doo
