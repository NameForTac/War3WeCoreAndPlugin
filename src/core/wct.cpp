#include "wct.h"

namespace wct {

static CustomTextTrigger read_ct(Buffer& buf) {
    int32_t size = buf.read_i32(); // size including null terminator
    CustomTextTrigger ct;
    if (size > 0) {
        ct.text.resize(size - 1); // exclude null
        buf.read_raw(ct.text.data(), size - 1);
        buf.read_u8(); // null terminator
    }
    return ct;
}

static void write_ct(Buffer& buf, const CustomTextTrigger& ct) {
    int32_t size = static_cast<int32_t>(ct.text.size()) + 1; // +1 for null
    buf.write_i32(size);
    buf.write_raw(ct.text.data(), ct.text.size());
    buf.write_u8(0);
}

CustomTextTriggers read(Buffer& buf) {
    CustomTextTriggers ctt;
    ctt.version = buf.read_u32();
    ctt.custom_script_comment = buf.read_string();

    // Global custom text (1 always present)
    ctt.global_custom_text = read_ct(buf);

    // Trigger custom texts
    uint32_t count = buf.read_u32();
    ctt.triggers.reserve(count);
    for (uint32_t i = 0; i < count; ++i)
        ctt.triggers.push_back(read_ct(buf));

    return ctt;
}

void write(Buffer& buf, const CustomTextTriggers& ctt) {
    buf.write_u32(ctt.version);
    buf.write_string(ctt.custom_script_comment);

    write_ct(buf, ctt.global_custom_text);

    buf.write_u32(static_cast<uint32_t>(ctt.triggers.size()));
    for (auto& ct : ctt.triggers)
        write_ct(buf, ct);
}

} // namespace wct
