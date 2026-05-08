#pragma once
#include <QImage>
#include <vector>

struct BLPHeader {
    char     magic[4];     // "BLP1"
    uint32_t type;         // 0=JPEG, 1=Paletted
    uint32_t has_alpha;    // 0x08=has alpha
    uint32_t width;
    uint32_t height;
    uint32_t team_color;   // 3/4/5
    uint32_t always_1;
    uint32_t mipmap_offset[16];
    uint32_t mipmap_size[16];
};

// Decode a BLP1 file from raw bytes into a QImage (largest mipmap level).
QImage read_blp(const std::vector<uint8_t>& data);

// Load a texture from a WC3 MPQ file path (e.g. "TerrainArt/LordaeronSummer/Ldrt.blp")
// by opening the War3.mpq from the given WC3 data directory.
QImage load_wc3_texture(const std::string& wc3_data_dir, const std::string& mpq_path);
