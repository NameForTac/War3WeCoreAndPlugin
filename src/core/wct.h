#pragma once

#include "buffer.h"
#include <string>
#include <vector>

// ============================================================
// war3map.wct — Custom text triggers
// ============================================================
struct CustomTextTrigger {
    std::string text; // null-terminated, size stored in file
};

struct CustomTextTriggers {
    uint32_t version = 1;
    std::string custom_script_comment;
    CustomTextTrigger global_custom_text;
    std::vector<CustomTextTrigger> triggers;
};

namespace wct {

CustomTextTriggers read(Buffer& buf);
void write(Buffer& buf, const CustomTextTriggers& ctt);

} // namespace wct
