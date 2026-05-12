#include "ini.h"
#include <sstream>
#include <algorithm>

namespace ini {

INIFile parse(const std::string& text) {
    INIFile ini;
    std::string current_section;
    std::istringstream stream(text);
    std::string line;

    while (std::getline(stream, line)) {
        // Trim whitespace
        auto trim_start = line.find_first_not_of(" \t\r\n");
        if (trim_start == std::string::npos)
            continue;
        auto trim_end = line.find_last_not_of(" \t\r\n");
        std::string trimmed = line.substr(trim_start, trim_end - trim_start + 1);

        // Skip comments
        if (trimmed.empty() || trimmed[0] == ';' || trimmed[0] == '#')
            continue;

        // Section header
        if (trimmed[0] == '[') {
            auto close = trimmed.find(']');
            if (close != std::string::npos)
                current_section = trimmed.substr(1, close - 1);
            continue;
        }

        // Key=value
        auto eq = trimmed.find('=');
        if (eq != std::string::npos) {
            std::string key = trimmed.substr(0, eq);
            std::string value = trimmed.substr(eq + 1);

            // Trim key/value
            auto k_start = key.find_first_not_of(" \t");
            auto k_end = key.find_last_not_of(" \t");
            if (k_start != std::string::npos)
                key = key.substr(k_start, k_end - k_start + 1);

            auto v_start = value.find_first_not_of(" \t");
            auto v_end = value.find_last_not_of(" \t");
            if (v_start != std::string::npos)
                value = value.substr(v_start, v_end - v_start + 1);

            // Remove inline comments (only after value)
            auto semi = value.find(';');
            auto hash = value.find('#');
            auto comment_pos = std::min(
                semi != std::string::npos ? semi : std::string::npos,
                hash != std::string::npos ? hash : std::string::npos
            );
            if (comment_pos != std::string::npos) {
                // Check if comment is before any quotes
                auto first_quote = value.find_first_of("\"");
                if (first_quote == std::string::npos || comment_pos < first_quote)
                    value = value.substr(0, comment_pos);
                // Trim again
                auto v_end2 = value.find_last_not_of(" \t");
                if (v_end2 != std::string::npos)
                    value = value.substr(0, v_end2 + 1);
            }

            ini.sections[current_section][key] = value;
        }
    }

    return ini;
}

std::string generate(const INIFile& ini) {
    std::ostringstream out;
    for (auto& [section, keys] : ini.sections) {
        out << "[" << section << "]\n";
        for (auto& [key, value] : keys)
            out << key << "=" << value << "\n";
        out << "\n";
    }
    return out.str();
}

} // namespace ini

// ============================================================
// INIFile convenience methods
// ============================================================
std::string INIFile::get(const std::string& section, const std::string& key) const {
    auto sit = sections.find(section);
    if (sit == sections.end()) return {};
    auto kit = sit->second.find(key);
    return (kit != sit->second.end()) ? kit->second : std::string{};
}

void INIFile::set(const std::string& section, const std::string& key, const std::string& value) {
    sections[section][key] = value;
}

bool INIFile::has(const std::string& section, const std::string& key) const {
    auto sit = sections.find(section);
    if (sit == sections.end()) return false;
    return sit->second.find(key) != sit->second.end();
}

std::vector<std::string> INIFile::keys(const std::string& section) const {
    auto sit = sections.find(section);
    if (sit == sections.end()) return {};
    std::vector<std::string> result;
    result.reserve(sit->second.size());
    for (auto& [k, _] : sit->second)
        result.push_back(k);
    return result;
}

std::vector<std::string> INIFile::section_names() const {
    std::vector<std::string> result;
    result.reserve(sections.size());
    for (auto& [s, _] : sections)
        result.push_back(s);
    return result;
}
