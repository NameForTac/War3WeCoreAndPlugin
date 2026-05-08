#pragma once

#include "w3i.h"
#include "w3u.h"
#include "w3c.h"
#include "w3r.h"
#include "w3e.h"
#include "doo.h"
#include "units_doo.h"
#include "imp.h"
#include "wts.h"
#include "slk.h"

#include <functional>
#include <memory>
#include <string>

class Wc3Manager;

// Progress callback: status message, current step, total steps
using ProgressCallback = std::function<void(const std::string&, int, int)>;

// ============================================================
// MapBuilder — orchestrator for map I/O
// ============================================================
enum class OutputMode {
    OBJ,    // binary object files (w3a/w3u/...)
    SLK,    // SLK spreadsheets + TXT strings
    LNI,    // pure text format
};

struct BuildSettings {
    OutputMode mode = OutputMode::OBJ;
    bool remove_listfile = true;
    bool remove_attributes = true;
};

class MapBuilder {
public:
    MapBuilder();
    ~MapBuilder();

    // --- Source ---
    bool open_source(const std::string& path);

    // --- WC3 resource manager ---
    void set_wc3_manager(Wc3Manager* mgr) { wc3_mgr_ = mgr; }

    // Read a file with map-first then WC3 fallback.
    std::vector<uint8_t>    read_resource(const std::string& name);

    // --- Read ---
    MapInfo                 read_w3i();
    ObjectFile              read_object(const std::string& file_name);
    std::vector<Camera>     read_w3c();
    std::vector<Region>     read_w3r();
    Terrain                 read_terrain();
    PlacedUnits             read_placed_units();
    PlacedDoodads           read_placed_doodads();
    std::vector<ImportedFile> read_imp();
    WTS                     read_wts();
    SLKTable                read_slk(const std::string& file_name);
    std::vector<uint8_t>    read_raw(const std::string& file_name);
    int64_t                 file_size(const std::string& file_name);
    std::vector<std::string> list_files();

    // --- Modify ---
    void set_w3i(const MapInfo& info);
    void set_terrain(const Terrain& terrain);
    void set_placed_units(const PlacedUnits& units);
    void set_placed_doodads(const PlacedDoodads& doodads);
    void set_object(const std::string& file_name, const ObjectFile& obj);
    void set_wts(const WTS& table);
    void set_file(const std::string& name, std::vector<uint8_t> data);
    void remove_file(const std::string& name);

    // --- Build ---
    bool save(const std::string& output_path, const BuildSettings& settings = {});

    // --- Progress ---
    void set_progress_callback(ProgressCallback cb);

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
    ProgressCallback progress_;
    Wc3Manager* wc3_mgr_ = nullptr;
};
