#include "w3i.h"
#include <stdexcept>

namespace w3i {

// Read a 4-byte fixed-size string (format "c4" in Lua)
static std::string read_c4(Buffer& buf) {
    std::string s(4, '\0');
    buf.read_raw(&s[0], 4);
    // Trim null padding
    auto pos = s.find('\0');
    if (pos != std::string::npos) s.resize(pos);
    return s;
}

// Write a 4-byte fixed-size string (format "c4" in Lua)
static void write_c4(Buffer& buf, const std::string& s) {
    char c4[4] = {};
    for (size_t i = 0; i < 4 && i < s.size(); ++i)
        c4[i] = s[i];
    buf.write_raw(c4, 4);
}

MapInfo read(Buffer& buf) {
    MapInfo info;

    // === Header ===
    info.version        = buf.read_i32();
    info.map_version    = buf.read_i32();
    info.editor_version = buf.read_i32();

    if (info.version >= 28) {
        info.war3_version[0] = buf.read_i32();
        info.war3_version[1] = buf.read_i32();
        info.war3_version[2] = buf.read_i32();
        info.war3_version[3] = buf.read_i32();
    }

    info.map_name           = buf.read_string();
    info.author             = buf.read_string();
    info.description        = buf.read_string();
    info.players_desc       = buf.read_string();

    // 8 floats: camera bounds (left, right, bottom, top, comp_x, comp_y, comp_z, comp_unk)
    info.camera_bounds.left   = buf.read_f32();
    info.camera_bounds.right  = buf.read_f32();
    info.camera_bounds.bottom = buf.read_f32();
    info.camera_bounds.top    = buf.read_f32();
    info.camera_bounds.comp_x = buf.read_f32();
    info.camera_bounds.comp_y = buf.read_f32();
    info.camera_bounds.comp_z = buf.read_f32();
    info.camera_bounds.comp_unk = buf.read_f32();

    // 4 longs: camera complements
    info.camera_complements[0] = buf.read_i32();
    info.camera_complements[1] = buf.read_i32();
    info.camera_complements[2] = buf.read_i32();
    info.camera_complements[3] = buf.read_i32();

    info.map_width   = buf.read_i32();
    info.map_height  = buf.read_i32();
    info.map_flags   = buf.read_i32();
    info.ground_type = buf.read_u8();

    // === Version >= 25 extra fields ===
    if (info.version >= 25) {
        info.loading_screen_id        = buf.read_i32();
        info.loading_screen_path      = buf.read_string();
        info.loading_screen_text      = buf.read_string();
        info.loading_screen_title     = buf.read_string();
        info.loading_screen_subtitle  = buf.read_string();

        info.game_data_set            = buf.read_i32();

        info.prologue_path            = buf.read_string();
        info.prologue_text            = buf.read_string();
        info.prologue_title           = buf.read_string();
        info.prologue_subtitle        = buf.read_string();

        info.fog.type    = buf.read_i32();
        info.fog.start_z = buf.read_f32();
        info.fog.end_z   = buf.read_f32();
        info.fog.density = buf.read_f32();
        info.fog.r = buf.read_u8();
        info.fog.g = buf.read_u8();
        info.fog.b = buf.read_u8();
        info.fog.a = buf.read_u8();

        info.weather_id  = read_c4(buf);
        info.sound_env   = buf.read_string();
        info.light_env   = buf.read_u8();

        info.water_r = buf.read_u8();
        info.water_g = buf.read_u8();
        info.water_b = buf.read_u8();
        info.water_a = buf.read_u8();

        if (info.version >= 28)
            info.script_type = buf.read_i32();

        if (info.version >= 31) {
            info.unk10 = buf.read_i32();
            info.unk11 = buf.read_i32();
        }
    }

    // === Players ===
    {
        int32_t count = buf.read_i32();
        info.players.reserve(count);
        for (int i = 0; i < count; ++i) {
            PlayerInfo p;
            p.id          = buf.read_i32();
            p.type        = buf.read_i32();
            p.race        = buf.read_i32();
            p.fixed_start = buf.read_i32();
            p.name        = buf.read_string();
            p.start_x     = buf.read_f32();
            p.start_y     = buf.read_f32();
            p.ally_low    = buf.read_i32();
            p.ally_high   = buf.read_i32();
            if (info.version >= 31) {
                buf.read_i32(); // unk12
                buf.read_i32(); // unk13
            }
            info.players.push_back(std::move(p));
        }
    }

    // === Forces ===
    {
        int32_t count = buf.read_i32();
        info.forces.reserve(count);
        for (int i = 0; i < count; ++i) {
            ForceInfo f;
            f.flags       = buf.read_i32();
            f.player_mask = buf.read_u32(); // unsigned in w3x2lni ('L')
            f.name        = buf.read_string();
            info.forces.push_back(std::move(f));
        }
    }

    // === Tech upgrades ===
    {
        int32_t count = buf.read_i32();
        info.tech_upgrades.reserve(count);
        for (int i = 0; i < count; ++i) {
            TechMod t;
            t.player_id = buf.read_i32();
            t.id        = buf.read_object_id();
            t.unk       = buf.read_i32();
            t.level     = buf.read_i32();
            t.available = buf.read_i32();
            info.tech_upgrades.push_back(std::move(t));
        }
    }

    // === Tech availability ===
    {
        int32_t count = buf.read_i32();
        info.tech_availabilities.reserve(count);
        for (int i = 0; i < count; ++i) {
            TechMod t;
            t.player_id = buf.read_i32();
            t.id        = buf.read_object_id();
            t.unk       = buf.read_i32();
            t.level     = buf.read_i32();
            t.available = buf.read_i32();
            info.tech_availabilities.push_back(std::move(t));
        }
    }

    // === Random groups ===
    {
        int32_t count = buf.read_i32();
        info.random_groups.reserve(count);
        for (int i = 0; i < count; ++i) {
            RandomUnitGroup g;
            g.id     = buf.read_object_id();
            g.unk    = buf.read_i32();
            g.chance = buf.read_i32();
            g.count  = buf.read_i32();
            int32_t total = buf.read_i32();
            g.column_types.resize(g.count);
            for (int c = 0; c < g.count; ++c)
                g.column_types[c] = buf.read_i32();
            int remaining = total - 2 - g.count;
            g.ids.resize(g.count);
            for (int c = 0; c < g.count; ++c) {
                int per_col = remaining / g.count;
                if (c < remaining % g.count) per_col++;
                for (int j = 0; j < per_col; ++j) {
                    g.ids[c].push_back(buf.read_object_id());
                    buf.read_i32(); // weight
                }
            }
            info.random_groups.push_back(std::move(g));
        }
    }

    // === Random items ===
    {
        int32_t count = buf.read_i32();
        info.random_items.reserve(count);
        for (int i = 0; i < count; ++i) {
            RandomItemSet s;
            s.id     = buf.read_object_id();
            s.unk    = buf.read_i32();
            s.chance = buf.read_i32();
            s.count  = buf.read_i32();
            int32_t total = buf.read_i32();
            int item_count = total - 2;
            for (int j = 0; j < item_count; ++j)
                s.item_ids.push_back(buf.read_object_id());
            info.random_items.push_back(std::move(s));
        }
    }

    return info;
}

// ---- Write ----

void write(Buffer& buf, const MapInfo& info) {
    // === Header ===
    buf.write_i32(info.version);
    buf.write_i32(info.map_version);
    buf.write_i32(info.editor_version);

    if (info.version >= 28) {
        for (int i = 0; i < 4; ++i)
            buf.write_i32(info.war3_version[i]);
    }

    buf.write_string(info.map_name);
    buf.write_string(info.author);
    buf.write_string(info.description);
    buf.write_string(info.players_desc);

    // 8 floats: camera bounds
    buf.write_f32(info.camera_bounds.left);
    buf.write_f32(info.camera_bounds.right);
    buf.write_f32(info.camera_bounds.bottom);
    buf.write_f32(info.camera_bounds.top);
    buf.write_f32(info.camera_bounds.comp_x);
    buf.write_f32(info.camera_bounds.comp_y);
    buf.write_f32(info.camera_bounds.comp_z);
    buf.write_f32(info.camera_bounds.comp_unk);

    // 4 longs: camera complements
    for (int i = 0; i < 4; ++i)
        buf.write_i32(info.camera_complements[i]);

    buf.write_i32(info.map_width);
    buf.write_i32(info.map_height);
    buf.write_i32(info.map_flags);
    buf.write_u8(info.ground_type);

    // === Version >= 25 ===
    if (info.version >= 25) {
        buf.write_i32(info.loading_screen_id);
        buf.write_string(info.loading_screen_path);
        buf.write_string(info.loading_screen_text);
        buf.write_string(info.loading_screen_title);
        buf.write_string(info.loading_screen_subtitle);

        buf.write_i32(info.game_data_set);

        buf.write_string(info.prologue_path);
        buf.write_string(info.prologue_text);
        buf.write_string(info.prologue_title);
        buf.write_string(info.prologue_subtitle);

        buf.write_i32(info.fog.type);
        buf.write_f32(info.fog.start_z);
        buf.write_f32(info.fog.end_z);
        buf.write_f32(info.fog.density);
        buf.write_u8(info.fog.r);
        buf.write_u8(info.fog.g);
        buf.write_u8(info.fog.b);
        buf.write_u8(info.fog.a);

        write_c4(buf, info.weather_id);
        buf.write_string(info.sound_env);
        buf.write_u8(info.light_env);

        buf.write_u8(info.water_r);
        buf.write_u8(info.water_g);
        buf.write_u8(info.water_b);
        buf.write_u8(info.water_a);

        if (info.version >= 28)
            buf.write_i32(info.script_type);
        if (info.version >= 31) {
            buf.write_i32(info.unk10);
            buf.write_i32(info.unk11);
        }
    }

    // === Players ===
    buf.write_i32((int32_t)info.players.size());
    for (auto& p : info.players) {
        buf.write_i32(p.id);
        buf.write_i32(p.type);
        buf.write_i32(p.race);
        buf.write_i32(p.fixed_start);
        buf.write_string(p.name);
        buf.write_f32(p.start_x);
        buf.write_f32(p.start_y);
        buf.write_i32(p.ally_low);
        buf.write_i32(p.ally_high);
        if (info.version >= 31) {
            buf.write_i32(0); // unk12
            buf.write_i32(0); // unk13
        }
    }

    // === Forces ===
    buf.write_i32((int32_t)info.forces.size());
    for (auto& f : info.forces) {
        buf.write_i32(f.flags);
        buf.write_u32((uint32_t)f.player_mask);
        buf.write_string(f.name);
    }

    // === Tech upgrades ===
    buf.write_i32((int32_t)info.tech_upgrades.size());
    for (auto& t : info.tech_upgrades) {
        buf.write_i32(t.player_id);
        buf.write_object_id(t.id);
        buf.write_i32(t.unk);
        buf.write_i32(t.level);
        buf.write_i32(t.available);
    }

    // === Tech availability ===
    buf.write_i32((int32_t)info.tech_availabilities.size());
    for (auto& t : info.tech_availabilities) {
        buf.write_i32(t.player_id);
        buf.write_object_id(t.id);
        buf.write_i32(t.unk);
        buf.write_i32(t.level);
        buf.write_i32(t.available);
    }

    // === Random groups ===
    buf.write_i32((int32_t)info.random_groups.size());
    for (auto& g : info.random_groups) {
        buf.write_object_id(g.id);
        buf.write_i32(0); // unk
        buf.write_i32(g.chance);
        buf.write_i32(g.count);
        int32_t total = 2;
        for (auto& col : g.ids) total += (int32_t)col.size();
        buf.write_i32(total);
        for (auto& ct : g.column_types)
            buf.write_i32(ct);
        for (auto& col : g.ids) {
            for (auto& id : col)
                buf.write_object_id(id);
            for (size_t j = 0; j < col.size(); ++j)
                buf.write_i32(100); // weight
        }
    }

    // === Random items ===
    buf.write_i32((int32_t)info.random_items.size());
    for (auto& s : info.random_items) {
        buf.write_object_id(s.id);
        buf.write_i32(0); // unk
        buf.write_i32(s.chance);
        buf.write_i32(s.count);
        buf.write_i32(2 + (int32_t)s.item_ids.size());
        for (auto& id : s.item_ids)
            buf.write_object_id(id);
    }
}

} // namespace w3i
