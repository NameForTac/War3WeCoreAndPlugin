#pragma once

#include "buffer.h"
#include "types.h"
#include <utility>
#include <vector>

// ============================================================
// w3o — Combined Object Export File
//
// Wraps all 7 object file types into a single file.
// Format: version(4) + 7 × (presence_flag(4) + [object_file])
// ============================================================
namespace w3o {

// Read all sections present in a w3o buffer.
// Returns pairs of (file_type, object_data).
void read(Buffer& buf, std::vector<std::pair<ObjectFileType, ObjectFile>>& out);

// Write all sections to a buffer.
void write(Buffer& buf, const std::vector<std::pair<ObjectFileType, ObjectFile>>& sections);

} // namespace w3o
