#include <cstdio>
#include <cstring>
#include <iostream>
#include <string>
#include <vector>
#include <direct.h>

#include "core/map_builder.h"

static void print_usage() {
    fprintf(stderr,
        "w3x-packer v0.1 — Warcraft III .w3x map packer\n"
        "\n"
        "Usage:\n"
        "  w3x_packer info  <input.w3x>              — Print map info\n"
        "  w3x_packer list  <input.w3x>              — List files in archive\n"
        "  w3x_packer pack  <input.w3x> <output.w3x> — Re-pack (clean + validate)\n"
        "  w3x_packer extract <input.w3x> <dir>      — Extract all files\n"
        "  w3x_packer dump   <input.w3x> <file>     — Hex dump a file\n"
        "\n"
    );
}

static int cmd_info(MapBuilder& builder) {
    auto info = builder.read_w3i();
    printf("Map Name:      %s\n", info.map_name.c_str());
    printf("Author:        %s\n", info.author.c_str());
    printf("Description:   %s\n", info.description.c_str());
    printf("Map Version:   %d\n", info.map_version);
    printf("W3I Version:   %d\n", info.version);
    printf("Size:          %d x %d\n", info.map_width, info.map_height);
    printf("Flags:         0x%08X\n", info.map_flags);
    printf("Players:       %zu\n", info.players.size());
    printf("Forces:        %zu\n", info.forces.size());
    printf("Script Type:   %s\n", info.script_type == 0 ? "JASS" : "Lua");
    printf("Ground Type:   %d\n", info.ground_type);

    // Cameras
    auto cameras = builder.read_w3c();
    if (!cameras.empty()) {
        printf("Cameras:       %zu\n", cameras.size());
        for (auto& c : cameras)
            printf("  - %s (%.1f, %.1f, %.1f)\n",
                   c.name.c_str(), c.target_x, c.target_y, c.z_offset);
    }

    // Regions
    auto regions = builder.read_w3r();
    if (!regions.empty()) {
        printf("Regions:       %zu\n", regions.size());
        for (auto& r : regions)
            printf("  - %s (%.0f, %.0f)-(%.0f, %.0f)%s\n",
                   r.name.c_str(), r.left, r.bottom, r.right, r.top,
                   r.weather_id[0] ? (" weather=" + r.weather_id).c_str() : "");
    }

    return 0;
}

static int cmd_dump(MapBuilder& builder, const std::string& file_name) {
    auto data = builder.read_raw(file_name);
    printf("%s: %zu bytes\n", file_name.c_str(), data.size());
    for (size_t i = 0; i < data.size() && i < 128; ++i) {
        printf("%02X ", data[i]);
        if ((i + 1) % 16 == 0) printf("\n");
    }
    if (data.size() > 128) printf("\n... (%zu more bytes)\n", data.size() - 128);
    printf("\n");
    return 0;
}

static int cmd_list(MapBuilder& builder) {
    auto files = builder.list_files();
    for (auto& f : files)
        printf("%s\n", f.c_str());
    return 0;
}

static int cmd_pack(MapBuilder& builder, const std::string& output) {
    BuildSettings settings;
    settings.mode = OutputMode::OBJ;
    settings.remove_listfile = true;
    settings.remove_attributes = true;

    if (builder.save(output, settings)) {
        printf("Packed successfully: %s\n", output.c_str());
        return 0;
    } else {
        fprintf(stderr, "Error: pack failed\n");
        return 1;
    }
}

static int cmd_extract(MapBuilder& builder, const std::string& dir) {
    auto files = builder.list_files();
    for (auto& name : files) {
        auto data = builder.read_raw(name);

        // Build output path
        std::string out_path = dir + "/" + name;

        // Create subdirectories (Windows compatible)
        for (auto& ch : out_path)
            if (ch == '/') ch = '\\';

        auto slash = out_path.find_last_of('\\');
        if (slash != std::string::npos) {
            std::string subdir = out_path.substr(0, slash);
            // Create nested directories one at a time
            std::string cur;
            for (size_t i = 0; i < subdir.size(); ++i) {
                if (subdir[i] == '\\') {
                    _mkdir(cur.c_str());
                }
                cur += subdir[i];
            }
            _mkdir(cur.c_str());
        }

        FILE* f = fopen(out_path.c_str(), "wb");
        if (f) {
            fwrite(data.data(), 1, data.size(), f);
            fclose(f);
            printf("  %s (%zu bytes)\n", name.c_str(), data.size());
        } else {
            fprintf(stderr, "  Error writing: %s\n", out_path.c_str());
        }
    }
    printf("Extracted %zu files to %s\n", files.size(), dir.c_str());
    return 0;
}

int main(int argc, char* argv[]) {
    setbuf(stdout, NULL);
    setbuf(stderr, NULL);

    if (argc < 2) {
        print_usage();
        return 1;
    }

    std::string cmd = argv[1];

    if (cmd == "info" || cmd == "list" || cmd == "pack" || cmd == "extract" || cmd == "dump") {
        if ((cmd == "pack" && argc < 4) || (cmd == "extract" && argc < 4) || (cmd == "dump" && argc < 4) || argc < 3) {
            print_usage();
            return 1;
        }

        std::string input = argv[2];

        try {
            MapBuilder builder;
            if (!builder.open_source(input)) {
                fprintf(stderr, "Error: cannot open %s\n", input.c_str());
                return 1;
            }

            if (cmd == "info")
                return cmd_info(builder);
            else if (cmd == "list")
                return cmd_list(builder);
            else if (cmd == "dump")
                return cmd_dump(builder, argv[3]);
            else if (cmd == "pack")
                return cmd_pack(builder, argv[3]);
            else if (cmd == "extract")
                return cmd_extract(builder, argv[3]);
        } catch (const std::exception& e) {
            fprintf(stderr, "Error: %s\n", e.what());
            return 1;
        }
    }

    print_usage();
    return 1;
}
