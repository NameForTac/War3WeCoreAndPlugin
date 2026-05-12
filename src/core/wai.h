#pragma once

#include "buffer.h"
#include <string>
#include <vector>

// ============================================================
// war3map.wai — Warcraft III AI file (version 2)
// ============================================================
struct AICondition {
    int32_t type = 0;        // 0=time, 1=gold, 2=lumber, 3=town_units, etc.
    int32_t player = 0;      // 0=player 1, 1=player 2, 2=both
    int32_t arg = 0;         // argument (unit ID, count, etc. depending on type)
    int32_t comparator = 0;  // 0=less, 1=equal, 2=greater
    float value = 0;         // threshold value
};

struct BuildPriority {
    std::string unit_id; // 4-char code
    int32_t priority = 5;
    int32_t condition_index = -1; // -1 = always
};

struct HarvestPriority {
    std::string unit_id; // 4-char code
    int32_t priority = 5;
};

struct TargetPriority {
    std::string unit_id; // 4-char code
    int32_t priority = 5;
};

struct AttackGroup {
    int32_t flags = 0;           // melee/ranged/casters mix flags
    std::vector<std::string> unit_ids; // 4-char unit codes
};

struct AttackWave {
    int32_t group_index = -1;    // attack group index, -1 = random
    int32_t delay = 0;           // delay between waves (seconds)
    int32_t rest = 0;            // rest after wave (seconds)
    int32_t initial_delay = 0;   // initial delay (seconds)
};

struct AIPlayer {
    int32_t id = 0;
    int32_t race = 1;
    std::string name;
    std::vector<int32_t> extra; // remaining player data for roundtrip
};

struct AIPlayerData {
    int32_t id = 0;
    int32_t race = 0;
    std::string name;
    bool has_data = false;
};

struct AIFile {
    // -- Header --
    int32_t version = 2;
    std::string name;
    int32_t race = 0;        // 1=human, 2=orc, 4=undead, 8=nelf, -1=random
    int32_t options = 0;     // hiword=difficulty, loword=flags
    int32_t worker_count = 0;
    std::string gold_worker_id;   // 4-char
    std::string wood_worker_id;   // 4-char
    std::string base_building_id; // 4-char
    std::string gold_mill_id;     // 4-char

    // -- Conditions --
    int32_t cond_unknown = 7; // fixed value after condition count
    std::vector<AICondition> conditions;

    // -- Heroes --
    std::string hero_ids[3];              // 3 hero IDs
    int32_t hero_train_pct[6] = {};       // 6 training percentages

    // -- Skills (9 heroes × 10 skills) --
    std::string hero_skills[9][10];       // each is a 4-char ability ID

    // -- Priorities --
    std::vector<BuildPriority> build_priorities;
    std::vector<HarvestPriority> harvest_priorities;
    std::vector<TargetPriority> target_priorities;

    // -- Attack --
    int32_t repeat_waves = 0;
    int32_t min_forces = 0;
    int32_t initial_delay = 0;
    std::vector<AttackGroup> attack_groups;
    std::vector<AttackWave> attack_waves;

    // -- Footer --
    int32_t footer_unknown = 1;
    int32_t game_options = 0;
    int32_t game_speed = 0;   // 1=slow, 2=normal, 3=fast
    std::string map_path;
    std::vector<AIPlayerData> players;
    std::vector<int32_t> footer_extra; // trailing unknown data
};

namespace wai {

AIFile read(Buffer& buf);
void write(Buffer& buf, const AIFile& ai);

} // namespace wai
