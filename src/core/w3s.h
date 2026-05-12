#pragma once

#include "buffer.h"
#include <string>
#include <vector>

// ============================================================
// war3map.w3s — Sound definitions
// ============================================================
struct SoundDef {
    std::string id;          // Sound ID name
    std::string file_path;   // Sound file path
    std::string eax_effect;  // EAX effect name
    uint32_t flags = 0;
    uint32_t fade_in_rate = 0;
    uint32_t fade_out_rate = 0;
    uint32_t volume = 0;
    float pitch = 1.0f;
    float unknown_a = 0;
    float unknown_b = 0;
    uint32_t channel = 0;
    float min_distance = 0;
    float max_distance = 0;
    float distance_cutoff = 0;
    float unknown_c = 0;
    float unknown_d = 0;
    float unknown_e = 0;
    float unknown_f = 0;
    float unknown_g = 0;

    // Sentinel value for unset floats
    static constexpr float SENTINEL = 1330004544.0f; // 0x4F800000
};

struct Sounds {
    uint32_t version = 1;
    std::vector<SoundDef> sounds;
};

namespace w3s {

Sounds read(Buffer& buf);
void write(Buffer& buf, const Sounds& sounds);

} // namespace w3s
