#include "wts.h"
#include <sstream>
#include <cctype>

namespace wts {

WTS parse(const std::string& text) {
    WTS table;
    std::istringstream stream(text);
    std::string line;
    int current_id = -1;
    std::string current_value;
    bool in_brace = false;
    bool in_quotes = false;  // inside a quoted string value

    auto finish_string = [&]() {
        if (current_id >= 0) {
            while (!current_value.empty() &&
                   (current_value.back() == '\n' || current_value.back() == '\r' ||
                    current_value.back() == ' ')) {
                current_value.pop_back();
            }
            table[current_id] = current_value;
        }
        current_id = -1;
        current_value.clear();
        in_brace = false;
        in_quotes = false;
    };

    while (std::getline(stream, line)) {
        if (!line.empty() && line.back() == '\r')
            line.pop_back();

        // } always terminates the current string
        if (line == "}") {
            finish_string();
            continue;
        }

        if (!in_brace) {
            // Look for "STRING N" pattern
            if (line.size() > 6 && line.substr(0, 6) == "STRING") {
                finish_string();
                char* end = nullptr;
                long id = std::strtol(line.c_str() + 6, &end, 10);
                if (end != line.c_str() + 6) {
                    current_id = (int)id;
                }
                continue;
            }
            if (line == "{") {
                in_brace = true;
                continue;
            }
        } else {
            // We're inside braces, collecting the value
            if (current_id < 0) continue;

            if (!in_quotes && line.size() >= 1 && line.front() == '"') {
                // Start of a quoted string
                in_quotes = true;
                if (line.size() >= 2 && line.back() == '"') {
                    // Single-line quoted: "value"
                    current_value = line.substr(1, line.size() - 2);
                    in_quotes = false;
                } else {
                    // Multi-line quoted: starts with " but continues
                    current_value = line.substr(1);
                }
            } else if (in_quotes) {
                // Inside a multi-line quoted string
                if (!line.empty() && line.back() == '"') {
                    current_value += "\n" + line.substr(0, line.size() - 1);
                    in_quotes = false;
                } else {
                    current_value += "\n" + line;
                }
            } else {
                // Unquoted value (raw text between braces)
                if (!current_value.empty())
                    current_value += "\n";
                current_value += line;
            }
        }
    }
    finish_string();

    return table;
}

std::string generate(const WTS& table) {
    std::string result;
    for (auto& [id, value] : table) {
        result += "STRING " + std::to_string(id) + "\n";
        result += "{\n";
        // Only wrap in quotes if value doesn't contain quotes (avoids ambiguity)
        if (value.find('"') == std::string::npos) {
            result += "\"" + value + "\"\n";
        } else {
            result += value + "\n";
        }
        result += "}\n\n";
    }
    return result;
}

} // namespace wts
