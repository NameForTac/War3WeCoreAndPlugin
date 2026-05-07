#include "../src/core/doo.h"
#include <cmath>

void test_doo() {
    // Build a PlacedDoodads, write, read back, verify

    PlacedDoodads original;
    original.version = 8;
    original.subversion = 0x0B;

    // Doodad 1: a simple tree
    PlacedDoodad tree;
    tree.type_id = "LTlt";
    tree.variation = 3;
    tree.x = 128.0f; tree.y = 256.0f; tree.z = 0.0f;
    tree.rotation = 1.57f;
    tree.scale_x = tree.scale_y = tree.scale_z = 1.0f;
    tree.flags = 2;
    tree.life_percent = 100;
    tree.item_table_ptr = -1;
    tree.editor_id = 42;
    original.doodads.push_back(tree);

    // Doodad 2: a crate with a drop table
    PlacedDoodad crate;
    crate.type_id = "LTcr";
    crate.variation = 0;
    crate.x = 512.0f; crate.y = 384.0f; crate.z = 0.0f;
    crate.rotation = 0.0f;
    crate.scale_x = crate.scale_y = crate.scale_z = 1.0f;
    crate.flags = 2;
    crate.life_percent = 100;
    crate.item_table_ptr = 0;
    {
        DropItemSet set;
        DropItem item;
        item.item_id = "pghe";
        item.chance = 60;
        set.items.push_back(item);
        item.item_id = "rde1";
        item.chance = 40;
        set.items.push_back(item);
        crate.item_sets.push_back(set);
    }
    crate.editor_id = 43;
    original.doodads.push_back(crate);

    // Special doodad
    SpecialDoodad sd;
    sd.type_id = "D00B";
    sd.x = 100.0f; sd.y = 200.0f; sd.z = 50.0f;
    original.special.push_back(sd);

    // Write
    Buffer buf;
    doo::write(buf, original);
    auto data = buf.take_data();

    // Read back
    Buffer read_buf(std::move(data));
    auto result = doo::read(read_buf);

    // Verify
    if (result.version != 8)            throw "version";
    if (result.subversion != 0x0B)      throw "subversion";
    if (result.doodads.size() != 2)     throw "doodad count";
    if (result.special.size() != 1)     throw "special count";

    if (result.doodads[0].type_id != "LTlt") throw "doodad0 type_id";
    if (result.doodads[0].variation != 3)    throw "doodad0 variation";
    if (std::fabs(result.doodads[0].x - 128.0f) > 0.001f) throw "doodad0 x";
    if (std::fabs(result.doodads[0].y - 256.0f) > 0.001f) throw "doodad0 y";
    if (result.doodads[0].flags != 2)        throw "doodad0 flags";
    if (result.doodads[0].life_percent != 100) throw "doodad0 life";
    if (result.doodads[0].editor_id != 42)   throw "doodad0 editor_id";
    if (result.doodads[0].item_table_ptr != -1) throw "doodad0 item_table_ptr";
    if (result.doodads[0].item_sets.size() != 0) throw "doodad0 item_sets";

    if (result.doodads[1].type_id != "LTcr") throw "doodad1 type_id";
    if (result.doodads[1].item_sets.size() != 1) throw "doodad1 item_sets count";
    if (result.doodads[1].item_sets[0].items.size() != 2) throw "doodad1 items count";
    if (result.doodads[1].item_sets[0].items[0].item_id != "pghe") throw "doodad1 item0";
    if (result.doodads[1].item_sets[0].items[0].chance != 60) throw "doodad1 chance0";
    if (result.doodads[1].item_sets[0].items[1].item_id != "rde1") throw "doodad1 item1";

    if (result.special[0].type_id != "D00B") throw "special type_id";
    if (std::fabs(result.special[0].x - 100.0f) > 0.001f) throw "special x";
    if (std::fabs(result.special[0].y - 200.0f) > 0.001f) throw "special y";
    if (std::fabs(result.special[0].z - 50.0f) > 0.001f)  throw "special z";

    if (!read_buf.eof()) throw "not all data consumed";
}
