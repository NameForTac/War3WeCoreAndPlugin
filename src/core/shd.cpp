#include "shd.h"

namespace shd {

ShadowMap read(Buffer& buf, int32_t width, int32_t height) {
    ShadowMap sm;
    sm.width = width;
    sm.height = height;
    auto count = static_cast<size_t>(width * height);
    sm.data.resize(count);
    buf.read_raw(sm.data.data(), count);
    return sm;
}

void write(Buffer& buf, const ShadowMap& sm) {
    buf.write_raw(sm.data.data(), sm.data.size());
}

} // namespace shd
