#include "map_builder.h"
#include "archive.h"
#include "wc3_manager.h"
#include "w3c.h"
#include "w3r.h"
#include "w3e.h"
#include "doo.h"
#include "units_doo.h"
#include "imp.h"
#include <map>
#include <unordered_map>
#include <optional>
#include <unordered_set>
#include <sstream>
#include <fstream>

// ============================================================
// Internal implementation
// ============================================================
struct MapBuilder::Impl {
    std::unique_ptr<Archive> source;
    std::string source_path;

    // Cached / modified data
    std::optional<MapInfo>     w3i_cache;
    std::map<std::string, ObjectFile> object_cache;
    std::optional<Terrain>      terrain_cache;
    std::optional<PlacedUnits>  units_cache;
    std::optional<PlacedDoodads> doodads_cache;
    std::optional<WTS>          wts_cache;

    // File overrides (name → data)
    std::map<std::string, std::vector<uint8_t>> file_overrides;
    std::unordered_set<std::string> file_removals;

    bool source_is_open = false;

    // Map file extension → ObjectFileType
    static ObjectFileType file_type_for(const std::string& name) {
        static const std::unordered_map<std::string, ObjectFileType> map = {
            {"war3map.w3u", ObjectFileType::Units},
            {"war3map.w3t", ObjectFileType::Items},
            {"war3map.w3b", ObjectFileType::Destructables},
            {"war3map.w3d", ObjectFileType::Doodads},
            {"war3map.w3a", ObjectFileType::Abilities},
            {"war3map.w3h", ObjectFileType::Buffs},
            {"war3map.w3q", ObjectFileType::Upgrades},
        };
        auto it = map.find(name);
        return (it != map.end()) ? it->second : ObjectFileType::Units;
    }

    // Standard map files (not imported resources)
    static const std::unordered_set<std::string>& standard_files() {
        static std::unordered_set<std::string> files = {
            "war3map.w3i", "war3map.w3e", "war3map.wpm",
            "war3mapunits.doo", "war3map.doo",
            "war3map.shd", "war3map.w3c", "war3map.w3r", "war3map.w3s",
            "war3map.mmp", "war3mapmap.blp", "war3map.wts",
            "war3map.wtg", "war3map.wct", "war3map.j", "scripts/war3map.j",
            "war3map.imp", "war3mapextra.txt",
            "war3map.w3a", "war3map.w3u", "war3map.w3t", "war3map.w3h",
            "war3map.w3b", "war3map.w3d", "war3map.w3q",
            "war3mapmisc.txt", "war3mapskin.txt",
            "(listfile)", "(attributes)", "(signature)",
        };
        return files;
    }
};

MapBuilder::MapBuilder() : impl_(std::make_unique<Impl>()) {}
MapBuilder::~MapBuilder() = default;

void MapBuilder::set_progress_callback(ProgressCallback cb) {
    progress_ = std::move(cb);
}

// --- Source ---

bool MapBuilder::open_source(const std::string& path) {
    auto arc = std::make_unique<Archive>();
    if (!arc->open_read(path))
        return false;
    impl_->source = std::move(arc);
    impl_->source_path = path;
    impl_->source_is_open = true;
    impl_->w3i_cache.reset();
    impl_->object_cache.clear();
    impl_->terrain_cache.reset();
    impl_->units_cache.reset();
    impl_->doodads_cache.reset();
    impl_->wts_cache.reset();
    impl_->file_overrides.clear();
    impl_->file_removals.clear();
    return true;
}

// --- Read ---

MapInfo MapBuilder::read_w3i() {
    if (impl_->w3i_cache)
        return *impl_->w3i_cache;

    auto data = impl_->source->read_file("war3map.w3i");
    Buffer buf(std::move(data));
    auto info = w3i::read(buf);
    impl_->w3i_cache = info;
    return info;
}

ObjectFile MapBuilder::read_object(const std::string& file_name) {
    auto it = impl_->object_cache.find(file_name);
    if (it != impl_->object_cache.end())
        return it->second;

    auto data = impl_->source->read_file(file_name);
    Buffer buf(std::move(data));
    auto type = Impl::file_type_for(file_name);
    auto obj = w3u::read(buf, type);
    impl_->object_cache[file_name] = obj;
    return obj;
}

