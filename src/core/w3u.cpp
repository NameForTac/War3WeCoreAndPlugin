#include "w3u.h"

namespace w3u {

static Modification read_mod(Buffer& buf, int extra_ints) {
    Modification m;
    m.mod_id   = buf.read_object_id();
    m.type     = static_cast<ValueType>(buf.read_u32());

    // Read extra ints (level/variation/data_ptr) according to file type
    m.level    = (extra_ints >= 1) ? buf.read_i32() : 0;
    m.data_ptr = (extra_ints >= 2) ? buf.read_i32() : 0;

    switch (m.type) {
    case ValueType::Int:
    case ValueType::Unreal:
    case ValueType::Bool:
    case ValueType::Char:
        m.value = buf.read_i32();
        break;
    case ValueType::Real:
        m.value = buf.read_f32();
        break;
    case ValueType::String:
        m.value = buf.read_string();
        break;
    default:
        // Unknown type — try reading as int (safe fallback)
        m.value = buf.read_i32();
        break;
    }

    buf.read_i32(); // terminator (0)
    return m;
}

static void write_mod(Buffer& buf, const Modification& m, int extra_ints) {
    buf.write_object_id(m.mod_id);
    buf.write_u32(static_cast<uint32_t>(m.type));

    // Write extra ints according to file type
    if (extra_ints >= 1) buf.write_i32(m.level);
    if (extra_ints >= 2) buf.write_i32(m.data_ptr);

    switch (m.type) {
    case ValueType::Int:
    case ValueType::Unreal:
    case ValueType::Bool:
    case ValueType::Char:
        buf.write_i32(std::get<int32_t>(m.value));
        break;
    case ValueType::Real:
        buf.write_f32(std::get<float>(m.value));
        break;
    case ValueType::String:
        buf.write_string(std::get<std::string>(m.value));
        break;
    default:
        buf.write_i32(std::get<int32_t>(m.value));
        break;
    }
    buf.write_i32(0); // terminator
}

static ObjectEntry read_entry(Buffer& buf, int extra_ints) {
    ObjectEntry e;
    e.original_id = buf.read_object_id();
    e.new_id      = buf.read_object_id();
    int32_t count = buf.read_i32();
    e.mods.reserve(count);
    for (int i = 0; i < count; ++i)
        e.mods.push_back(read_mod(buf, extra_ints));
    return e;
}

static void write_entry(Buffer& buf, const ObjectEntry& e, int extra_ints) {
    buf.write_object_id(e.original_id);
    buf.write_object_id(e.new_id);
    buf.write_i32((int32_t)e.mods.size());
    for (auto& m : e.mods)
        write_mod(buf, m, extra_ints);
}

ObjectFile read(Buffer& buf, ObjectFileType type) {
    ObjectFile obj;
    obj.version = buf.read_u32();
    obj.file_type = type;
    int extra = object_file_extra_ints(type);

    // Original objects
    {
        int32_t count = buf.read_i32();
        obj.original_objects.reserve(count);
        for (int i = 0; i < count; ++i)
            obj.original_objects.push_back(read_entry(buf, extra));
    }

    // Custom objects
    {
        int32_t count = buf.read_i32();
        obj.custom_objects.reserve(count);
        for (int i = 0; i < count; ++i)
            obj.custom_objects.push_back(read_entry(buf, extra));
    }

    return obj;
}

void write(Buffer& buf, const ObjectFile& obj) {
    int extra = object_file_extra_ints(obj.file_type);

    buf.write_u32(obj.version);

    buf.write_i32((int32_t)obj.original_objects.size());
    for (auto& e : obj.original_objects)
        write_entry(buf, e, extra);

    buf.write_i32((int32_t)obj.custom_objects.size());
    for (auto& e : obj.custom_objects)
        write_entry(buf, e, extra);
}

} // namespace w3u
