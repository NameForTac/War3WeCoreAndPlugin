#include "archive.h"

#include <StormLib.h>
#include <cstring>
#include <algorithm>
#include <sstream>
#include <cstdio>

// ============================================================
// HM3W header constants
// ============================================================
static constexpr uint32_t HM3W_MAGIC = 0x57334D48; // "HM3W" little-endian

static constexpr size_t HM3W_MIN_SIZE = 16; // magic(4) + version(4) + name_min(2) + flags(4) + players(4)

struct HM3WHeader {
    uint32_t magic;
    uint32_t version;
    // Followed by: map_name (null-term), map_flags (u32), player_count (u32)
};

// ============================================================
// Archive implementation
// ============================================================
struct Archive::Impl {
    HANDLE handle = nullptr;
    std::string file_path;
    std::string mpq_tmp_path;
    std::vector<uint8_t> hm3w_header;

    // For reading: offset to MPQ data within the file
    size_t mpq_offset = 0;

    ~Impl() {
        if (!mpq_tmp_path.empty())
            std::remove(mpq_tmp_path.c_str());
    }
};

Archive::Archive() : impl_(std::make_unique<Impl>()) {}
Archive::~Archive() { close(); }

Archive::Archive(Archive&& other) noexcept : impl_(std::move(other.impl_)) {}
Archive& Archive::operator=(Archive&& other) noexcept {
    if (this != &other) {
        close();
        impl_ = std::move(other.impl_);
    }
    return *this;
}

void Archive::close() {
    if (impl_->handle) {
        SFileCompactArchive(impl_->handle, nullptr, false);
        SFileCloseArchive(impl_->handle);
        impl_->handle = nullptr;

        // Wrap temp MPQ with padded HM3W header to final path
        if (!impl_->mpq_tmp_path.empty()) {
            FILE* ftmp = fopen(impl_->mpq_tmp_path.c_str(), "rb");
            if (ftmp) {
                FILE* fout = fopen(impl_->file_path.c_str(), "wb");
                if (fout) {
                    fwrite(impl_->hm3w_header.data(), 1, impl_->hm3w_header.size(), fout);
                    char buf[65536];
                    size_t n;
                    while ((n = fread(buf, 1, sizeof(buf), ftmp)) > 0)
                        fwrite(buf, 1, n, fout);
                    fclose(fout);
                }
                fclose(ftmp);
            }
            std::remove(impl_->mpq_tmp_path.c_str());
            impl_->mpq_tmp_path.clear();
        }
    }
}

bool Archive::is_open() const {
    return impl_ && impl_->handle != nullptr;
}

// --- Read ---

bool Archive::open_read(const std::string& path) {
    close();
    impl_->file_path = path;

    DWORD flags = MPQ_OPEN_READ_ONLY;
    if (!SFileOpenArchive(path.c_str(), 0, flags, &impl_->handle)) {
        impl_->handle = nullptr;
        return false;
    }
    return true;
}

// --- Write ---

bool Archive::open_write(const std::string& path,
                          const std::string& map_name,
                          int32_t map_flags,
                          int32_t player_count,
                          int32_t file_count)
{
    close();
    impl_->file_path = path;

    // Build HM3W header padded to 0x200 bytes so StormLib can find the
    // MPQ header at offset 0x200 when reopening (it scans at 0x200-aligned offsets)
    {
        auto raw = make_hm3w_header(map_name, map_flags, player_count);
        impl_->hm3w_header.assign(0x200, 0);
        size_t copy_size = std::min(raw.size(), impl_->hm3w_header.size());
        std::memcpy(impl_->hm3w_header.data(), raw.data(), copy_size);
    }

    // Create MPQ archive to a temp file, wrapping to final path in close()
    impl_->mpq_tmp_path = path + ".tmp";
    SFILE_CREATE_MPQ create_info = {};
    create_info.cbSize         = sizeof(SFILE_CREATE_MPQ);
    create_info.dwMpqVersion   = MPQ_FORMAT_VERSION_1;
    create_info.pvUserData     = nullptr;
    create_info.cbUserData     = 0;
    create_info.dwStreamFlags  = 0; // STREAM_PROVIDER_FLAT | BASE_PROVIDER_FILE
    create_info.dwFileFlags1   = MPQ_FILE_DEFAULT_INTERNAL; // Create (listfile)
    create_info.dwFileFlags2   = MPQ_FILE_DEFAULT_INTERNAL; // Create (attributes)
    create_info.dwFileFlags3   = MPQ_FILE_DEFAULT_INTERNAL; // Create (signature)
    create_info.dwAttrFlags    = MPQ_ATTRIBUTE_CRC32 | MPQ_ATTRIBUTE_FILETIME | MPQ_ATTRIBUTE_MD5;
    create_info.dwSectorSize   = 0x10000;
    create_info.dwRawChunkSize = 0;
    create_info.dwMaxFileCount = std::max(file_count, 10) + 3;

    if (!SFileCreateArchive2(impl_->mpq_tmp_path.c_str(), &create_info, &impl_->handle)) {
        impl_->handle = nullptr;
        return false;
    }
    return true;
}

// --- File operations ---

bool Archive::has_file(const std::string& name) const {
    if (!impl_->handle) return false;
    HANDLE h = nullptr;
    if (SFileOpenFileEx(impl_->handle, name.c_str(), SFILE_OPEN_FROM_MPQ, &h)) {
        SFileCloseFile(h);
        return true;
    }
    return false;
}

