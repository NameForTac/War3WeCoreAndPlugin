#include "wai.h"
#include <stdexcept>

namespace wai {

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

AIFile read(Buffer& buf) {
    AIFile ai;

    ai.version = buf.read_i32();
    ai.name    = buf.read_string();
    ai.race    = buf.read_i32();
    ai.options = buf.read_i32();

    ai.worker_count       = buf.read_i32();
    ai.gold_worker_id     = fourcc(buf.read_u32());
    ai.wood_worker_id     = fourcc(buf.read_u32());
    ai.base_building_id   = fourcc(buf.read_u32());
    ai.gold_mill_id       = fourcc(buf.read_u32());

    // Conditions
    int32_t cond_count = buf.read_i32();
    ai.cond_unknown = buf.read_i32(); // should be 7
    ai.conditions.reserve(cond_count);
    for (int32_t i = 0; i < cond_count; ++i) {
        AICondition c;
        c.type       = buf.read_i32();
        c.player     = buf.read_i32();
        c.arg        = buf.read_i32();
        c.comparator = buf.read_i32();
        c.value      = buf.read_f32();
        ai.conditions.push_back(std::move(c));
    }

    // Heroes
    for (int i = 0; i < 3; ++i)
        ai.hero_ids[i] = fourcc(buf.read_u32());

    for (int i = 0; i < 6; ++i)
        ai.hero_train_pct[i] = buf.read_i32();

    for (int i = 0; i < 9; ++i)
        for (int j = 0; j < 10; ++j)
            ai.hero_skills[i][j] = fourcc(buf.read_u32());

    // Build priorities
    int32_t build_count = buf.read_i32();
    ai.build_priorities.reserve(build_count);
    for (int32_t i = 0; i < build_count; ++i) {
        BuildPriority bp;
        bp.unit_id         = fourcc(buf.read_u32());
        bp.priority        = buf.read_i32();
        bp.condition_index = buf.read_i32();
        ai.build_priorities.push_back(std::move(bp));
    }

    // Harvest priorities
    int32_t harvest_count = buf.read_i32();
    ai.harvest_priorities.reserve(harvest_count);
    for (int32_t i = 0; i < harvest_count; ++i) {
        HarvestPriority hp;
        hp.unit_id  = fourcc(buf.read_u32());
        hp.priority = buf.read_i32();
        ai.harvest_priorities.push_back(std::move(hp));
    }

    // Target priorities
    int32_t target_count = buf.read_i32();
    ai.target_priorities.reserve(target_count);
    for (int32_t i = 0; i < target_count; ++i) {
        TargetPriority tp;
        tp.unit_id  = fourcc(buf.read_u32());
        tp.priority = buf.read_i32();
        ai.target_priorities.push_back(std::move(tp));
    }

    // Attack
    ai.repeat_waves   = buf.read_i32();
    ai.min_forces     = buf.read_i32();
    ai.initial_delay  = buf.read_i32();

    int32_t group_count = buf.read_i32();
    ai.attack_groups.reserve(group_count);
    for (int32_t i = 0; i < group_count; ++i) {
        AttackGroup ag;
        ag.flags = buf.read_i32();
        int32_t unit_count = buf.read_i32();
        ag.unit_ids.reserve(unit_count);
        for (int32_t j = 0; j < unit_count; ++j)
            ag.unit_ids.push_back(fourcc(buf.read_u32()));
        ai.attack_groups.push_back(std::move(ag));
    }

    int32_t wave_count = buf.read_i32();
    ai.attack_waves.reserve(wave_count);
    for (int32_t i = 0; i < wave_count; ++i) {
        AttackWave aw;
        aw.group_index   = buf.read_i32();
        aw.delay         = buf.read_i32();
        aw.rest          = buf.read_i32();
        aw.initial_delay = buf.read_i32();
        ai.attack_waves.push_back(std::move(aw));
    }

    // Footer
    ai.footer_unknown = buf.read_i32();
    ai.game_options   = buf.read_i32();
    ai.game_speed     = buf.read_i32();
    ai.map_path       = buf.read_string();

    int32_t player_count = buf.read_i32();
    ai.players.reserve(player_count);
    for (int32_t i = 0; i < player_count; ++i) {
        AIPlayerData p;
        p.id   = buf.read_i32();
        p.race = buf.read_i32();
        p.name = buf.read_string();
        p.has_data = true;
        ai.players.push_back(std::move(p));
    }

    // Read any trailing bytes (some files have extra data)
    while (!buf.eof())
        ai.footer_extra.push_back(buf.read_i32());

    return ai;
}

void write(Buffer& buf, const AIFile& ai) {
    buf.write_i32(ai.version);
    buf.write_string(ai.name);
    buf.write_i32(ai.race);
    buf.write_i32(ai.options);
    buf.write_i32(ai.worker_count);
    buf.write_u32(to_fourcc(ai.gold_worker_id));
    buf.write_u32(to_fourcc(ai.wood_worker_id));
    buf.write_u32(to_fourcc(ai.base_building_id));
    buf.write_u32(to_fourcc(ai.gold_mill_id));

    // Conditions
    buf.write_i32(static_cast<int32_t>(ai.conditions.size()));
    buf.write_i32(ai.cond_unknown);
    for (auto& c : ai.conditions) {
        buf.write_i32(c.type);
        buf.write_i32(c.player);
        buf.write_i32(c.arg);
        buf.write_i32(c.comparator);
        buf.write_f32(c.value);
    }

    // Heroes
    for (int i = 0; i < 3; ++i)
        buf.write_u32(to_fourcc(ai.hero_ids[i]));
    for (int i = 0; i < 6; ++i)
        buf.write_i32(ai.hero_train_pct[i]);
    for (int i = 0; i < 9; ++i)
        for (int j = 0; j < 10; ++j)
            buf.write_u32(to_fourcc(ai.hero_skills[i][j]));

    // Build priorities
    buf.write_i32(static_cast<int32_t>(ai.build_priorities.size()));
    for (auto& bp : ai.build_priorities) {
        buf.write_u32(to_fourcc(bp.unit_id));
        buf.write_i32(bp.priority);
        buf.write_i32(bp.condition_index);
    }

    // Harvest priorities
    buf.write_i32(static_cast<int32_t>(ai.harvest_priorities.size()));
    for (auto& hp : ai.harvest_priorities) {
        buf.write_u32(to_fourcc(hp.unit_id));
        buf.write_i32(hp.priority);
    }

    // Target priorities
    buf.write_i32(static_cast<int32_t>(ai.target_priorities.size()));
    for (auto& tp : ai.target_priorities) {
        buf.write_u32(to_fourcc(tp.unit_id));
        buf.write_i32(tp.priority);
    }

    // Attack
    buf.write_i32(ai.repeat_waves);
    buf.write_i32(ai.min_forces);
    buf.write_i32(ai.initial_delay);

    buf.write_i32(static_cast<int32_t>(ai.attack_groups.size()));
    for (auto& ag : ai.attack_groups) {
        buf.write_i32(ag.flags);
        buf.write_i32(static_cast<int32_t>(ag.unit_ids.size()));
        for (auto& uid : ag.unit_ids)
            buf.write_u32(to_fourcc(uid));
    }

    buf.write_i32(static_cast<int32_t>(ai.attack_waves.size()));
    for (auto& aw : ai.attack_waves) {
        buf.write_i32(aw.group_index);
        buf.write_i32(aw.delay);
        buf.write_i32(aw.rest);
        buf.write_i32(aw.initial_delay);
    }

    // Footer
    buf.write_i32(ai.footer_unknown);
    buf.write_i32(ai.game_options);
    buf.write_i32(ai.game_speed);
    buf.write_string(ai.map_path);

    buf.write_i32(static_cast<int32_t>(ai.players.size()));
    for (auto& p : ai.players) {
        buf.write_i32(p.id);
        buf.write_i32(p.race);
        buf.write_string(p.name);
    }

    // Trailing data
    for (auto& v : ai.footer_extra)
        buf.write_i32(v);
}

} // namespace wai
