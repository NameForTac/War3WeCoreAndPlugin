#pragma once

#include <array>
#include <cstdint>
#include <string>
#include <variant>
#include <vector>
#include <cstring>
#include <algorithm>
#include <map>
#include <memory>
#include <stdexcept>
#include <cassert>

// ============================================================
// ObjectID — 4-character code (e.g. 'hfoo', 'A000')
// ============================================================
struct ObjectID {
    char code[4] = {};

    constexpr ObjectID() = default;
    constexpr explicit ObjectID(const char (&c)[5]) : code{c[0], c[1], c[2], c[3]} {}

    static ObjectID from_raw(uint32_t raw) {
        ObjectID id;
        id.code[0] = (raw >> 0) & 0xFF;
        id.code[1] = (raw >> 8) & 0xFF;
        id.code[2] = (raw >> 16) & 0xFF;
        id.code[3] = (raw >> 24) & 0xFF;
        return id;
    }

    static ObjectID from_string(const std::string& s) {
        ObjectID id{};
        for (size_t i = 0; i < 4 && i < s.size(); ++i)
            id.code[i] = s[i];
        return id;
    }

    uint32_t to_raw() const {
        return (uint8_t(code[0]) << 0)
             | (uint8_t(code[1]) << 8)
             | (uint8_t(code[2]) << 16)
             | (uint8_t(code[3]) << 24);
    }

    std::string str() const { return {code[0], code[1], code[2], code[3]}; }
    bool is_null() const { return code[0] == 0 && code[1] == 0 && code[2] == 0 && code[3] == 0; }

    bool operator==(const ObjectID& o) const {
        return code[0] == o.code[0] && code[1] == o.code[1]
            && code[2] == o.code[2] && code[3] == o.code[3];
    }
    bool operator!=(const ObjectID& o) const { return !(*this == o); }
    bool operator<(const ObjectID& o) const { return to_raw() < o.to_raw(); }
};

// ============================================================
// ValueType — modification value type (from MetaData.slk)
// ============================================================
enum class ValueType : uint32_t {
    Int       = 0,
    Real      = 1,
    Unreal    = 2,
    String    = 3,
    Bool      = 4,
    Char      = 5,
};

// ============================================================
// ObjectFileType — which type of object file (affects extra ints
// in modification structures)
// ============================================================
enum class ObjectFileType : uint32_t {
    Units         = 0,
    Items         = 1,
    Destructables = 2,
    Doodads       = 3,
    Abilities     = 4,
    Buffs         = 5,
    Upgrades      = 6,
};

// Number of extra int32 fields (level/variation/data_ptr) in
// modification structures, per file type:
//   w3u/w3t/w3b/w3h — 0
//   w3d              — 1 (variation)
//   w3q              — 1 (level)
//   w3a              — 2 (level + data_pointer)
inline int object_file_extra_ints(ObjectFileType type) {
    switch (type) {
        case ObjectFileType::Abilities: return 2;
        case ObjectFileType::Doodads:   return 1;
        case ObjectFileType::Upgrades:  return 1;
        default:                        return 0;
    }
}

// ============================================================
// Modification — a single field modification
// ============================================================
struct Modification {
    ObjectID        mod_id;      // 4-char field ID
    ValueType       type;        // value type
    int32_t         level;       // level (0 = non-leveled)
    int32_t         data_ptr;    // data pointer (usually 0)

    // The value: int32, float, or string
    std::variant<int32_t, float, std::string> value;

    int32_t  int_val()    const { return std::get<int32_t>(value); }
    float    float_val()  const { return std::get<float>(value); }
    const std::string& str_val() const { return std::get<std::string>(value); }
};

// ============================================================
// ObjectEntry — one object (original mod or custom object)
// ============================================================
struct ObjectEntry {
    ObjectID original_id;   // parent / original object ID
    ObjectID new_id;        // 0 = modify original, non-zero = custom object
    std::vector<Modification> mods;
};

