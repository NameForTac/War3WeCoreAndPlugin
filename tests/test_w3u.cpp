#include "../src/core/w3u.h"
#include <cstring>

void test_w3u() {
    // Build an object file, write, read back
    ObjectFile obj;
    obj.version = 2;

    // Original object
    ObjectEntry orig;
    orig.original_id = ObjectID::from_string("hfoo");
    // new_id = null means modify the original
    {
        Modification m;
        m.mod_id = ObjectID::from_string("uhot");
        m.type = ValueType::Int;
        m.level = 1;
        m.data_ptr = 0;
        m.value = int32_t(10);
        orig.mods.push_back(m);
    }
    {
        Modification m;
        m.mod_id = ObjectID::from_string("udes");
        m.type = ValueType::String;
        m.level = 0;
        m.data_ptr = 0;
        m.value = std::string("A description");
        orig.mods.push_back(m);
    }
    {
        Modification m;
        m.mod_id = ObjectID::from_string("uhdg");
        m.type = ValueType::Real;
        m.level = 0;
        m.data_ptr = 0;
        m.value = 3.5f;
        orig.mods.push_back(m);
    }
    obj.original_objects.push_back(orig);

    // Custom object
    ObjectEntry custom;
    custom.original_id = ObjectID::from_string("hfoo");
    custom.new_id = ObjectID::from_string("h001");
    {
        Modification m;
        m.mod_id = ObjectID::from_string("uhot");
        m.type = ValueType::Int;
        m.level = 1;
        m.data_ptr = 0;
        m.value = int32_t(50);
        custom.mods.push_back(m);
    }
    obj.custom_objects.push_back(custom);

    // Write
    Buffer w;
    w3u::write(w, obj);
    auto data = w.take_data();
    size_t orig_size = data.size();

    // Read back
    Buffer r(std::move(data));
    auto parsed = w3u::read(r);

    // Verify
    if (parsed.version != 2)
        throw std::runtime_error("version mismatch");

    if (parsed.original_objects.size() != 1)
        throw std::runtime_error("original_objects count mismatch");
    if (parsed.original_objects[0].original_id != ObjectID::from_string("hfoo"))
        throw std::runtime_error("orig object ID mismatch");
    if (parsed.original_objects[0].mods.size() != 3)
        throw std::runtime_error("orig mods count mismatch");
    if (parsed.original_objects[0].mods[0].int_val() != 10)
        throw std::runtime_error("orig int val mismatch");
    if (parsed.original_objects[0].mods[1].str_val() != "A description")
        throw std::runtime_error("orig string val mismatch");
    if (std::abs(parsed.original_objects[0].mods[2].float_val() - 3.5f) > 0.0001f)
        throw std::runtime_error("orig float val mismatch");

    if (parsed.custom_objects.size() != 1)
        throw std::runtime_error("custom_objects count mismatch");
    if (parsed.custom_objects[0].new_id != ObjectID::from_string("h001"))
        throw std::runtime_error("custom object new_id mismatch");
    if (parsed.custom_objects[0].mods[0].int_val() != 50)
        throw std::runtime_error("custom int val mismatch");

    // Roundtrip consistency check
    Buffer w2;
    w3u::write(w2, parsed);
    auto data2 = w2.take_data();
    if (data2.size() != orig_size)
        throw std::runtime_error("roundtrip size mismatch");
}
