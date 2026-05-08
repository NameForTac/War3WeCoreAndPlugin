#include "blp_reader.h"
#include <cstring>
#include <algorithm>
#include <QDebug>
#include <QFile>

// ============================================================
// BLP1 decoder
// ============================================================

// Helper: read a uint32_t from an offset in the data
static inline uint32_t r32(const uint8_t* d, size_t off) {
    uint32_t v;
    std::memcpy(&v, d + off, 4);
    return v;
}

// Decode a paletted BLP (type 1)
static QImage decode_paletted(const uint8_t* data, size_t size) {
    if (size < 160) return {};

    uint32_t w = r32(data, 12);
    uint32_t h = r32(data, 16);
    uint32_t has_alpha = r32(data, 8);
    uint32_t pix_count = w * h;

    if (w < 1 || h < 1 || w > 4096 || h > 4096) return {};
    if (pix_count == 0) return {};

    if (size < 156 + 1024) return {};
    const uint8_t* pal = data + 156;

    size_t idx_off = 156 + 1024;
    if (idx_off + pix_count > size) return {};

    QImage img((int)w, (int)h, QImage::Format_RGBA8888);
    const uint8_t* idx = data + idx_off;

    if (has_alpha & 0x08) {
        size_t alpha_off = idx_off + pix_count;
        if (alpha_off + pix_count > size) {
            for (uint32_t i = 0; i < pix_count; ++i) {
                uint8_t b = pal[idx[i] * 4];
                uint8_t g = pal[idx[i] * 4 + 1];
                uint8_t r = pal[idx[i] * 4 + 2];
                img.bits()[i * 4]     = r;
                img.bits()[i * 4 + 1] = g;
                img.bits()[i * 4 + 2] = b;
                img.bits()[i * 4 + 3] = 255;
            }
        } else {
            const uint8_t* alpha = data + alpha_off;
            for (uint32_t i = 0; i < pix_count; ++i) {
                uint8_t b = pal[idx[i] * 4];
                uint8_t g = pal[idx[i] * 4 + 1];
                uint8_t r = pal[idx[i] * 4 + 2];
                img.bits()[i * 4]     = r;
                img.bits()[i * 4 + 1] = g;
                img.bits()[i * 4 + 2] = b;
                img.bits()[i * 4 + 3] = alpha[i];
            }
        }
    } else {
        for (uint32_t i = 0; i < pix_count; ++i) {
            uint8_t b = pal[idx[i] * 4];
            uint8_t g = pal[idx[i] * 4 + 1];
            uint8_t r = pal[idx[i] * 4 + 2];
            img.bits()[i * 4]     = r;
            img.bits()[i * 4 + 1] = g;
            img.bits()[i * 4 + 2] = b;
            img.bits()[i * 4 + 3] = 255;
        }
    }

    return img;
}

// Decode a JPEG BLP (type 0)
static QImage decode_jpeg_blp(const uint8_t* data, size_t size) {
    if (size < 160) return {};

    uint32_t jpeg_hdr_size = r32(data, 156);
    uint32_t mip0_offset  = r32(data, 28);
    uint32_t mip0_size    = r32(data, 92);

    if (jpeg_hdr_size == 0 || jpeg_hdr_size >= 1024 * 1024) return {};
    if (mip0_offset == 0 || mip0_size == 0 || mip0_size >= 4 * 1024 * 1024) return {};

    size_t hdr_off = 160;
    size_t hdr_end = hdr_off + jpeg_hdr_size;
    size_t mip_end = (size_t)mip0_offset + mip0_size;

    if (hdr_end > size || mip_end > size) return {};

    std::vector<uint8_t> jpeg_data;
    jpeg_data.reserve(jpeg_hdr_size + mip0_size + 2);

    if (jpeg_hdr_size < 2 || data[hdr_off] != 0xFF || data[hdr_off + 1] != 0xD8) {
        jpeg_data.push_back(0xFF);
        jpeg_data.push_back(0xD8);
    }

    jpeg_data.insert(jpeg_data.end(), data + hdr_off, data + hdr_end);
    jpeg_data.insert(jpeg_data.end(), data + mip0_offset, data + mip0_offset + mip0_size);

    if (jpeg_data.size() < 2 ||
        jpeg_data[jpeg_data.size() - 2] != 0xFF ||
        jpeg_data[jpeg_data.size() - 1] != 0xD9)
    {
        jpeg_data.push_back(0xFF);
        jpeg_data.push_back(0xD9);
    }

    QImage img = QImage::fromData(jpeg_data.data(), (int)jpeg_data.size(), "JPEG");
    return img;
}

QImage read_blp(const std::vector<uint8_t>& data) {
    if (data.size() < 156) {
        qWarning() << "[BLP] data too small:" << data.size();
        return {};
    }

    const uint8_t* raw = data.data();
    size_t size = data.size();

    if (raw[0] != 'B' || raw[1] != 'L' || raw[2] != 'P' || raw[3] != '1') {
        qWarning() << "[BLP] invalid magic:" << raw[0] << raw[1] << raw[2] << raw[3];
        return {};
    }

    uint32_t type = r32(raw, 4);
    uint32_t w = r32(raw, 12);
    uint32_t h = r32(raw, 16);

    if (w == 0 || h == 0 || w > 4096 || h > 4096) {
        qWarning() << "[BLP] invalid dimensions:" << w << "x" << h;
        return {};
    }

    QImage result;
    if (type == 0) {
        result = decode_jpeg_blp(raw, size);
    } else if (type == 1) {
        result = decode_paletted(raw, size);
    } else {
        qWarning() << "[BLP] unknown type:" << type;
        return {};
    }

    if (result.isNull()) {
        qWarning() << "[BLP] failed to decode type" << type << w << "x" << h;
    }

    return result;
}
