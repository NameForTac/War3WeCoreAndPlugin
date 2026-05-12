#include "metadata.h"
#include "slk.h"

#include <algorithm>
#include <fstream>
#include <sstream>

// ============================================================
// SLK file name mapping
// ============================================================
const char* MetaDataDB::meta_slk_for(ObjectFileType type) {
    switch (type) {
        case ObjectFileType::Units:         return "Units/UnitMetaData.slk";
        case ObjectFileType::Items:         return "Units/ItemMetaData.slk";
        case ObjectFileType::Abilities:     return "Units/AbilityMetaData.slk";
        case ObjectFileType::Buffs:         return "Units/BuffMetaData.slk";
        case ObjectFileType::Upgrades:      return "Units/UpgradeMetaData.slk";
        case ObjectFileType::Destructables: return "Units/DestructableData.slk";
        case ObjectFileType::Doodads:       return "Units/DoodadData.slk";
    }
    return nullptr;
}

const char* MetaDataDB::data_slk_for(ObjectFileType type) {
    switch (type) {
        case ObjectFileType::Units:         return "Units/UnitData.slk";
        case ObjectFileType::Items:         return "Units/ItemData.slk";
        case ObjectFileType::Abilities:     return "Units/AbilityData.slk";
        case ObjectFileType::Buffs:         return "Units/BuffData.slk";
        case ObjectFileType::Upgrades:      return "Units/UpgradeData.slk";
        case ObjectFileType::Destructables: return "Units/DestructableData.slk";
        case ObjectFileType::Doodads:       return "Units/DoodadData.slk";
    }
    return nullptr;
}

// ============================================================
// Load all data from a WC3 installation directory
// ============================================================
bool MetaDataDB::load_data_dir(const std::string& wc3_path) {
    loaded_ = false;
    fields_.clear();
    known_objects_.clear();
    object_names_.clear();
    westring_map_.clear();

    // Normalize path separator
    std::string dir = wc3_path;
    if (!dir.empty() && dir.back() != '/' && dir.back() != '\\')
        dir += '/';
    for (auto& c : dir) if (c == '\\') c = '/';

    // 1. Load WESTRING key resolution table
    load_westring(dir);

    // 2. Load object display names from *strings.txt
    static const char* name_files[] = {
        "Units/humanunitstrings.txt",
        "Units/orcunitstrings.txt",
        "Units/undeadunitstrings.txt",
        "Units/nightelfunitstrings.txt",
        "Units/neutralunitstrings.txt",
        "Units/campaignunitstrings.txt",
        "Units/commonabilitystrings.txt",
        "Units/humanabilitystrings.txt",
        "Units/orcabilitystrings.txt",
        "Units/undeadabilitystrings.txt",
        "Units/nightelfabilitystrings.txt",
        "Units/neutralabilitystrings.txt",
        "Units/campaignabilitystrings.txt",
        "Units/itemabilitystrings.txt",
        "Units/itemstrings.txt",
        "Units/humanupgradestrings.txt",
        "Units/orcupgradestrings.txt",
        "Units/undeadupgradestrings.txt",
        "Units/nightelfupgradestrings.txt",
        "Units/neutralupgradestrings.txt",
        "Units/campaignupgradestrings.txt",
        // Custom_V1 overrides (loaded after standard files)
        "Custom_V1/Units/humanunitstrings.txt",
        "Custom_V1/Units/orcunitstrings.txt",
        "Custom_V1/Units/undeadunitstrings.txt",
        "Custom_V1/Units/nightelfunitstrings.txt",
        "Custom_V1/Units/neutralunitstrings.txt",
        "Custom_V1/Units/campaignunitstrings.txt",
        "Custom_V1/Units/humanupgradestrings.txt",
        "Custom_V1/Units/orcupgradestrings.txt",
        "Custom_V1/Units/undeadupgradestrings.txt",
        "Custom_V1/Units/nightelfupgradestrings.txt",
        "Custom_V1/Units/neutralupgradestrings.txt",
        "Custom_V1/Units/campaignupgradestrings.txt",
        "Custom_V1/Units/itemstrings.txt",
    };
    for (auto* name : name_files)
        load_object_name_file(dir + name);

    // 3. Load field metadata from SLK
    static const ObjectFileType all_types[] = {
        ObjectFileType::Units,
        ObjectFileType::Items,
        ObjectFileType::Abilities,
        ObjectFileType::Buffs,
        ObjectFileType::Upgrades,
        ObjectFileType::Destructables,
        ObjectFileType::Doodads,
    };

    for (auto type : all_types) {
        auto* meta_name = meta_slk_for(type);
        if (meta_name) {
            std::string meta_path = dir + meta_name;
            load_fields_from_slk(meta_path, type);
        }

        auto* data_name = data_slk_for(type);
        if (data_name) {
            std::string data_path = dir + data_name;
            load_objects_from_slk(data_path, type);
        }
    }

    loaded_ = true;
    return true;
}

