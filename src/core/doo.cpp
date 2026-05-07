#include "doo.h"
#include <stdexcept>

namespace doo {

// ============================================================
// Shared helpers for 4-char codes and item sets
// ============================================================
static std::string fourcc(uint32_t code) {
    char buf[5] = {
        (char)(code & 0xFF), (char)((code >> 8) & 0xFF),
        (char)((code >> 16) & 0xFF), (char)((code >> 24) & 0xFF), 0
    };
    return std::string(buf);
}

static uint32_t to_fourcc(const std::string& s) {
    uint32_t code = 0;
    for (size_t i = 0; i < 4 && i < s.size(); ++i)
        code |= (uint8_t)s[i] << (i * 8);
    return code;
}

// Read item sets from buffer
static std::vector<DropItemSet> read_item_sets(Buffer& buf, int32_t count) {
    std::vector<DropItemSet> sets;
    sets.reserve(count);
    for (int32_t s = 0; s < count; ++s) {
        DropItemSet set;
        int32_t item_count = buf.read_i32();
        set.items.reserve(item_count);
        for (int32_t i = 0; i < item_count; ++i) {
            DropItem item;
            item.item_id = fourcc(buf.read_u32());
            item.chance = buf.read_i32();
            set.items.push_back(std::move(item));
        }
        sets.push_back(std::move(set));
    }
    return sets;
}

static void write_item_sets(Buffer& buf, const std::vector<DropItemSet>& sets) {
    buf.write_i32(static_cast<int32_t>(sets.size()));
    for (auto& set : sets) {
        buf.write_i32(static_cast<int32_t>(set.items.size()));
        for (auto& item : set.items) {
            buf.write_u32(to_fourcc(item.item_id));
            buf.write_i32(item.chance);
        }
    }
}

// ============================================================
// Doodad read/write
// ============================================================
static PlacedDoodad read_doodad(Buffer& buf) {
    PlacedDoodad d;
    d.type_id   = fourcc(buf.read_u32());
    d.variation = buf.read_i32();
    d.x = buf.read_f32();
    d.y = buf.read_f32();
    d.z = buf.read_f32();
    d.rotation  = buf.read_f32();
    d.scale_x   = buf.read_f32();
    d.scale_y   = buf.read_f32();
    d.scale_z   = buf.read_f32();
    d.flags     = buf.read_u8();
    d.life_percent = buf.read_u8();
    d.item_table_ptr = buf.read_i32();
    int32_t set_count = buf.read_i32();
    d.item_sets = read_item_sets(buf, set_count);
    d.editor_id = buf.read_i32();
    return d;
}

static void write_doodad(Buffer& buf, const PlacedDoodad& d) {
    buf.write_u32(to_fourcc(d.type_id));
    buf.write_i32(d.variation);
    buf.write_f32(d.x);
    buf.write_f32(d.y);
    buf.write_f32(d.z);
    buf.write_f32(d.rotation);
    buf.write_f32(d.scale_x);
    buf.write_f32(d.scale_y);
    buf.write_f32(d.scale_z);
    buf.write_u8(d.flags);
    buf.write_u8(d.life_percent);
    buf.write_i32(d.item_table_ptr);
    write_item_sets(buf, d.item_sets);
    buf.write_i32(d.editor_id);
}

// ============================================================
// Top-level read/write
// ============================================================
PlacedDoodads read(Buffer& buf) {
    uint32_t magic = buf.read_u32();
    if (magic != 0x6f643357) // "W3do" little-endian
        throw std::runtime_error("doo: bad magic");

    PlacedDoodads pd;
    pd.version    = buf.read_u32();
    pd.subversion = buf.read_u32();

    int32_t count = buf.read_i32();
    pd.doodads.reserve(count);
    for (int32_t i = 0; i < count; ++i)
        pd.doodads.push_back(read_doodad(buf));

    // Special doodads section
    uint32_t sp_version = buf.read_u32();
    int32_t sp_count = buf.read_i32();
    pd.special.reserve(sp_count);
    for (int32_t i = 0; i < sp_count; ++i) {
        SpecialDoodad sd;
        sd.type_id = fourcc(buf.read_u32());
        sd.z = buf.read_f32();
        sd.x = buf.read_f32();
        sd.y = buf.read_f32();
        pd.special.push_back(std::move(sd));
    }

    return pd;
}

void write(Buffer& buf, const PlacedDoodads& pd) {
    buf.write_u32(0x6f643357); // "W3do"
    buf.write_u32(pd.version);
    buf.write_u32(pd.subversion);

    buf.write_i32(static_cast<int32_t>(pd.doodads.size()));
    for (auto& d : pd.doodads)
        write_doodad(buf, d);

    // Special doodads
    buf.write_u32(0); // special section version
    buf.write_i32(static_cast<int32_t>(pd.special.size()));
    for (auto& sd : pd.special) {
        buf.write_u32(to_fourcc(sd.type_id));
        buf.write_f32(sd.z);
        buf.write_f32(sd.x);
        buf.write_f32(sd.y);
    }
}

} // namespace doo
