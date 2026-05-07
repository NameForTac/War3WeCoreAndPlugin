#include "buffer.h"
#include <cstring>
#include <stdexcept>

Buffer::Buffer(std::vector<uint8_t> data) : data_(std::move(data)) {}

void Buffer::ensure_read(size_t n) const {
    if (pos_ + n > data_.size())
        throw std::out_of_range("Buffer::read: out of range");
}

void Buffer::ensure_write(size_t n) {
    if (pos_ + n > data_.size())
        data_.resize(pos_ + n);
}

// --- Read ---

uint8_t Buffer::read_u8() {
    ensure_read(1);
    return data_[pos_++];
}

uint16_t Buffer::read_u16() {
    ensure_read(2);
    uint16_t v = data_[pos_] | (uint16_t(data_[pos_ + 1]) << 8);
    pos_ += 2;
    return v;
}

uint32_t Buffer::read_u32() {
    ensure_read(4);
    uint32_t v = data_[pos_]
               | (uint32_t(data_[pos_ + 1]) << 8)
               | (uint32_t(data_[pos_ + 2]) << 16)
               | (uint32_t(data_[pos_ + 3]) << 24);
    pos_ += 4;
    return v;
}

int32_t Buffer::read_i32() { return static_cast<int32_t>(read_u32()); }

float Buffer::read_f32() {
    uint32_t bits = read_u32();
    float v;
    memcpy(&v, &bits, 4);
    return v;
}

ObjectID Buffer::read_object_id() {
    ensure_read(4);
    ObjectID id;
    id.code[0] = static_cast<char>(data_[pos_]);
    id.code[1] = static_cast<char>(data_[pos_ + 1]);
    id.code[2] = static_cast<char>(data_[pos_ + 2]);
    id.code[3] = static_cast<char>(data_[pos_ + 3]);
    pos_ += 4;
    return id;
}

std::string Buffer::read_string() {
    std::string s;
    while (!eof()) {
        char c = static_cast<char>(read_u8());
        if (c == '\0') break;
        s += c;
    }
    return s;
}

void Buffer::read_raw(void* dst, size_t len) {
    ensure_read(len);
    memcpy(dst, &data_[pos_], len);
    pos_ += len;
}

void Buffer::skip(size_t len) {
    ensure_read(len);
    pos_ += len;
}

// --- Write ---

void Buffer::write_u8(uint8_t v) {
    ensure_write(1);
    data_[pos_++] = v;
}

void Buffer::write_u16(uint16_t v) {
    ensure_write(2);
    data_[pos_]       = static_cast<uint8_t>(v & 0xFF);
    data_[pos_ + 1]   = static_cast<uint8_t>((v >> 8) & 0xFF);
    pos_ += 2;
}

void Buffer::write_u32(uint32_t v) {
    ensure_write(4);
    data_[pos_]       = static_cast<uint8_t>(v & 0xFF);
    data_[pos_ + 1]   = static_cast<uint8_t>((v >> 8) & 0xFF);
    data_[pos_ + 2]   = static_cast<uint8_t>((v >> 16) & 0xFF);
    data_[pos_ + 3]   = static_cast<uint8_t>((v >> 24) & 0xFF);
    pos_ += 4;
}

void Buffer::write_i32(int32_t v) { write_u32(static_cast<uint32_t>(v)); }

void Buffer::write_f32(float v) {
    uint32_t bits;
    memcpy(&bits, &v, 4);
    write_u32(bits);
}

void Buffer::write_object_id(ObjectID id) {
    write_raw(id.code, 4);
}

void Buffer::write_string(const std::string& s) {
    write_raw(s.data(), s.size());
    write_u8(0);  // null terminator
}

void Buffer::write_raw(const void* src, size_t len) {
    ensure_write(len);
    memcpy(&data_[pos_], src, len);
    pos_ += len;
}

// --- State ---

void Buffer::seek(size_t pos) {
    if (pos > data_.size())
        throw std::out_of_range("Buffer::seek: out of range");
    pos_ = pos;
}