// ============================================================
// Load WESTRING key → display name resolution table
// ============================================================
void MetaDataDB::load_westring(const std::string& dir) {
    std::string path = dir + "UI/WorldEditStrings.txt";
    std::ifstream ifs(path);
    if (!ifs.is_open())
        return;

    std::string line;
    // Format: KEY=VALUE  (with [WorldEditStrings] section header)
    // Lines starting with '[' are section headers (skip)
    // Lines starting with '//' are comments (skip)
    while (std::getline(ifs, line)) {
        // Trim whitespace
        auto notspace = [](char c) { return c > ' '; };
        auto start = std::find_if(line.begin(), line.end(), notspace);
        auto end = std::find_if(line.rbegin(), line.rend(), notspace).base();
        line = (start < end) ? std::string(start, end) : "";

        if (line.empty() || line[0] == '[' || line.compare(0, 2, "//") == 0)
            continue;

        auto eq = line.find('=');
        if (eq == std::string::npos)
            continue;

        std::string key = line.substr(0, eq);
        std::string val = line.substr(eq + 1);

        // Strip surrounding quotes if present
        if (val.size() >= 2 && val[0] == '"' && val.back() == '"')
            val = val.substr(1, val.size() - 2);

        westring_map_[key] = val;
    }
}

// ============================================================
// Load object display names from a *strings.txt file
// Format: INI-like with [objectId] sections containing Name= value
// ============================================================
void MetaDataDB::load_object_name_file(const std::string& path) {
    std::ifstream ifs(path);
    if (!ifs.is_open())
        return;

    std::string line;
    std::string current_id;

    while (std::getline(ifs, line)) {
        // Trim whitespace
        auto notspace = [](char c) { return c > ' '; };
        auto start = std::find_if(line.begin(), line.end(), notspace);
        auto end = std::find_if(line.rbegin(), line.rend(), notspace).base();
        line = (start < end) ? std::string(start, end) : "";

        if (line.empty() || line.compare(0, 2, "//") == 0)
            continue;

        // Section header: [objectId]
        if (line[0] == '[') {
            auto close = line.find(']');
            if (close != std::string::npos)
                current_id = line.substr(1, close - 1);
            else
                current_id.clear();
            continue;
        }

        if (current_id.empty())
            continue;

        // Name=DisplayName
        if (line.compare(0, 5, "Name=") == 0 || line.compare(0, 5, "name=") == 0) {
            std::string name = line.substr(5);
            // Strip surrounding quotes
            if (name.size() >= 2 && name[0] == '"' && name.back() == '"')
                name = name.substr(1, name.size() - 2);
            if (!name.empty())
                object_names_[current_id] = name;
        }
    }
}

