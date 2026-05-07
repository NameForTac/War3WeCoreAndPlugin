#include "../src/core/object_id_gen.h"
#include "../src/core/types.h"

void test_object_id_gen() {
    // ── Basic generation with prefix ──
    {
        ObjectIDGenerator gen;
        ObjectID id = gen.generate('h');
        if (id.code[0] != 'h') throw "prefix h";
        if (id.code[1] != '0' || id.code[2] != '0' || id.code[3] != '0')
            throw "h000 expected";
    }

    // ── Sequential generation ──
    {
        ObjectIDGenerator gen;
        auto a = gen.generate('h');
        auto b = gen.generate('h');
        auto c = gen.generate('h');
        if (a.str() != "h000") throw "first: h000";
        if (b.str() != "h001") throw "second: h001";
        if (c.str() != "h002") throw "third: h002";
    }

    // ── From base ObjectID ──
    {
        ObjectIDGenerator gen;
        ObjectID base = ObjectID::from_string("hfoo");
        ObjectID id = gen.generate(base);
        if (id.str() != "h000") throw "from hfoo: expected h000";
    }

    // ── Different prefixes are independent ──
    {
        ObjectIDGenerator gen;
        gen.generate('h');
        gen.generate('h');
        auto a = gen.generate('A');
        if (a.str() != "A000") throw "A prefix should start at 000";
        auto c = gen.generate('h');
        if (c.str() != "h002") throw "h should continue at 002";
    }

    // ── Seed with existing IDs — handles gaps correctly ──
    {
        ObjectIDGenerator gen;
        gen.seed_id(ObjectID::from_string("h000"));
        gen.seed_id(ObjectID::from_string("h001"));
        gen.seed_id(ObjectID::from_string("h005"));

        // Should pick the first unused slot: 002
        auto id = gen.generate('h');
        if (id.str() != "h002") throw "seeded: expected h002";

        id = gen.generate('h');
        if (id.str() != "h003") throw "seeded: expected h003";

        id = gen.generate('h');
        if (id.str() != "h004") throw "seeded: expected h004";

        id = gen.generate('h');
        if (id.str() != "h006") throw "seeded: expected h006";
    }

    // ── Seed with ObjectFile ──
    {
        ObjectFile file;
        file.file_type = ObjectFileType::Units;

        ObjectEntry e1;
        e1.original_id = ObjectID::from_string("hfoo");
        e1.new_id = ObjectID::from_string("h000");
        file.custom_objects.push_back(e1);

        ObjectEntry e2;
        e2.original_id = ObjectID::from_string("hfoo");
        e2.new_id = ObjectID::from_string("h001");
        file.custom_objects.push_back(e2);

        ObjectIDGenerator gen;
        gen.seed_with(file);

        auto id = gen.generate('h');
        if (id.str() != "h002") throw "from ObjectFile: expected h002";
    }

    // ── Clear resets state ──
    {
        ObjectIDGenerator gen;
        gen.generate('h');
        gen.generate('h');
        gen.clear();
        auto id = gen.generate('h');
        if (id.str() != "h000") throw "after clear: expected h000";
    }

    // ── Non-digit IDs (e.g. hero codes like 'Hamg') don't occupy number slots ──
    {
        ObjectIDGenerator gen;
        gen.seed_id(ObjectID::from_string("Hamg"));

        auto id = gen.generate('H');
        if (id.str() != "H000") throw "non-digit seed should not affect counter";
    }
}
