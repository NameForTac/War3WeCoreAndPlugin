#include "../src/core/slk.h"
#include <cstring>

void test_slk() {
    SLKTable original;
    original.headers = {"ID", "Name", "Value", "IsGood"};

    original.rows.push_back({"u000", "Footman", "250", "1"});
    original.rows.push_back({"u001", "Knight", "500", "1"});
    original.rows.push_back({"u002", "Peasant", "100", "0"});

    auto text = slk::generate(original);
    auto parsed = slk::parse(text);

    if (parsed.headers.size() != original.headers.size())
        throw std::runtime_error("SLK header count mismatch");

    for (size_t i = 0; i < parsed.headers.size(); ++i) {
        if (parsed.headers[i] != original.headers[i])
            throw std::runtime_error("SLK header mismatch at " + std::to_string(i));
    }

    if (parsed.rows.size() != original.rows.size())
        throw std::runtime_error("SLK row count mismatch");

    for (size_t r = 0; r < parsed.rows.size(); ++r) {
        for (size_t c = 0; c < parsed.rows[r].size(); ++c) {
            if (parsed.rows[r][c] != original.rows[r][c]) {
                throw std::runtime_error("SLK data mismatch at " +
                    std::to_string(r) + "," + std::to_string(c));
            }
        }
    }

    // Roundtrip
    auto text2 = slk::generate(parsed);
    if (text != text2)
        throw std::runtime_error("SLK roundtrip mismatch");
}
