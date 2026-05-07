#pragma once

#include "buffer.h"
#include <string>
#include <vector>

// ============================================================
// war3map.w3c — Cinematic Cameras
// ============================================================
struct Camera {
    float target_x = 0;
    float target_y = 0;
    float z_offset = 0;
    float rotation = 0;     // degrees
    float angle_of_attack = 0; // degrees
    float distance = 0;
    float roll = 0;
    float fov = 0;          // field of view, degrees
    float far_z = 0;
    float unk = 100.0f;
    std::string name;
};

namespace w3c {

std::vector<Camera> read(Buffer& buf);
void write(Buffer& buf, const std::vector<Camera>& cameras);

} // namespace w3c
