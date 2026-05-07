#include "imp.h"

namespace imp {

std::vector<ImportedFile> read(Buffer& buf) {
    /*uint32_t version =*/ buf.read_u32();
    uint32_t count = buf.read_u32();

    std::vector<ImportedFile> files;
    files.reserve(count);

    for (uint32_t i = 0; i < count; ++i) {
        ImportedFile f;
        f.path_type = buf.read_u8();
        f.path      = buf.read_string();
        files.push_back(std::move(f));
    }

    return files;
}

void write(Buffer& buf, const std::vector<ImportedFile>& files) {
    buf.write_u32(0); // version
    buf.write_u32((uint32_t)files.size());

    for (auto& f : files) {
        buf.write_u8(f.path_type);
        buf.write_string(f.path);
    }
}

} // namespace imp
