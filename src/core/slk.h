#pragma once

#include "buffer.h"

// Read/write SLK (SYLK spreadsheet format)
namespace slk {

SLKTable  parse(const std::string& text);
std::string generate(const SLKTable& table);

} // namespace slk
