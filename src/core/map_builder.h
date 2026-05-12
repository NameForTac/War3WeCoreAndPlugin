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
#include "wpm.h"
#include "shd.h"
#include "w3s.h"
#include "wct.h"
#include "wtg.h"
#include "ini.h"
#include "wai.h"
#include "w3f.h"

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

    PathingMap          read_wpm();
    ShadowMap           read_shd(int32_t width, int32_t height);
    Sounds              read_w3s();
    CustomTextTriggers  read_wct();
    TriggerData         read_wtg();
    std::string         read_jass();
    INIFile             read_ini(const std::string& file_name);
    AIFile              read_wai();
    CampaignInfo        read_w3f();

    // --- Modify ---
    void set_w3i(const MapInfo& info);
    void set_terrain(const Terrain& terrain);
    void set_placed_units(const PlacedUnits& units);
    void set_placed_doodads(const PlacedDoodads& doodads);
    void set_object(const std::string& file_name, const ObjectFile& obj);
    void set_wts(const WTS& table);
    void set_wpm(const PathingMap& pm);
    void set_shd(const ShadowMap& sm);
    void set_w3s(const Sounds& sounds);
    void set_wct(const CustomTextTriggers& ctt);
    void set_wtg(const TriggerData& td);
    void set_jass(const std::string& script);
    void set_ini(const std::string& file_name, const INIFile& ini);
    void set_wai(const AIFile& ai);
    void set_w3f(const CampaignInfo& ci);
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
