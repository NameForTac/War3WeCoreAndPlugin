#include "../src/core/w3i.h"
#include <cstring>

void test_w3i() {
    // Build a MapInfo, write it, read it back, and verify

    MapInfo original;
    original.version = 31;
    original.map_version = 1;
    original.editor_version = 100;
    original.map_name = "TestMap";
    original.author = "TestAuthor";
    original.description = "A test map";
    original.players_desc = "Players";
    original.camera_bounds = {-1.0f, 1.0f, -1.0f, 1.0f};
    original.camera_complements = {0, 0, 0, 0};
    original.map_width = 64;
    original.map_height = 64;
    original.map_flags = 0;
    original.ground_type = 0;

    // Version >= 25 fields
    original.loading_screen_id = 0;
    original.game_data_set = 0;
    original.script_type = 0; // JASS

    // Players
    PlayerInfo p1;
    p1.id = 0;
    p1.type = 0; // playable
    p1.race = 1; // human
    p1.fixed_start = 1;
    p1.name = "Player 1";
    p1.start_x = 0.0f;
    p1.start_y = 0.0f;
    p1.ally_low = 1;
    p1.ally_high = 0;
    original.players.push_back(p1);

    // Forces
    ForceInfo f1;
    f1.flags = 0x01; // allied
    f1.player_mask = 0x01; // player 0
    f1.name = "Force 1";
    original.forces.push_back(f1);

    // Write, read back, write again — verify roundtrip consistency
    Buffer w;
    w3i::write(w, original);
    auto data = w.take_data();
    size_t orig_size = data.size();

    Buffer r(std::move(data));
    auto parsed = w3i::read(r);

    // Verify fields...
    if (parsed.version != original.version)
        throw std::runtime_error("version mismatch");
    if (parsed.map_name != original.map_name)
        throw std::runtime_error("map_name mismatch");
    if (parsed.author != original.author)
        throw std::runtime_error("author mismatch");
    if (parsed.description != original.description)
        throw std::runtime_error("description mismatch");
    if (parsed.map_width != original.map_width)
        throw std::runtime_error("map_width mismatch");
    if (parsed.map_height != original.map_height)
        throw std::runtime_error("map_height mismatch");
    if (parsed.players.size() != 1)
        throw std::runtime_error("player count mismatch");
    if (parsed.players[0].name != "Player 1")
        throw std::runtime_error("player name mismatch");
    if (parsed.forces.size() != 1)
        throw std::runtime_error("force count mismatch");
    if (parsed.forces[0].name != "Force 1")
        throw std::runtime_error("force name mismatch");
    if (parsed.script_type != 0)
        throw std::runtime_error("script_type mismatch");

    // Write again and check roundtrip consistency
    Buffer w2;
    w3i::write(w2, parsed);
    auto data2 = w2.take_data();
    if (data2.size() != orig_size)
        throw std::runtime_error("roundtrip size mismatch");
}
