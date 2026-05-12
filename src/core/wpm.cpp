#include "wpm.h"
#include <stdexcept>

namespace wpm {

PathingMap read(Buffer& buf) {
    // "MP3W" little-endian
    uint32_t magic = buf.read_u32();
    if (magic != 0x5733504D)
        throw std::runtime_error("wpm: bad magic");

    PathingMap pm;
    pm.version = buf.read_u32();
    pm.width   = buf.read_i32();
    pm.height  = buf.read_i32();

    auto count = static_cast<size_t>(pm.width * pm.height);
    pm.data.resize(count);
    buf.read_raw(pm.data.data(), count);

    return pm;
}

void write(Buffer& buf, const PathingMap& pm) {
    buf.write_u32(0x5733504D); // "MP3W"
    buf.write_u32(pm.version);
    buf.write_i32(pm.width);
    buf.write_i32(pm.height);
    buf.write_raw(pm.data.data(), pm.data.size());
}

} // namespace wpm
