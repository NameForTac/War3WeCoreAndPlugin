#include "../src/core/wts.h"
#include <cstring>

void test_wts() {
    WTS original;
    original[0] = "Hello World";
    original[1] = "Multi\nLine\nString";
    original[5] = "Another string with \"quotes\"";

    auto text = wts::generate(original);
    auto parsed = wts::parse(text);

    if (parsed.size() != original.size())
        throw std::runtime_error("WTS count mismatch");

    if (parsed[0] != "Hello World")
        throw std::runtime_error("WTS[0] mismatch");

    if (parsed[1] != "Multi\nLine\nString")
        throw std::runtime_error("WTS[1] mismatch");

    if (parsed[5] != "Another string with \"quotes\"")
        throw std::runtime_error("WTS[5] mismatch");

    // Roundtrip
    auto text2 = wts::generate(parsed);
    if (text != text2)
        throw std::runtime_error("WTS roundtrip mismatch");
}
