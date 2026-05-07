#include "w3c.h"

namespace w3c {

std::vector<Camera> read(Buffer& buf) {
    /*uint32_t version =*/ buf.read_u32();
    uint32_t count = buf.read_u32();

    std::vector<Camera> cameras;
    cameras.reserve(count);

    for (uint32_t i = 0; i < count; ++i) {
        Camera c;
        c.target_x        = buf.read_f32();
        c.target_y        = buf.read_f32();
        c.z_offset        = buf.read_f32();
        c.rotation        = buf.read_f32();
        c.angle_of_attack = buf.read_f32();
        c.distance        = buf.read_f32();
        c.roll            = buf.read_f32();
        c.fov             = buf.read_f32();
        c.far_z           = buf.read_f32();
        c.unk             = buf.read_f32();
        c.name            = buf.read_string();
        cameras.push_back(std::move(c));
    }

    return cameras;
}

void write(Buffer& buf, const std::vector<Camera>& cameras) {
    buf.write_u32(0); // version
    buf.write_u32((uint32_t)cameras.size());

    for (auto& c : cameras) {
        buf.write_f32(c.target_x);
        buf.write_f32(c.target_y);
        buf.write_f32(c.z_offset);
        buf.write_f32(c.rotation);
        buf.write_f32(c.angle_of_attack);
        buf.write_f32(c.distance);
        buf.write_f32(c.roll);
        buf.write_f32(c.fov);
        buf.write_f32(c.far_z);
        buf.write_f32(c.unk);
        buf.write_string(c.name);
    }
}

} // namespace w3c
