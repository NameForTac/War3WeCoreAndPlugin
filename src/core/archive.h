#pragma once

#include "buffer.h"
#include <memory>
#include <string_view>

// ============================================================
// Archive — HM3W + StormLib MPQ wrapper
// ============================================================
class Archive {
public:
    Archive();
    ~Archive();

    // Non-copyable
    Archive(const Archive&) = delete;
    Archive& operator=(const Archive&) = delete;
    Archive(Archive&&) noexcept;
    Archive& operator=(Archive&&) noexcept;

    // --- Open / Close ---
    bool open_read(const std::string& path);
    bool open_write(const std::string& path,
                    const std::string& map_name,
                    int32_t map_flags,
                    int32_t player_count,
                    int32_t file_count);
    void close();

    bool is_open() const;

    // --- File operations ---
    bool                has_file(const std::string& name) const;
    int64_t             file_size(const std::string& name) const;
    std::vector<uint8_t> read_file(const std::string& name);
    bool                write_file(const std::string& name,
                                   const void* data, size_t size,
                                   bool encrypt = false);
    std::vector<std::string> list_files() const;
    std::vector<std::string> list_all_files() const;

    // --- HM3W helpers ---
    static std::vector<uint8_t> make_hm3w_header(const std::string& map_name,
                                                   int32_t map_flags,
                                                   int32_t player_count);
    static bool parse_hm3w_header(const uint8_t* data, size_t size,
                                  std::string& out_map_name,
                                  int32_t& out_flags,
                                  int32_t& out_players);

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};
