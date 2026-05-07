#include "../src/core/archive.h"
#include "../src/core/w3i.h"
#include "../src/core/buffer.h"
#include <cstdio>
#include <cstdlib>
#include <string>

static std::string temp_path() {
    return "_test_roundtrip.w3x";
}

void test_archive() {
    // Verify we can find our own temp path
    std::string test_path = temp_path();
    printf("  [debug] test_path = %s\n", test_path.c_str());

    // ── Step 1: Archive write test ──
    {
        Archive arc;
        if (!arc.open_write(test_path, "TestMap", 0, 1, 10)) {
            throw std::runtime_error("open_write failed");
        }

        // Write w3i content
        Buffer w3i_buf;
        {
            MapInfo info;
            info.version = 31;
            info.map_version = 1;
            info.map_name = "TestMap";
            info.author = "TestAuthor";
            info.description = "Roundtrip test";
            info.players_desc = "Players";
            info.camera_bounds = {-1, 1, -1, 1};
            info.camera_complements = {0, 0, 0, 0};
            info.map_width = 64;
            info.map_height = 64;
            info.map_flags = 0;
            info.ground_type = 0;
            info.loading_screen_id = 0;
            info.game_data_set = 0;
            info.script_type = 0;

            PlayerInfo p;
            p.id = 0; p.type = 0; p.race = 1;
            p.fixed_start = 1; p.name = "Player 1";
            p.start_x = 0; p.start_y = 0;
            p.ally_low = 1; p.ally_high = 0;
            info.players.push_back(p);

            ForceInfo f;
            f.flags = 1; f.player_mask = 1; f.name = "Force 1";
            info.forces.push_back(f);

            w3i::write(w3i_buf, info);
        }

        auto w3i_data = w3i_buf.take_data();
        if (!arc.write_file("war3map.w3i", w3i_data.data(), w3i_data.size())) {
            throw std::runtime_error("write_file(war3map.w3i) failed");
        }

        // Write a dummy text file
        std::string dummy = "Hello from w3x-packer!";
        if (!arc.write_file("dummy.txt", dummy.data(), dummy.size())) {
            throw std::runtime_error("write_file(dummy.txt) failed");
        }

        arc.close();
        printf("  [debug] wrote archive, size = %zu bytes\n", w3i_data.size());
    }

    // ── Step 2: Archive read test ──
    {
        Archive arc;
        if (!arc.open_read(test_path)) {
            throw std::runtime_error("open_read failed");
        }

        // Verify has_file
        if (!arc.has_file("war3map.w3i")) {
            throw std::runtime_error("has_file(war3map.w3i) returned false");
        }
        if (!arc.has_file("dummy.txt")) {
            throw std::runtime_error("has_file(dummy.txt) returned false");
        }

        // Read and verify w3i
        auto w3i_data = arc.read_file("war3map.w3i");
        if (w3i_data.empty()) {
            throw std::runtime_error("read_file(war3map.w3i) returned empty");
        }

        Buffer buf(w3i_data);
        auto info = w3i::read(buf);
        if (info.map_name != "TestMap") {
            throw std::runtime_error("map_name mismatch: " + info.map_name);
        }
        if (info.author != "TestAuthor") {
            throw std::runtime_error("author mismatch");
        }

        // Read and verify dummy.txt
        auto dummy_data = arc.read_file("dummy.txt");
        std::string dummy(dummy_data.begin(), dummy_data.end());
        if (dummy != "Hello from w3x-packer!") {
            throw std::runtime_error("dummy.txt content mismatch: " + dummy);
        }

        // List files
        auto files = arc.list_files();
        printf("  [debug] files in archive (%zu):\n", files.size());
        for (auto& f : files)
            printf("    - %s\n", f.c_str());

        arc.close();
    }

    // Cleanup
    std::remove(test_path.c_str());
    printf("  [debug] cleanup complete\n");
}
