#include "object_id_gen.h"

void ObjectIDGenerator::seed_with(const ObjectFile& file) {
    for (const auto& entry : file.original_objects) {
        seed_id(entry.original_id);
        if (!entry.new_id.is_null())
            seed_id(entry.new_id);
    }
    for (const auto& entry : file.custom_objects) {
        seed_id(entry.original_id);
        if (!entry.new_id.is_null())
            seed_id(entry.new_id);
    }
}

void ObjectIDGenerator::seed_id(const ObjectID& id) {
    if (id.is_null()) return;
    used_ids_.insert(id);
}

ObjectID ObjectIDGenerator::generate(const ObjectID& base_id) {
    char prefix = base_id.code[0];
    if (prefix == 0) prefix = 'X';
    return generate(prefix);
}

ObjectID ObjectIDGenerator::generate(char prefix) {
    for (int counter = 0; counter < 1000; ++counter) {
        ObjectID id = make_id(prefix, counter);
        if (used_ids_.find(id) == used_ids_.end()) {
            used_ids_.insert(id);
            return id;
        }
    }
    // All 1000 IDs for this prefix are already used — fallback
    ObjectID id{};
    id.code[0] = prefix;
    return id;
}

ObjectID ObjectIDGenerator::make_id(char prefix, int counter) {
    ObjectID id{};
    id.code[0] = prefix;
    id.code[1] = char('0' + ((counter / 100) % 10));
    id.code[2] = char('0' + ((counter / 10) % 10));
    id.code[3] = char('0' + (counter % 10));
    return id;
}

void ObjectIDGenerator::clear() {
    used_ids_.clear();
}