// ============================================================
// ObjectFile — shared format for w3a/u/t/h/b/d/q
// ============================================================
struct ObjectFile {
    uint32_t version = 2;
    ObjectFileType file_type = ObjectFileType::Units;
    std::vector<ObjectEntry> original_objects;
    std::vector<ObjectEntry> custom_objects;
};

// ============================================================
// WTS — string table (TRIGSTR_XXX)
// ============================================================
using WTS = std::map<int, std::string>;

// ============================================================
// SLK — spreadsheet table
// ============================================================
struct SLKTable {
    std::vector<std::string> headers;
    std::vector<std::vector<std::string>> rows;

    bool empty() const { return rows.empty(); }
    size_t num_cols() const { return headers.size(); }
    size_t num_rows() const { return rows.size(); }
};

// ============================================================
// MapInfo — w3i parsed structure (forward decl, full in w3i.h)
// ============================================================
struct PlayerInfo {
    int32_t id = 0;
    int32_t type = 0;
    int32_t race = 0;
    int32_t fixed_start = 0;
    std::string name;
    float start_x = 0, start_y = 0;
    int32_t ally_low = 0, ally_high = 0;
};

struct ForceInfo {
    int32_t flags = 0;
    int32_t player_mask = 0;
    std::string name;
};

struct TechMod {
    int32_t player_id = 0;
    ObjectID id;
    int32_t unk = 0;
    int32_t level = 0;
    int32_t available = 0;
};

struct RandomUnitGroup {
    ObjectID id;
    int32_t unk = 0;
    int32_t chance = 0;
    int32_t count = 0;
    std::vector<int32_t> column_types;    // 0=unit, 1=item
    std::vector<std::vector<ObjectID>> ids;
};

struct RandomItemSet {
    ObjectID id;
    int32_t unk = 0;
    int32_t chance = 0;
    int32_t count = 0;
    std::vector<ObjectID> item_ids;
};

struct CameraBounds {
    float left = 0, right = 0, bottom = 0, top = 0;
    float comp_x = 0, comp_y = 0, comp_z = 0, comp_unk = 0;
};

struct FogState {
    int32_t type = 0;       // 0=none, 1=linear, 2=exp
    float start_z = 0;
    float end_z = 0;
    float density = 0;
    uint8_t r = 0, g = 0, b = 0, a = 0;
};

struct MapInfo {
    // Header
    int32_t version = 31;
    int32_t map_version = 0;
    int32_t editor_version = 0;
    std::array<int32_t, 4> war3_version{};
    std::string map_name;
    std::string author;
    std::string description;
    std::string players_desc;
    CameraBounds camera_bounds;
    std::array<int32_t, 4> camera_complements{};
    int32_t map_width = 0, map_height = 0;
    int32_t map_flags = 0;
    uint8_t ground_type = 0;

    // Version >= 25
    int32_t loading_screen_id = 0;       // type identifier
    std::string loading_screen_path;
    std::string loading_screen_text;
    std::string loading_screen_title;
    std::string loading_screen_subtitle;
    int32_t game_data_set = 0;
    std::string prologue_path;
    std::string prologue_text;
    std::string prologue_title;
    std::string prologue_subtitle;
    FogState fog;
    std::string weather_id;              // 4-byte code (e.g. "WEls")
    std::string sound_env;               // null-terminated string
    uint8_t light_env = 0;               // 1 byte
    uint8_t water_r = 0, water_g = 0, water_b = 0, water_a = 0;

    // Version >= 28
    int32_t script_type = 0;  // 0=JASS, 1=Lua

    // Version >= 31
    int32_t unk10 = 0;
    int32_t unk11 = 0;

    // Players
    std::vector<PlayerInfo> players;

    // Forces
    std::vector<ForceInfo> forces;

    // Tech
    std::vector<TechMod> tech_upgrades;
    std::vector<TechMod> tech_availabilities;
    std::vector<RandomUnitGroup> random_groups;
    std::vector<RandomItemSet> random_items;
};
