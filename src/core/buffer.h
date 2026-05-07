#pragma once

#include "types.h"
#include <vector>
#include <string>
#include <cstdint>

// ============================================================
// Buffer — little-endian binary serialization
// ============================================================
class Buffer {
public:
    Buffer() = default;
    explicit Buffer(std::vector<uint8_t> data);

    // --- Read ---
    uint8_t     read_u8();
    uint16_t    read_u16();
    uint32_t    read_u32();
    int32_t     read_i32();
    float       read_f32();
    ObjectID    read_object_id();
    std::string read_string();       // null-terminated
    void        read_raw(void* dst, size_t len);
    void        skip(size_t len);

    // --- Write ---
    void write_u8(uint8_t v);
    void write_u16(uint16_t v);
    void write_u32(uint32_t v);
    void write_i32(int32_t v);
    void write_f32(float v);
    void write_object_id(ObjectID id);
    void write_string(const std::string& s);   // null-terminated
    void write_raw(const void* src, size_t len);

    // --- State ---
    size_t tell() const { return pos_; }
    void   seek(size_t pos);
    bool   eof() const { return pos_ >= data_.size(); }
    size_t size() const { return data_.size(); }
    const uint8_t* data() const { return data_.data(); }

    // --- Build ---
    std::vector<uint8_t> take_data() { pos_ = 0; return std::move(data_); }
    void clear() { data_.clear(); pos_ = 0; }

private:
    std::vector<uint8_t> data_;
    size_t pos_ = 0;

    void ensure_read(size_t n) const;
    void ensure_write(size_t n);
};