int64_t Archive::file_size(const std::string& name) const {
    if (!impl_->handle) return -1;

    HANDLE h = nullptr;
    if (!SFileOpenFileEx(impl_->handle, name.c_str(), SFILE_OPEN_FROM_MPQ, &h))
        return -1;

    DWORD size = SFileGetFileSize(h, nullptr);
    SFileCloseFile(h);

    return (size == SFILE_INVALID_SIZE) ? -1 : static_cast<int64_t>(size);
}

std::vector<uint8_t> Archive::read_file(const std::string& name) {
    std::vector<uint8_t> result;
    if (!impl_->handle) return result;

    HANDLE h = nullptr;
    if (!SFileOpenFileEx(impl_->handle, name.c_str(), SFILE_OPEN_FROM_MPQ, &h))
        return result;

    DWORD size = SFileGetFileSize(h, nullptr);
    if (size == 0 || size == SFILE_INVALID_SIZE) {
        SFileCloseFile(h);
        return result;
    }

    result.resize(size);
    DWORD read = 0;
    bool ok = SFileReadFile(h, result.data(), size, &read, nullptr);
    SFileCloseFile(h);

    if (!ok || read != size) {
        result.clear();
    }
    return result;
}

bool Archive::write_file(const std::string& name, const void* data, size_t size, bool encrypt) {
    if (!impl_->handle) return false;

    uint32_t flags = MPQ_FILE_COMPRESS | MPQ_FILE_REPLACEEXISTING;
    if (encrypt)
        flags |= MPQ_FILE_ENCRYPTED;

    HANDLE hFile = nullptr;
    if (!SFileCreateFile(impl_->handle, name.c_str(), 0, static_cast<DWORD>(size), 0, flags, &hFile)) {
        return false;
    }

    bool ok = SFileWriteFile(hFile, data, static_cast<DWORD>(size), MPQ_COMPRESSION_ZLIB);
    if (!SFileFinishFile(hFile)) {
        ok = false;
    }

    return ok;
}

std::vector<std::string> Archive::list_files() const {
    std::vector<std::string> files;
    if (!impl_->handle) return files;

    HANDLE h = nullptr;
    if (!SFileOpenFileEx(impl_->handle, "(listfile)", SFILE_OPEN_FROM_MPQ, &h))
        return files;

    DWORD size = SFileGetFileSize(h, nullptr);
    if (size == SFILE_INVALID_SIZE || size == 0) {
        SFileCloseFile(h);
        return files;
    }

    std::string text(size, '\0');
    DWORD read = 0;
    if (SFileReadFile(h, text.data(), size, &read, nullptr)) {
        std::istringstream stream(text);
        std::string line;
        while (std::getline(stream, line)) {
            if (!line.empty() && line.back() == '\r')
                line.pop_back();
            if (!line.empty())
                files.push_back(line);
        }
    }
    SFileCloseFile(h);

    // Sort (w3x convention: case-insensitive sort)
    std::sort(files.begin(), files.end(), [](const std::string& a, const std::string& b) {
        std::string al, bl;
        al.resize(a.size()); bl.resize(b.size());
        std::transform(a.begin(), a.end(), al.begin(), ::tolower);
        std::transform(b.begin(), b.end(), bl.begin(), ::tolower);
        return al < bl;
    });

    return files;
}

std::vector<std::string> Archive::list_all_files() const {
    std::vector<std::string> files;
    if (!impl_->handle) return files;

    SFILE_FIND_DATA fd = {};
    HANDLE hFind = SFileFindFirstFile(impl_->handle, "*", &fd, nullptr);
    if (hFind && hFind != INVALID_HANDLE_VALUE) {
        do {
            files.push_back(fd.cFileName);
        } while (SFileFindNextFile(hFind, &fd));
        SFileFindClose(hFind);
    }

    std::sort(files.begin(), files.end(), [](const std::string& a, const std::string& b) {
        std::string al, bl;
        al.resize(a.size()); bl.resize(b.size());
        std::transform(a.begin(), a.end(), al.begin(), ::tolower);
        std::transform(b.begin(), b.end(), bl.begin(), ::tolower);
        return al < bl;
    });

    return files;
}

// --- HM3W helpers ---

std::vector<uint8_t> Archive::make_hm3w_header(const std::string& map_name,
                                                int32_t map_flags,
                                                int32_t player_count)
{
    Buffer buf;
    buf.write_u32(HM3W_MAGIC);     // "HM3W"
    buf.write_u32(0);              // version
    buf.write_string(map_name);
    buf.write_i32(map_flags);
    buf.write_i32(player_count);
    return buf.take_data();
}

bool Archive::parse_hm3w_header(const uint8_t* data, size_t size,
                                 std::string& out_map_name,
                                 int32_t& out_flags,
                                 int32_t& out_players)
{
    if (size < HM3W_MIN_SIZE) return false;

    Buffer buf(std::vector<uint8_t>(data, data + size));

    uint32_t magic = buf.read_u32();
    if (magic != HM3W_MAGIC) return false;

    /*uint32_t version =*/ buf.read_u32();
    out_map_name = buf.read_string();
    out_flags    = buf.read_i32();
    out_players  = buf.read_i32();
    return true;
}
