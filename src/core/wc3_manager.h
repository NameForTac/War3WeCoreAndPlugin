#pragma once

#include <string>
#include <vector>
#include <cstdint>

// ============================================================
// Wc3Manager — Loads War3.mpq / War3x.mpq from the WC3
// installation directory using LoadLibrary/GetProcAddress
// so callers don't need to link against StormLib directly.
//
// Thread safety: not thread-safe (intended for main/GUI thread).
// ============================================================
class Wc3Manager {
public:
    Wc3Manager();
    ~Wc3Manager();

    Wc3Manager(const Wc3Manager&) = delete;
    Wc3Manager& operator=(const Wc3Manager&) = delete;
    Wc3Manager(Wc3Manager&&) noexcept;
    Wc3Manager& operator=(Wc3Manager&&) noexcept;

    // Initialize with WC3 data directory. Opens War3.mpq and
    // War3x.mpq if they exist.  Safe to call multiple times
    // (re-closes and re-opens).
    bool initialize(const std::string& wc3_data_dir);

    // Close all open MPQ archives.
    void close();

    bool is_initialized() const { return initialized_; }
    const std::string& data_dir() const { return data_dir_; }

    // Read a file from WC3 archives.  Checks War3x.mpq first,
    // then War3.mpq.  Returns empty vector if not found.
    std::vector<uint8_t> read_file(const std::string& name) const;

    // Check if a file exists in any WC3 archive.
    bool has_file(const std::string& name) const;

private:
    bool load_stormlib();
    void free_stormlib();
    bool try_open_mpq(const std::string& path, void*& out_handle) const;

    bool initialized_ = false;
    std::string data_dir_;
    void* war3_mpq_ = nullptr;
    void* war3x_mpq_ = nullptr;

    // StormLib function pointers (loaded via LoadLibrary)
    void* storm_lib_ = nullptr;
    bool (*SFileOpenArchive_)(const char*, uint32_t, uint32_t, void**) = nullptr;
    bool (*SFileOpenFileEx_)(void*, const char*, uint32_t, void**) = nullptr;
    uint32_t (*SFileGetFileSize_)(void*, uint32_t*) = nullptr;
    bool (*SFileReadFile_)(void*, void*, uint32_t, uint32_t*, void*) = nullptr;
    bool (*SFileCloseArchive_)(void*) = nullptr;
    bool (*SFileCloseFile_)(void*) = nullptr;
};