std::vector<Camera> MapBuilder::read_w3c() {
    auto data = impl_->source->read_file("war3map.w3c");
    if (data.empty()) return {};
    Buffer buf(std::move(data));
    return w3c::read(buf);
}

std::vector<Region> MapBuilder::read_w3r() {
    auto data = impl_->source->read_file("war3map.w3r");
    if (data.empty()) return {};
    Buffer buf(std::move(data));
    return w3r::read(buf);
}

Terrain MapBuilder::read_terrain() {
    if (impl_->terrain_cache)
        return *impl_->terrain_cache;

    auto data = impl_->source->read_file("war3map.w3e");
    Buffer buf(std::move(data));
    auto t = w3e::read(buf);
    impl_->terrain_cache = t;
    return t;
}

void MapBuilder::set_terrain(const Terrain& terrain) {
    impl_->terrain_cache = terrain;
}

PlacedUnits MapBuilder::read_placed_units() {
    if (impl_->units_cache)
        return *impl_->units_cache;

    auto data = impl_->source->read_file("war3mapunits.doo");
    Buffer buf(std::move(data));
    auto u = units_doo::read(buf);
    impl_->units_cache = u;
    return u;
}

void MapBuilder::set_placed_units(const PlacedUnits& units) {
    impl_->units_cache = units;
}

PlacedDoodads MapBuilder::read_placed_doodads() {
    if (impl_->doodads_cache)
        return *impl_->doodads_cache;

    auto data = impl_->source->read_file("war3map.doo");
    Buffer buf(std::move(data));
    auto d = doo::read(buf);
    impl_->doodads_cache = d;
    return d;
}

void MapBuilder::set_placed_doodads(const PlacedDoodads& doodads) {
    impl_->doodads_cache = doodads;
}

std::vector<ImportedFile> MapBuilder::read_imp() {
    auto data = impl_->source->read_file("war3map.imp");
    if (data.empty()) return {};
    Buffer buf(std::move(data));
    return imp::read(buf);
}

WTS MapBuilder::read_wts() {
    if (impl_->wts_cache)
        return *impl_->wts_cache;

    auto data = impl_->source->read_file("war3map.wts");
    std::string text(data.begin(), data.end());
    auto table = wts::parse(text);
    impl_->wts_cache = table;
    return table;
}

SLKTable MapBuilder::read_slk(const std::string& file_name) {
    auto data = impl_->source->read_file(file_name);
    std::string text(data.begin(), data.end());
    return slk::parse(text);
}

std::vector<uint8_t> MapBuilder::read_raw(const std::string& file_name) {
    return impl_->source->read_file(file_name);
}

std::vector<uint8_t> MapBuilder::read_resource(const std::string& name) {
    // 1) Map archive first
    if (impl_->source && impl_->source->has_file(name))
        return impl_->source->read_file(name);
    // 2) WC3 fallback (War3x.mpq → war3.mpq)
    if (wc3_mgr_)
        return wc3_mgr_->read_file(name);
    return {};
}

int64_t MapBuilder::file_size(const std::string& file_name) {
    return impl_->source->file_size(file_name);
}

std::vector<std::string> MapBuilder::list_files() {
    return impl_->source->list_all_files();
}

// --- Modify ---

void MapBuilder::set_w3i(const MapInfo& info) {
    impl_->w3i_cache = info;
}

void MapBuilder::set_object(const std::string& file_name, const ObjectFile& obj) {
    impl_->object_cache[file_name] = obj;
}

void MapBuilder::set_wts(const WTS& table) {
    impl_->wts_cache = table;
}

void MapBuilder::set_file(const std::string& name, std::vector<uint8_t> data) {
    impl_->file_overrides[name] = std::move(data);
    impl_->file_removals.erase(name);
}

void MapBuilder::remove_file(const std::string& name) {
    impl_->file_removals.insert(name);
    impl_->file_overrides.erase(name);
}

// --- Build ---

