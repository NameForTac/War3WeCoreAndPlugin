#pragma once

#include "buffer.h"

// Read/write war3map.w3i (map info header)
// Supports versions 18, 25, 28, 31
namespace w3i {

MapInfo   read(Buffer& buf);
void      write(Buffer& buf, const MapInfo& info);

} // namespace w3i
