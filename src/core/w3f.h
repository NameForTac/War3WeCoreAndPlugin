#pragma once

#include "buffer.h"
#include <string>
#include <vector>

// ============================================================
// war3campaign.w3f — Warcraft III campaign information file
// ============================================================
struct CampaignMap {
    int32_t visible = 1;
    std::string chapter_title;
    std::string map_title;
    std::string map_path;
};

struct CampaignFlowMap {
    std::string unknown;
    std::string map_path;
};

struct CampaignInfo {
    int32_t version = 1;
    int32_t campaign_version = 0;
    int32_t editor_version = 0;

    std::string campaign_name;
    std::string campaign_difficulty;
    std::string author;
    std::string description;

    int32_t flags = 0;       // 0=Fixed+W3M, 1=Variable+W3M, 2=Fixed+W3X, 3=Variable+W3X
    int32_t bg_screen_index = 0;
    std::string custom_bg_screen_path;
    std::string minimap_image_path;
    int32_t ambient_sound_index = 0;
    std::string custom_ambient_sound_path;

    int32_t fog_flag = 0;   // terrain fog flag + parameters
    int32_t ui_race = 0;    // cursor/UI race index

    std::vector<CampaignMap> maps;
    std::vector<CampaignFlowMap> flow_maps;
};

namespace w3f {

CampaignInfo read(Buffer& buf);
void write(Buffer& buf, const CampaignInfo& info);

} // namespace w3f
