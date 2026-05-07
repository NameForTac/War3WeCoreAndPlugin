#pragma once

#include "types.h"
#include <set>

// ============================================================
// ObjectIDGenerator — auto-generates custom object IDs
//
// Rule: take the first character of the base ID + 3-digit
// counter, e.g. 'hfoo' → 'h000', 'h001', ...
// ============================================================
class ObjectIDGenerator {
public:
    // Seed with all existing IDs from an ObjectFile
    void seed_with(const ObjectFile& file);
    void seed_id(const ObjectID& id);

    // Generate a new ID based on a base object's prefix
    ObjectID generate(const ObjectID& base_id);

    // Generate the next ID for a given prefix character
    ObjectID generate(char prefix);

    void clear();

private:
    std::set<ObjectID> used_ids_;

    ObjectID make_id(char prefix, int counter);
};
