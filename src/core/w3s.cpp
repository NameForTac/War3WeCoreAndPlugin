#include "w3s.h"

namespace w3s {

Sounds read(Buffer& buf) {
    Sounds s;
    s.version = buf.read_u32();

    uint32_t count = buf.read_u32();
    s.sounds.reserve(count);

    for (uint32_t i = 0; i < count; ++i) {
        SoundDef sd;
        sd.id             = buf.read_string();
        sd.file_path      = buf.read_string();
        sd.eax_effect     = buf.read_string();
        sd.flags          = buf.read_u32();
        sd.fade_in_rate   = buf.read_u32();
        sd.fade_out_rate  = buf.read_u32();
        sd.volume         = buf.read_u32();
        sd.pitch          = buf.read_f32();
        sd.unknown_a      = buf.read_f32();
        sd.unknown_b      = buf.read_f32();
        sd.channel        = buf.read_u32();
        sd.min_distance   = buf.read_f32();
        sd.max_distance   = buf.read_f32();
        sd.distance_cutoff = buf.read_f32();
        sd.unknown_c      = buf.read_f32();
        sd.unknown_d      = buf.read_f32();
        sd.unknown_e      = buf.read_f32();
        sd.unknown_f      = buf.read_f32();
        sd.unknown_g      = buf.read_f32();
        s.sounds.push_back(std::move(sd));
    }

    return s;
}

void write(Buffer& buf, const Sounds& s) {
    buf.write_u32(s.version);
    buf.write_u32(static_cast<uint32_t>(s.sounds.size()));

    for (auto& sd : s.sounds) {
        buf.write_string(sd.id);
        buf.write_string(sd.file_path);
        buf.write_string(sd.eax_effect);
        buf.write_u32(sd.flags);
        buf.write_u32(sd.fade_in_rate);
        buf.write_u32(sd.fade_out_rate);
        buf.write_u32(sd.volume);
        buf.write_f32(sd.pitch);
        buf.write_f32(sd.unknown_a);
        buf.write_f32(sd.unknown_b);
        buf.write_u32(sd.channel);
        buf.write_f32(sd.min_distance);
        buf.write_f32(sd.max_distance);
        buf.write_f32(sd.distance_cutoff);
        buf.write_f32(sd.unknown_c);
        buf.write_f32(sd.unknown_d);
        buf.write_f32(sd.unknown_e);
        buf.write_f32(sd.unknown_f);
        buf.write_f32(sd.unknown_g);
    }
}

} // namespace w3s
