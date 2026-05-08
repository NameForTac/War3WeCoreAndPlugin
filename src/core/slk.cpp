#include "slk.h"
#include <sstream>
#include <charconv>

namespace slk {

// Split a string on delimiter, handling quoted values
static std::vector<std::string> split_slk(const std::string& line, char delim) {
    std::vector<std::string> parts;
    std::string cur;
    for (size_t i = 0; i < line.size(); ++i) {
        if (line[i] == '"') {
            cur += line[i];
            ++i;
            while (i < line.size() && line[i] != '"') {
                cur += line[i];
                ++i;
            }
            if (i < line.size()) cur += line[i];
        } else if (line[i] == delim) {
            parts.push_back(cur);
            cur.clear();
        } else {
            cur += line[i];
        }
    }
    parts.push_back(cur);
    return parts;
}

SLKTable parse(const std::string& text) {
    SLKTable table;
    std::istringstream stream(text);
    std::string line;

    // Temporary storage by (col, row)
    struct Cell { int col; int row; std::string value; };
    std::vector<Cell> cells;
    int max_col = 0, max_row = 0;

    int current_row = 0;
    while (std::getline(stream, line)) {
        if (line.empty() || line == "\r") continue;
        if (line.back() == '\r') line.pop_back();

        if (line[0] == 'I' && line.size() >= 2 && line[1] == 'D') {
            // Header: ID;PWXL;N;E
            continue;
        }
        if (line[0] == 'B') {
            // Bounds: B;X{cols};Y{rows};D0
            auto parts = split_slk(line, ';');
            for (auto& p : parts) {
                if (p.size() >= 2 && p[0] == 'X') {
                    max_col = std::max(max_col, std::stoi(p.substr(1)));
                } else if (p.size() >= 2 && p[0] == 'Y') {
                    max_row = std::max(max_row, std::stoi(p.substr(1)));
                }
            }
            continue;
        }
        if (line[0] == 'C') {
            // Cell: C;X{col};Y{row};K{value}
            auto parts = split_slk(line, ';');
            int col = 0;
            std::string value;
            for (auto& p : parts) {
                if (p.size() >= 2 && p[0] == 'X') {
                    col = std::stoi(p.substr(1));
                } else if (p.size() >= 2 && p[0] == 'Y') {
                    current_row = std::stoi(p.substr(1));
                } else if (p.size() >= 2 && p[0] == 'K') {
                    value = p.substr(1);
                    // Unquote if quoted
                    if (value.size() >= 2 && value.front() == '"' && value.back() == '"')
                        value = value.substr(1, value.size() - 2);
                }
            }
            max_col = std::max(max_col, col);
            max_row = std::max(max_row, current_row);
            cells.push_back({col, current_row, value});
            continue;
        }
        if (line[0] == 'E') {
            // End
            break;
        }
    }

    if (max_row < 1) return table;

    // Resolve headers from row 1
    table.headers.resize(max_col);
    for (auto& c : cells) {
        if (c.row == 1 && c.col <= max_col) {
            table.headers[c.col - 1] = c.value;
        }
    }

    // Resolve data rows
    int data_start_row = 2;
    table.rows.resize(max_row - 1); // exclude header row
    for (int r = data_start_row; r <= max_row; ++r) {
        std::vector<std::string> row(max_col);
        for (auto& c : cells) {
            if (c.row == r && c.col <= max_col) {
                row[c.col - 1] = c.value;
            }
        }
        table.rows[r - data_start_row] = std::move(row);
    }

    return table;
}

std::string generate(const SLKTable& table) {
    std::string out;
    int num_cols = (int)table.headers.size();
    int num_rows = (int)table.rows.size();

    out += "ID;PWXL;N;E\r\n";
    out += "B;X" + std::to_string(num_cols) + ";Y" + std::to_string(num_rows + 1) + ";D0\r\n";

    // Header row (Y1)
    for (int c = 0; c < num_cols; ++c) {
        out += "C;X" + std::to_string(c + 1) + ";Y1;K\"" + table.headers[c] + "\"\r\n";
    }

    // Data rows
    for (int r = 0; r < num_rows; ++r) {
        int y = r + 2;
        for (int c = 0; c < num_cols; ++c) {
            const auto& val = table.rows[r][c];
            if (val.empty()) continue;

            bool is_string = false;
            for (char ch : val) {
                if (!std::isdigit(static_cast<unsigned char>(ch)) && ch != '.' && ch != '-') {
                    is_string = true;
                    break;
                }
            }

            if (is_string) {
                out += "C;X" + std::to_string(c + 1) + ";Y" + std::to_string(y) + ";K\"" + val + "\"\r\n";
            } else {
                out += "C;X" + std::to_string(c + 1) + ";Y" + std::to_string(y) + ";K" + val + "\r\n";
            }
        }
    }

    out += "E\r\n";
    return out;
}

} // namespace slk