bool MapBuilder::save(const std::string& output_path, const BuildSettings& settings) {
    if (!impl_->source_is_open)
        return false;

    // Read the W3I first to get map metadata for HM3W header
    // (populates w3i_cache so we can close the source archive later)
    auto info = read_w3i();
    bool same_file = (output_path == impl_->source_path);

    // Pre-read all source files (uses hash table enumeration, works
    // even when the source MPQ has no (listfile))
    auto all_files = impl_->source->list_all_files();
    struct SrcFile { std::string name; std::vector<uint8_t> data; };
    std::vector<SrcFile> src_files;

    // Count total steps for progress reporting
    int total_steps = (int)all_files.size();  // pre-read: one per source file
    total_steps += 1;                          // write w3i
    total_steps += (int)impl_->object_cache.size();
    if (impl_->terrain_cache) total_steps++;
    if (impl_->units_cache) total_steps++;
    if (impl_->doodads_cache) total_steps++;
    if (impl_->wts_cache) total_steps++;
    if (!impl_->file_overrides.empty()) total_steps++;
    total_steps += 1;  // write remaining source files
    total_steps += 1;  // close archive
    if (same_file) total_steps++;  // reopen
    int step = 0;

    for (auto& name : all_files) {
        if (progress_) progress_("Reading: " + name, ++step, total_steps);
        if (impl_->file_removals.find(name) != impl_->file_removals.end())
            continue;
        if (name == "war3map.w3i") continue;
        if (name == "war3map.w3e" && impl_->terrain_cache) continue;
        if (name == "war3mapunits.doo" && impl_->units_cache) continue;
        if (name == "war3map.doo" && impl_->doodads_cache) continue;
        if (impl_->object_cache.count(name)) continue;
        if (name == "war3map.wts" && impl_->wts_cache) continue;
        if (impl_->file_overrides.count(name)) continue;
        if (settings.remove_listfile && name == "(listfile)") continue;
        if (settings.remove_attributes && name == "(attributes)") continue;
        if (name == "(signature)") continue;

        auto data = impl_->source->read_file(name);
        if (!data.empty())
            src_files.push_back({name, std::move(data)});
    }

    // Close source archive before writing (avoid sharing violation)
    if (same_file) {
        impl_->source.reset();
        impl_->source_is_open = false;
    }

    // Count files for StormLib header
    int file_count = (int)src_files.size();
    for (auto& [name, _] : impl_->file_overrides)
        if (std::find(all_files.begin(), all_files.end(), name) == all_files.end())
            file_count++;
    if (impl_->w3i_cache) file_count++;
    if (impl_->terrain_cache) file_count++;
    if (impl_->units_cache) file_count++;
    if (impl_->doodads_cache) file_count++;
    if (impl_->wts_cache) file_count++;
    for (auto& [name, _] : impl_->object_cache)
        file_count++;

    // Create output archive
    Archive archive;
    if (!archive.open_write(output_path, info.map_name, info.map_flags,
                            (int32_t)info.players.size(), file_count))
        return false;

    // Helper: internal Warcraft III map files need MPQ encryption.
    // All files prefixed "war3map" and scripts/ are internal map files
    // that Warcraft III expects to be MPQ-encrypted.
    auto needs_encrypt = [](const std::string& name) -> bool {
        return name.compare(0, 7, "war3map") == 0
            || name.compare(0, 8, "scripts/") == 0;
    };

    // Helper to write a file to the archive
    auto write_file = [&](const std::string& name, const std::vector<uint8_t>& data) {
        archive.write_file(name, data.data(), data.size(), needs_encrypt(name));
    };

    // Write W3I
    {
        if (progress_) progress_("Writing map info (w3i)", ++step, total_steps);
        auto map_info = read_w3i(); // uses cache
        Buffer buf;
        w3i::write(buf, map_info);
        write_file("war3map.w3i", buf.take_data());
    }

    // Write object files
    for (auto& [name, obj] : impl_->object_cache) {
        if (progress_) progress_("Writing: " + name, ++step, total_steps);
        Buffer buf;
        w3u::write(buf, obj);
        write_file(name, buf.take_data());
    }

    // Write terrain
    if (impl_->terrain_cache) {
        if (progress_) progress_("Writing terrain (w3e)", ++step, total_steps);
        Buffer buf;
        w3e::write(buf, *impl_->terrain_cache);
        write_file("war3map.w3e", buf.take_data());
    }

    // Write placed units
    if (impl_->units_cache) {
        if (progress_) progress_("Writing placed units", ++step, total_steps);
        Buffer buf;
        units_doo::write(buf, *impl_->units_cache);
        write_file("war3mapunits.doo", buf.take_data());
    }

    // Write placed doodads
    if (impl_->doodads_cache) {
        if (progress_) progress_("Writing placed doodads", ++step, total_steps);
        Buffer buf;
        doo::write(buf, *impl_->doodads_cache);
        write_file("war3map.doo", buf.take_data());
    }

    // Write WTS
    if (impl_->wts_cache) {
        if (progress_) progress_("Writing trigger strings (wts)", ++step, total_steps);
        auto text = wts::generate(*impl_->wts_cache);
        std::vector<uint8_t> data(text.begin(), text.end());
        write_file("war3map.wts", data);
    }

    // Write file overrides
    if (!impl_->file_overrides.empty()) {
        if (progress_) progress_("Writing file overrides", ++step, total_steps);
    }
    for (auto& [name, data] : impl_->file_overrides) {
        write_file(name, data);
    }

    // Write pre-read source files (with name mapping for unnamed files)
    if (progress_) progress_("Writing remaining source files", ++step, total_steps);
    for (auto& sf : src_files) {
        // Check if this is a StormLib pseudo-name (File000000NN.ext)
        auto filename = sf.name;
        if (filename.size() > 12 &&
            filename[0] == 'F' && filename[1] == 'i' && filename[2] == 'l' && filename[3] == 'e' &&
            std::all_of(filename.begin() + 4, filename.begin() + 12, ::isdigit))
        {
            // Map extension to standard War3 file name
            auto dot = filename.find_last_of('.');
            std::string ext = (dot != std::string::npos) ? filename.substr(dot) : "";
            static const std::unordered_map<std::string, std::string> ext_map = {
                {".w3e", "war3map.w3e"},
                {".wpm", "war3map.wpm"},
                {".shd", "war3map.shd"},
                {".doo", "war3mapunits.doo"},
                {".w3c", "war3map.w3c"},
                {".w3r", "war3map.w3r"},
                {".w3s", "war3map.w3s"},
                {".mmp", "war3map.mmp"},
                {".wtg", "war3map.wtg"},
                {".wct", "war3map.wct"},
                {".w3a", "war3map.w3a"},
                {".w3u", "war3map.w3u"},
                {".w3t", "war3map.w3t"},
                {".w3h", "war3map.w3h"},
                {".w3b", "war3map.w3b"},
                {".w3d", "war3map.w3d"},
                {".w3q", "war3map.w3q"},
                {".wts", "war3map.wts"},
                {".j",   "war3map.j"},
            };
            auto it = ext_map.find(ext);
            if (it != ext_map.end())
                filename = it->second;
            else
                continue; // skip unknown files without proper names
        }
        // Skip already-handled files (written from cache above)
        if (filename == "war3map.w3i")
            continue;
        if (filename == "war3map.w3e" && impl_->terrain_cache)
            continue;
        if (filename == "war3mapunits.doo" && impl_->units_cache)
            continue;
        if (filename == "war3map.doo" && impl_->doodads_cache)
            continue;
        write_file(filename, sf.data);
    }

    // Close archive (finalizes MPQ)
    if (progress_) progress_("Finalizing archive", ++step, total_steps);
    archive.close();

    // Reopen if we closed the source
    if (same_file) {
        if (progress_) progress_("Reopening archive", ++step, total_steps);
        auto arc = std::make_unique<Archive>();
        if (arc->open_read(output_path))
            impl_->source = std::move(arc);

        // Keep source_is_open accurate
        impl_->source_is_open = (impl_->source != nullptr);
    }

    return true;
}
