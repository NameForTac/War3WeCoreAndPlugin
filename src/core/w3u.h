#pragma once

#include "buffer.h"

// Read/write object files (w3a/w3u/w3t/w3h/w3b/w3d/w3q)
// All share the same binary format, but differ in extra fields
// in modification structures (see ObjectFileType).
namespace w3u {

ObjectFile read(Buffer& buf, ObjectFileType type = ObjectFileType::Units);
void       write(Buffer& buf, const ObjectFile& obj);

} // namespace w3u
