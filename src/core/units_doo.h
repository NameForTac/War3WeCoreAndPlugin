#pragma once

#include "buffer.h"
#include "doo.h"
#include <string>
#include <vector>
#include <cstdint>

// ============================================================
// war3mapUnits.doo — Placed Units and Items
// ============================================================
struct InventoryItem {
    int32_t slot = 0;
    std::string item_id; // 4-char code
};

struct AbilityMod {
    std::string ability_id; // 4-char code
    bool autocast = false;
    int32_t level = 1;
};

struct RandomUnitData {
    int32_t flags = 0;
    // flags == 0: level + class
    uint8_t level = 1;
    uint8_t unit_class = 0;
    // flags == 1: group + position
    int32_t group = 0;
    int32_t position = 0;
    // flags == 2: table
    struct TableEntry { std::string unit_id; int32_t chance; };
    std::vector<TableEntry> table;
};

struct PlacedUnit {
    std::string type_id;     // 4-char code ("iDNR"=random item, "uDNR"=random unit)
    int32_t variation = 0;
    float x = 0, y = 0, z = 0;
    float rotation = 0;
    float scale_x = 1, scale_y = 1, scale_z = 1;
    std::string type_id2;    // second type ID (usually same as type_id)
    uint8_t flags = 2;
    int32_t owner = 0;       // 0=player 1, 16=neutral passive
    int32_t hp = -1;         // -1=default
    int32_t mp = -1;         // -1=default
    int32_t item_table_ptr = -1;
    std::vector<DropItemSet> item_sets; // from doo.h
    int32_t gold_amount = 12500;
    int32_t target_acquisition = -1; // -1=normal, -2=hold
    int32_t hero_level = 0;
    int32_t strength = 0, agility = 0, intelligence = 0;
    std::vector<InventoryItem> inventory;
    std::vector<AbilityMod> skills;
    RandomUnitData random;
    uint32_t custom_color = 0;
    int32_t portal_target = -1;
    int32_t creation_number = 0;
};

struct PlacedUnits {
    uint32_t version = 8;
    uint32_t subversion = 0x0B;
    std::vector<PlacedUnit> units;
};

namespace units_doo {

PlacedUnits read(Buffer& buf);
void write(Buffer& buf, const PlacedUnits& units);

} // namespace units_doo
