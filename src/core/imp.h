#pragma once

#include "buffer.h"
#include <string>
#include <vector>

// ============================================================
// war3map.imp — Imported Files List
// ============================================================
struct ImportedFile {
    uint8_t path_type = 5;   // 5/8 = standard "war3mapImported\", 10/13 = custom path
    std::string path;
};

namespace imp {

std::vector<ImportedFile> read(Buffer& buf);
void write(Buffer& buf, const std::vector<ImportedFile>& files);

} // namespace imp
