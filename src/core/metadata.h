#pragma once

#include "types.h"
#include <map>
#include <string>
#include <vector>

// ============================================================
// MetaField — describes a single modifiable field in the
// object editor (loaded from *MetaData.slk)
// ============================================================
struct MetaField {
    std::string id;            // 4-char field code (e.g. "uaap")
    std::string display_name;  // editor-friendly name (e.g. "Armor")
    ValueType type = ValueType::Int;
    double min_val = 0;
    double max_val = 999999.0;
    std::string default_val;
};

// ============================================================
// MetaDataDB — loads WC3 SLK metadata/data files and provides
// field metadata + known object lists for autocomplete.
// Also resolves object display names from *strings.txt
// and WESTRING_* keys from WorldEditStrings.txt.
// ============================================================
class MetaDataDB {
public:
    bool load_data_dir(const std::string& wc3_path);

    // Per-type field metadata
    const std::vector<MetaField>& get_fields(ObjectFileType type) const;
    const MetaField* find_field(ObjectFileType type, const std::string& field_id) const;

    // Known objects (from *Data.slk) for "create custom" UI
    const std::vector<std::pair<std::string, std::string>>& get_known_objects(ObjectFileType type) const;

    // Object display name resolution (from *strings.txt: Name= field)
    const std::string* find_object_name(const std::string& object_id) const;
    std::string resolve_object_name(const std::string& object_id) const;

    bool is_loaded() const { return loaded_; }

    // Map from file type to SLK file names
    static const char* meta_slk_for(ObjectFileType type);
    static const char* data_slk_for(ObjectFileType type);

private:
    bool loaded_ = false;
    std::map<ObjectFileType, std::vector<MetaField>> fields_;
    std::map<ObjectFileType, std::vector<std::pair<std::string, std::string>>> known_objects_;

    // objectId → display name  (from *strings.txt)
    std::map<std::string, std::string> object_names_;
    // WESTRING_XXX → resolved display string  (from WorldEditStrings.txt)
    std::map<std::string, std::string> westring_map_;

    void load_fields_from_slk(const std::string& path, ObjectFileType type);
    void load_objects_from_slk(const std::string& path, ObjectFileType type);

    void load_westring(const std::string& dir);
    void load_object_name_file(const std::string& path);
};
