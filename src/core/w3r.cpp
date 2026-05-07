#include "w3r.h"

namespace w3r {

std::vector<Region> read(Buffer& buf) {
    /*uint32_t version =*/ buf.read_u32();
    uint32_t count = buf.read_u32();

    std::vector<Region> regions;
    regions.reserve(count);

    for (uint32_t i = 0; i < count; ++i) {
        Region r;
        r.left   = buf.read_f32();
        r.right  = buf.read_f32();
        r.bottom = buf.read_f32();
        r.top    = buf.read_f32();
        r.name   = buf.read_string();
        r.index  = buf.read_i32();

        // weather_id: 4-byte char code (all zeros = disabled)
        auto raw = buf.read_u32();
        r.weather_id = { (char)(raw & 0xFF), (char)((raw >> 8) & 0xFF),
                         (char)((raw >> 16) & 0xFF), (char)((raw >> 24) & 0xFF), '\0' };

        r.ambient_sound = buf.read_string();

        // 3 bytes: BB, GG, RR
        r.color_b = buf.read_u8();
        r.color_g = buf.read_u8();
        r.color_r = buf.read_u8();

        buf.read_u8(); // end marker byte
        regions.push_back(std::move(r));
    }

    return regions;
}

void write(Buffer& buf, const std::vector<Region>& regions) {
    buf.write_u32(5); // version
    buf.write_u32((uint32_t)regions.size());

    for (auto& r : regions) {
        buf.write_f32(r.left);
        buf.write_f32(r.right);
        buf.write_f32(r.bottom);
        buf.write_f32(r.top);
        buf.write_string(r.name);
        buf.write_i32(r.index);

        // Write 4-char weather ID
        uint32_t raw = 0;
        if (r.weather_id.size() >= 4) {
            raw = (uint8_t)r.weather_id[0]
                | ((uint8_t)r.weather_id[1] << 8)
                | ((uint8_t)r.weather_id[2] << 16)
                | ((uint8_t)r.weather_id[3] << 24);
        }
        buf.write_u32(raw);

        buf.write_string(r.ambient_sound);
        buf.write_u8(r.color_b);
        buf.write_u8(r.color_g);
        buf.write_u8(r.color_r);
        buf.write_u8(0); // end marker
    }
}

} // namespace w3r
