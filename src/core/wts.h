#pragma once

#include "buffer.h"

// Read/write war3map.wts (string table)
namespace wts {

WTS       parse(const std::string& text);
std::string generate(const WTS& table);

} // namespace wts
