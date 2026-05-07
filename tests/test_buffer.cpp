#include "../src/core/buffer.h"
#include <cstring>
#include <cmath>

void test_buffer() {
    // --- Write then read ---
    Buffer w;
    w.write_u8(0x42);
    w.write_u16(0x1234);
    w.write_u32(0xDEADBEEF);
    w.write_f32(3.14159f);
    w.write_string("hello");
    w.write_object_id(ObjectID::from_string("hfoo"));

    auto data = w.take_data();
    Buffer r(std::move(data));

    if (r.read_u8() != 0x42)
        throw std::runtime_error("u8 mismatch");

    if (r.read_u16() != 0x1234)
        throw std::runtime_error("u16 mismatch");

    if (r.read_u32() != 0xDEADBEEF)
        throw std::runtime_error("u32 mismatch");

    float pi = r.read_f32();
    if (std::abs(pi - 3.14159f) > 0.0001f)
        throw std::runtime_error("f32 mismatch");

    if (r.read_string() != "hello")
        throw std::runtime_error("string mismatch");

    if (r.read_object_id() != ObjectID::from_string("hfoo"))
        throw std::runtime_error("object_id mismatch");

    // --- ObjectID ---
    ObjectID id1 = ObjectID::from_string("A000");
    ObjectID id2 = ObjectID::from_string("A000");
    ObjectID id3 = ObjectID::from_string("B000");

    if (id1 != id2)
        throw std::runtime_error("ObjectID equality");
    if (!(id1 == id2))
        throw std::runtime_error("ObjectID operator==");
    if (id1.is_null())
        throw std::runtime_error("ObjectID should not be null");

    ObjectID null_id;
    if (!null_id.is_null())
        throw std::runtime_error("ObjectID should be null");

    // ObjectID roundtrip via to_raw/from_raw
    auto raw = id1.to_raw();
    ObjectID id4 = ObjectID::from_raw(raw);
    if (id1 != id4)
        throw std::runtime_error("ObjectID raw roundtrip");

    // ObjectID string
    if (id1.str() != "A000")
        throw std::runtime_error("ObjectID str()");

    // --- Seek/tell ---
    Buffer b;
    b.write_u32(0x12345678);
    b.write_u32(0x9ABCDEF0);
    b.seek(0);
    if (b.read_u32() != 0x12345678)
        throw std::runtime_error("seek/tell read1");
    if (b.read_u32() != 0x9ABCDEF0)
        throw std::runtime_error("seek/tell read2");
}
