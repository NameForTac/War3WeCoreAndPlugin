#include "units_doo.h"
#include <stdexcept>

namespace units_doo {

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

static RandomUnitData read_random(Buffer& buf) {
    RandomUnitData r;
    r.flags = buf.read_i32();
    if (r.flags == 0) {
        r.level = buf.read_u8();
        r.unit_class = buf.read_u8();
        buf.skip(2); // padding? 3 bytes level + 1 byte class = 4 bytes total
    } else if (r.flags == 1) {
        r.group = buf.read_i32();
        r.position = buf.read_i32();
    } else if (r.flags == 2) {
        int32_t count = buf.read_i32();
        r.table.reserve(count);
        for (int32_t i = 0; i < count; ++i) {
            RandomUnitData::TableEntry e;
            e.unit_id = fourcc(buf.read_u32());
            e.chance = buf.read_i32();
            r.table.push_back(std::move(e));
        }
    }
    return r;
}

static void write_random(Buffer& buf, const RandomUnitData& r) {
    buf.write_i32(r.flags);
    if (r.flags == 0) {
        buf.write_u8(r.level);
        buf.write_u8(r.unit_class);
        buf.write_u16(0); // padding
    } else if (r.flags == 1) {
        buf.write_i32(r.group);
        buf.write_i32(r.position);
    } else if (r.flags == 2) {
        buf.write_i32(static_cast<int32_t>(r.table.size()));
        for (auto& e : r.table) {
            buf.write_u32(to_fourcc(e.unit_id));
            buf.write_i32(e.chance);
        }
    }
}

static PlacedUnit read_unit(Buffer& buf) {
    PlacedUnit u;
    u.type_id   = fourcc(buf.read_u32());
    u.variation = buf.read_i32();
    u.x = buf.read_f32();
    u.y = buf.read_f32();
    u.z = buf.read_f32();
    u.rotation  = buf.read_f32();
    u.scale_x   = buf.read_f32();
    u.scale_y   = buf.read_f32();
    u.scale_z   = buf.read_f32();
    u.type_id2  = fourcc(buf.read_u32());
    u.flags     = buf.read_u8();
    u.owner     = buf.read_i32();
    buf.read_u8(); // unknown
    buf.read_u8(); // unknown
    u.hp        = buf.read_i32();
    u.mp        = buf.read_i32();
    u.item_table_ptr = buf.read_i32();
    int32_t set_count = buf.read_i32();
    u.item_sets = read_item_sets(buf, set_count);
    u.gold_amount = buf.read_i32();
    u.target_acquisition = buf.read_i32();
    u.hero_level = buf.read_i32();
    u.strength  = buf.read_i32();
    u.agility   = buf.read_i32();
    u.intelligence = buf.read_i32();

    // Inventory
    int32_t inv_count = buf.read_i32();
    u.inventory.reserve(inv_count);
    for (int32_t i = 0; i < inv_count; ++i) {
        InventoryItem item;
        item.slot = buf.read_i32();
        item.item_id = fourcc(buf.read_u32());
        u.inventory.push_back(std::move(item));
    }

    // Skills
    int32_t skill_count = buf.read_i32();
    u.skills.reserve(skill_count);
    for (int32_t i = 0; i < skill_count; ++i) {
        AbilityMod ab;
        ab.ability_id = fourcc(buf.read_u32());
        ab.autocast = (buf.read_u32() != 0);
        ab.level = buf.read_i32();
        u.skills.push_back(std::move(ab));
    }

    u.random = read_random(buf);

    u.custom_color = buf.read_u32();
    u.portal_target = buf.read_i32();
    u.creation_number = buf.read_i32();

    return u;
}

static void write_unit(Buffer& buf, const PlacedUnit& u) {
    buf.write_u32(to_fourcc(u.type_id));
    buf.write_i32(u.variation);
    buf.write_f32(u.x);
    buf.write_f32(u.y);
    buf.write_f32(u.z);
    buf.write_f32(u.rotation);
    buf.write_f32(u.scale_x);
    buf.write_f32(u.scale_y);
    buf.write_f32(u.scale_z);
    buf.write_u32(to_fourcc(u.type_id2));
    buf.write_u8(u.flags);
    buf.write_i32(u.owner);
    buf.write_u8(0); // unknown
    buf.write_u8(0); // unknown
    buf.write_i32(u.hp);
    buf.write_i32(u.mp);
    buf.write_i32(u.item_table_ptr);
    write_item_sets(buf, u.item_sets);
    buf.write_i32(u.gold_amount);
    buf.write_i32(u.target_acquisition);
    buf.write_i32(u.hero_level);
    buf.write_i32(u.strength);
    buf.write_i32(u.agility);
    buf.write_i32(u.intelligence);

    buf.write_i32(static_cast<int32_t>(u.inventory.size()));
    for (auto& inv : u.inventory) {
        buf.write_i32(inv.slot);
        buf.write_u32(to_fourcc(inv.item_id));
    }

    buf.write_i32(static_cast<int32_t>(u.skills.size()));
    for (auto& ab : u.skills) {
        buf.write_u32(to_fourcc(ab.ability_id));
        buf.write_u32(ab.autocast ? 1 : 0);
        buf.write_i32(ab.level);
    }

    write_random(buf, u.random);

    buf.write_u32(u.custom_color);
    buf.write_i32(u.portal_target);
    buf.write_i32(u.creation_number);
}

PlacedUnits read(Buffer& buf) {
    uint32_t magic = buf.read_u32();
    if (magic != 0x6f643357) // "W3do" little-endian
        throw std::runtime_error("units_doo: bad magic");

    PlacedUnits pu;
    pu.version    = buf.read_u32();
    pu.subversion = buf.read_u32();

    int32_t count = buf.read_i32();
    pu.units.reserve(count);
    for (int32_t i = 0; i < count; ++i)
        pu.units.push_back(read_unit(buf));

    return pu;
}

void write(Buffer& buf, const PlacedUnits& pu) {
    buf.write_u32(0x6f643357); // "W3do"
    buf.write_u32(pu.version);
    buf.write_u32(pu.subversion);

    buf.write_i32(static_cast<int32_t>(pu.units.size()));
    for (auto& u : pu.units)
        write_unit(buf, u);
}

} // namespace units_doo
