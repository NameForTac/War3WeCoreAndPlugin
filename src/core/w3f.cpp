#include "w3f.h"

namespace w3f {

CampaignInfo read(Buffer& buf) {
    CampaignInfo ci;
    ci.version          = buf.read_i32();
    ci.campaign_version = buf.read_i32();
    ci.editor_version   = buf.read_i32();

    ci.campaign_name       = buf.read_string();
    ci.campaign_difficulty = buf.read_string();
    ci.author              = buf.read_string();
    ci.description         = buf.read_string();

    ci.flags               = buf.read_i32();
    ci.bg_screen_index     = buf.read_i32();
    ci.custom_bg_screen_path = buf.read_string();
    ci.minimap_image_path  = buf.read_string();
    ci.ambient_sound_index = buf.read_i32();
    ci.custom_ambient_sound_path = buf.read_string();

    ci.fog_flag = buf.read_i32();
    ci.ui_race  = buf.read_i32();

    // Maps
    int32_t map_count = buf.read_i32();
    ci.maps.reserve(map_count);
    for (int32_t i = 0; i < map_count; ++i) {
        CampaignMap m;
        m.visible       = buf.read_i32();
        m.chapter_title = buf.read_string();
        m.map_title     = buf.read_string();
        m.map_path      = buf.read_string();
        ci.maps.push_back(std::move(m));
    }

    // Flowchart maps
    int32_t flow_count = buf.read_i32();
    ci.flow_maps.reserve(flow_count);
    for (int32_t i = 0; i < flow_count; ++i) {
        CampaignFlowMap fm;
        fm.unknown  = buf.read_string();
        fm.map_path = buf.read_string();
        ci.flow_maps.push_back(std::move(fm));
    }

    return ci;
}

void write(Buffer& buf, const CampaignInfo& ci) {
    buf.write_i32(ci.version);
    buf.write_i32(ci.campaign_version);
    buf.write_i32(ci.editor_version);

    buf.write_string(ci.campaign_name);
    buf.write_string(ci.campaign_difficulty);
    buf.write_string(ci.author);
    buf.write_string(ci.description);

    buf.write_i32(ci.flags);
    buf.write_i32(ci.bg_screen_index);
    buf.write_string(ci.custom_bg_screen_path);
    buf.write_string(ci.minimap_image_path);
    buf.write_i32(ci.ambient_sound_index);
    buf.write_string(ci.custom_ambient_sound_path);

    buf.write_i32(ci.fog_flag);
    buf.write_i32(ci.ui_race);

    buf.write_i32(static_cast<int32_t>(ci.maps.size()));
    for (auto& m : ci.maps) {
        buf.write_i32(m.visible);
        buf.write_string(m.chapter_title);
        buf.write_string(m.map_title);
        buf.write_string(m.map_path);
    }

    buf.write_i32(static_cast<int32_t>(ci.flow_maps.size()));
    for (auto& fm : ci.flow_maps) {
        buf.write_string(fm.unknown);
        buf.write_string(fm.map_path);
    }
}

} // namespace w3f
