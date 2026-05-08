#include "blp_reader.h"
#include <cstring>
#include <algorithm>
#include <QDebug>
#include <QFile>
#include <windows.h>

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
    if (size < 160) return {};  // header + at least partial palette

    uint32_t w = r32(data, 12);
    uint32_t h = r32(data, 16);
    uint32_t has_alpha = r32(data, 8);
    uint32_t pix_count = w * h;

    if (w < 1 || h < 1 || w > 4096 || h > 4096) return {};
    if (pix_count == 0) return {};

    // Palette: 256 * 4 = 1024 bytes starting at offset 156
    if (size < 156 + 1024) return {};
    const uint8_t* pal = data + 156;

    // Pixel indices start after the palette (offset 156 + 1024 = 1180)
    size_t idx_off = 156 + 1024;
    if (idx_off + pix_count > size) return {};

    QImage img((int)w, (int)h, QImage::Format_RGBA8888);
    const uint8_t* idx = data + idx_off;

    if (has_alpha & 0x08) {
        // Has alpha channel — stored after pixel indices
        size_t alpha_off = idx_off + pix_count;
        if (alpha_off + pix_count > size) {
            // Try without alpha if data is truncated
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
// The BLP stores a shared JPEG header + SOS start, then each mipmap
// level has its own continuation of the entropy-coded data.
// We reconstruct a complete JPEG for the largest mipmap by concatenating
// the shared header with the mipmap 0 data.
static QImage decode_jpeg_blp(const uint8_t* data, size_t size) {
    if (size < 160) return {};

    uint32_t jpeg_hdr_size = r32(data, 156);  // 4 bytes after fixed header
    uint32_t mip0_offset  = r32(data, 28);    // mipmap_offset[0]
    uint32_t mip0_size    = r32(data, 92);    // mipmap_size[0]

    if (jpeg_hdr_size == 0 || jpeg_hdr_size >= 1024 * 1024) return {};
    if (mip0_offset == 0 || mip0_size == 0 || mip0_size >= 4 * 1024 * 1024) return {};

    // The shared JPEG header starts after the jpeg_hdr_size field
    size_t hdr_off = 160;  // 156 + 4 bytes for jpeg_hdr_size
    size_t hdr_end = hdr_off + jpeg_hdr_size;
    size_t mip_end = (size_t)mip0_offset + mip0_size;

    if (hdr_end > size || mip_end > size) return {};

    // Reconstruct full JPEG: shared header + mip0 data
    // But first check: does the shared header already include the SOI marker?
    // Standard JPEG starts with FF D8. BLP may or may not include it.
    std::vector<uint8_t> jpeg_data;
    jpeg_data.reserve(jpeg_hdr_size + mip0_size + 2);

    // Add SOI marker if not present
    if (jpeg_hdr_size < 2 || data[hdr_off] != 0xFF || data[hdr_off + 1] != 0xD8) {
        jpeg_data.push_back(0xFF);
        jpeg_data.push_back(0xD8);
    }

    jpeg_data.insert(jpeg_data.end(), data + hdr_off, data + hdr_end);
    jpeg_data.insert(jpeg_data.end(), data + mip0_offset, data + mip0_offset + mip0_size);

    // Add EOI marker if not present
    if (jpeg_data.size() < 2 ||
        jpeg_data[jpeg_data.size() - 2] != 0xFF ||
        jpeg_data[jpeg_data.size() - 1] != 0xD9)
    {
        jpeg_data.push_back(0xFF);
        jpeg_data.push_back(0xD9);
    }

    // Use Qt to decode the JPEG
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

    // Validate magic
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

// ============================================================
// Load a texture from War3.mpq using StormLib (via LoadLibrary)
// ============================================================

// StormLib function pointer types
typedef bool (WINAPI *SFOpenArchive_t)(const char*, DWORD, DWORD, HANDLE*);
typedef bool (WINAPI *SFFileExists_t)(HANDLE, const char*, DWORD);
typedef bool (WINAPI *SFOpenFileEx_t)(HANDLE, const char*, DWORD, HANDLE*);
typedef DWORD (WINAPI *SFGetFileSize_t)(HANDLE, DWORD*);
typedef bool (WINAPI *SFReadFile_t)(HANDLE, void*, DWORD, DWORD*, LPOVERLAPPED);
typedef bool (WINAPI *SFCloseArchive_t)(HANDLE);
typedef bool (WINAPI *SFCloseFile_t)(HANDLE);

// Load all StormLib functions via GetProcAddress so TCHAR mismatch is irrelevant
static HMODULE s_storm_lib = nullptr;
static SFOpenArchive_t   s_SFileOpenArchive   = nullptr;
static SFOpenFileEx_t    s_SFileOpenFileEx    = nullptr;
static SFGetFileSize_t   s_SFileGetFileSize   = nullptr;
static SFReadFile_t      s_SFileReadFile      = nullptr;
static SFCloseArchive_t  s_SFileCloseArchive  = nullptr;
static SFCloseFile_t     s_SFileCloseFile     = nullptr;

static bool ensure_stormlib_loaded() {
    if (s_storm_lib) return true;
    s_storm_lib = LoadLibraryW(L"libStormLib.dll");
    if (!s_storm_lib) return false;
    s_SFileOpenArchive  = (SFOpenArchive_t) GetProcAddress(s_storm_lib, "SFileOpenArchive");
    s_SFileOpenFileEx   = (SFOpenFileEx_t)    GetProcAddress(s_storm_lib, "SFileOpenFileEx");
    s_SFileGetFileSize  = (SFGetFileSize_t)   GetProcAddress(s_storm_lib, "SFileGetFileSize");
    s_SFileReadFile     = (SFReadFile_t)      GetProcAddress(s_storm_lib, "SFileReadFile");
    s_SFileCloseArchive = (SFCloseArchive_t)  GetProcAddress(s_storm_lib, "SFileCloseArchive");
    s_SFileCloseFile    = (SFCloseFile_t)     GetProcAddress(s_storm_lib, "SFileCloseFile");
    return (s_SFileOpenArchive && s_SFileOpenFileEx && s_SFileGetFileSize &&
            s_SFileReadFile && s_SFileCloseArchive && s_SFileCloseFile);
}

QImage load_wc3_texture(const std::string& wc3_data_dir, const std::string& mpq_path) {
    if (wc3_data_dir.empty()) {
        qWarning() << "[BLP] WC3 data directory not set";
        return {};
    }

    if (!ensure_stormlib_loaded()) {
        static bool warned = false;
        if (!warned) { qWarning() << "[BLP] Cannot load libStormLib.dll"; warned = true; }
        return {};
    }

    auto build_path = [&](const std::string& file) -> std::string {
        std::string p = wc3_data_dir;
        if (!p.empty() && p.back() != '/' && p.back() != '\\')
            p += '/';
        p += file;
        for (auto& c : p) if (c == '/') c = '\\';
        return p;
    };

    // Try war3.mpq first, then War3x.mpq
    const char* candidates[] = { "war3.mpq", "War3x.mpq" };
    for (auto name : candidates) {
        std::string mpq_file = build_path(name);
        HANDLE hMpq = nullptr;
        if (s_SFileOpenArchive(mpq_file.c_str(), 0, 0x100 /*MPQ_OPEN_READ_ONLY*/,
                               &hMpq) && hMpq) {
            HANDLE hFile = nullptr;
            if (s_SFileOpenFileEx(hMpq, mpq_path.c_str(), 0, &hFile)) {
                DWORD file_size = s_SFileGetFileSize(hFile, nullptr);
                if (file_size > 0 && file_size != 0xFFFFFFFF) {
                    std::vector<uint8_t> buf(file_size);
                    DWORD read = 0;
                    if (s_SFileReadFile(hFile, buf.data(), file_size, &read, nullptr) &&
                        read == file_size) {
                        s_SFileCloseFile(hFile);
                        s_SFileCloseArchive(hMpq);
                        QImage img = read_blp(buf);
                        if (!img.isNull()) return img;
                    } else {
                        s_SFileCloseFile(hFile);
                    }
                } else {
                    s_SFileCloseFile(hFile);
                }
            }
            s_SFileCloseArchive(hMpq);
        }
    }

    static bool warned = false;
    if (!warned) {
        qWarning().noquote()
            << QStringLiteral("[BLP] Cannot open MPQ from %1")
                   .arg(QString::fromStdString(wc3_data_dir));
        warned = true;
    }
    return {};
}