// ============================================================
// Load field metadata from a *MetaData.slk file
// ============================================================
void MetaDataDB::load_fields_from_slk(const std::string& path, ObjectFileType type) {
    std::ifstream ifs(path, std::ios::binary);
    if (!ifs.is_open())
        return;

    std::stringstream ss;
    ss << ifs.rdbuf();
    std::string text = ss.str();

    SLKTable table = slk::parse(text);
    if (table.empty())
        return;

    auto col_idx = [&](const std::string& name) -> int {
        for (size_t i = 0; i < table.headers.size(); ++i) {
            std::string h = table.headers[i];
            std::transform(h.begin(), h.end(), h.begin(), ::tolower);
            std::string n = name;
            std::transform(n.begin(), n.end(), n.begin(), ::tolower);
            if (h == n) return (int)i;
        }
        return -1;
    };

    int col_id      = col_idx("id");
    int col_field   = col_idx("field");
    int col_type    = col_idx("type");
    int col_min     = col_idx("minval");
    int col_max     = col_idx("maxval");
    int col_default = col_idx("defaultval");
    [[maybe_unused]] int col_display = col_idx("displayname");

    if (col_id < 0)
        return;

    auto& out = fields_[type];
    for (const auto& row : table.rows) {
        if ((size_t)col_id >= row.size())
            continue;
        std::string id = row[col_id];
        if (id.empty() || id == "XXXX")
            continue;

        MetaField mf;
        mf.id = id;

        if (col_field >= 0 && (size_t)col_field < row.size())
            mf.display_name = row[col_field];

        // Resolve display name via WESTRING map if it looks like a key
        if (!mf.display_name.empty() && mf.display_name.compare(0, 8, "WESTRING") == 0) {
            auto wit = westring_map_.find(mf.display_name);
            if (wit != westring_map_.end())
                mf.display_name = wit->second;
        }

        if (col_type >= 0 && (size_t)col_type < row.size()) {
            int t = 0;
            try { t = std::stoi(row[col_type]); } catch (...) {}
            if (t >= 0 && t <= 5)
                mf.type = static_cast<ValueType>(t);
        }

        if (col_min >= 0 && (size_t)col_min < row.size()) {
            try { mf.min_val = std::stod(row[col_min]); } catch (...) {}
        }
        if (col_max >= 0 && (size_t)col_max < row.size()) {
            try { mf.max_val = std::stod(row[col_max]); } catch (...) {}
        }
        if (col_default >= 0 && (size_t)col_default < row.size())
            mf.default_val = row[col_default];

        out.push_back(std::move(mf));
    }
}

// ============================================================
// Load known objects from a *Data.slk file
// ============================================================
void MetaDataDB::load_objects_from_slk(const std::string& path, ObjectFileType type) {
    std::ifstream ifs(path, std::ios::binary);
    if (!ifs.is_open())
        return;

    std::stringstream ss;
    ss << ifs.rdbuf();
    std::string text = ss.str();

    SLKTable table = slk::parse(text);
    if (table.empty())
        return;

    auto col_idx = [&](const std::string& name) -> int {
        for (size_t i = 0; i < table.headers.size(); ++i) {
            std::string h = table.headers[i];
            std::transform(h.begin(), h.end(), h.begin(), ::tolower);
            std::string n = name;
            std::transform(n.begin(), n.end(), n.begin(), ::tolower);
            if (h == n) return (int)i;
        }
        return -1;
    };

    int col_id   = col_idx("id");
    int col_name = col_idx("name");

    if (col_id < 0)
        return;

    auto& out = known_objects_[type];
    for (const auto& row : table.rows) {
        if ((size_t)col_id >= row.size())
            continue;
        std::string id = row[col_id];
        if (id.empty() || id == "XXXX")
            continue;

        std::string name;
        if (col_name >= 0 && (size_t)col_name < row.size())
            name = row[col_name];

        out.emplace_back(id, name);
    }
}

// ============================================================
// Object name resolution
// ============================================================
const std::string* MetaDataDB::find_object_name(const std::string& object_id) const {
    if (object_id.empty())
        return nullptr;
    auto it = object_names_.find(object_id);
    return it != object_names_.end() ? &it->second : nullptr;
}

std::string MetaDataDB::resolve_object_name(const std::string& object_id) const {
    auto* name = find_object_name(object_id);
    if (name)
        return *name;
    return object_id;
}

// ============================================================
// Accessors
// ============================================================
const std::vector<MetaField>& MetaDataDB::get_fields(ObjectFileType type) const {
    static const std::vector<MetaField> empty;
    auto it = fields_.find(type);
    return it != fields_.end() ? it->second : empty;
}

const MetaField* MetaDataDB::find_field(ObjectFileType type, const std::string& field_id) const {
    auto it = fields_.find(type);
    if (it == fields_.end())
        return nullptr;
    for (const auto& mf : it->second) {
        if (mf.id == field_id)
            return &mf;
    }
    return nullptr;
}

const std::vector<std::pair<std::string, std::string>>&
MetaDataDB::get_known_objects(ObjectFileType type) const {
    static const std::vector<std::pair<std::string, std::string>> empty;
    auto it = known_objects_.find(type);
    return it != known_objects_.end() ? it->second : empty;
}
