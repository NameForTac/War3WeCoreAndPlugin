#pragma once

#include <string>
#include <map>
#include <vector>

// ============================================================
// Simple INI-style file parser
// Used for: war3mapMisc.txt, war3mapSkin.txt, war3mapExtra.txt
// Format: [section]\nkey=value\n  (semicolon/hash comments)
// ============================================================
struct INIFile {
    // section → key → value
    std::map<std::string, std::map<std::string, std::string>> sections;

    // Get a value from a section (returns empty if not found)
    std::string get(const std::string& section, const std::string& key) const;

    // Set a value in a section
    void set(const std::string& section, const std::string& key, const std::string& value);

    // Check if key exists
    bool has(const std::string& section, const std::string& key) const;

    // List keys in a section (empty vector if section not found)
    std::vector<std::string> keys(const std::string& section) const;

    // List all section names
    std::vector<std::string> section_names() const;

    bool empty() const { return sections.empty(); }
    void clear() { sections.clear(); }
};

namespace ini {

INIFile parse(const std::string& text);
std::string generate(const INIFile& ini);

} // namespace ini
