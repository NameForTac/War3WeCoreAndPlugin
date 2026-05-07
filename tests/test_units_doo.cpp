#include "../src/core/units_doo.h"
#include <cmath>

void test_units_doo() {
    PlacedUnits original;
    original.version = 8;
    original.subversion = 0x0B;

    // Unit 1: a simple footman
    PlacedUnit footman;
    footman.type_id = "hfoo";
    footman.variation = 0;
    footman.x = 100.0f; footman.y = 200.0f; footman.z = 0.0f;
    footman.rotation = 0.0f;
    footman.scale_x = footman.scale_y = footman.scale_z = 1.0f;
    footman.type_id2 = "hfoo";
    footman.flags = 2;
    footman.owner = 0; // player 1
    footman.hp = -1;
    footman.mp = -1;
    footman.item_table_ptr = -1;
    footman.hero_level = 0;
    footman.strength = footman.agility = footman.intelligence = 0;
    footman.custom_color = 0;
    footman.portal_target = -1;
    footman.creation_number = 1;
    original.units.push_back(footman);

    // Unit 2: a hero with inventory and skills
    PlacedUnit hero;
    hero.type_id = "Hblm";
    hero.variation = 0;
    hero.x = 500.0f; hero.y = 500.0f; hero.z = 0.0f;
    hero.rotation = 1.0f;
    hero.scale_x = hero.scale_y = hero.scale_z = 1.0f;
    hero.type_id2 = "Hblm";
    hero.flags = 2;
    hero.owner = 0;
    hero.hp = -1;
    hero.mp = -1;
    hero.hero_level = 5;
    hero.strength = 20;
    hero.agility = 15;
    hero.intelligence = 25;

    // Inventory: 2 items
    {
        InventoryItem item;
        item.slot = 0; item.item_id = "ofir";
        hero.inventory.push_back(item);
        item.slot = 1; item.item_id = "pghe";
        hero.inventory.push_back(item);
    }

    // Skills: 1 ability
    {
        AbilityMod ab;
        ab.ability_id = "AHtb";
        ab.autocast = true;
        ab.level = 1;
        hero.skills.push_back(ab);
    }

    hero.custom_color = 0xFFFFFFFF;
    hero.portal_target = -1;
    hero.creation_number = 2;
    original.units.push_back(hero);

    // Unit 3: random unit table
    PlacedUnit random;
    random.type_id = "uDNR";
    random.x = 300.0f; random.y = 300.0f; random.z = 0.0f;
    random.rotation = 0.0f;
    random.scale_x = random.scale_y = random.scale_z = 1.0f;
    random.type_id2 = "uDNR";
    random.flags = 2;
    random.owner = 16; // neutral passive
    random.hp = -1;
    random.mp = -1;
    random.hero_level = 0;
    random.custom_color = 0;
    random.portal_target = -1;
    random.creation_number = 3;
    random.random.flags = 2; // table
    {
        RandomUnitData::TableEntry e;
        e.unit_id = "hfoo"; e.chance = 50;
        random.random.table.push_back(e);
        e.unit_id = "hrif"; e.chance = 50;
        random.random.table.push_back(e);
    }
    original.units.push_back(random);

    // Write
    Buffer buf;
    units_doo::write(buf, original);
    auto data = buf.take_data();

    // Read back
    Buffer read_buf(std::move(data));
    auto result = units_doo::read(read_buf);

    // Verify
    if (result.version != 8)          throw "version";
    if (result.subversion != 0x0B)    throw "subversion";
    if (result.units.size() != 3)     throw "unit count";

    // Check footman
    auto& u0 = result.units[0];
    if (u0.type_id != "hfoo")         throw "u0 type_id";
    if (std::fabs(u0.x - 100.0f) > 0.001f) throw "u0 x";
    if (std::fabs(u0.y - 200.0f) > 0.001f) throw "u0 y";
    if (u0.owner != 0)                throw "u0 owner";
    if (u0.hp != -1)                  throw "u0 hp";
    if (u0.creation_number != 1)      throw "u0 creation_number";

    // Check hero
    auto& u1 = result.units[1];
    if (u1.type_id != "Hblm")         throw "u1 type_id";
    if (u1.hero_level != 5)           throw "u1 hero_level";
    if (u1.strength != 20)            throw "u1 strength";
    if (u1.agility != 15)             throw "u1 agility";
    if (u1.intelligence != 25)        throw "u1 intelligence";
    if (u1.inventory.size() != 2)     throw "u1 inventory count";
    if (u1.inventory[0].slot != 0)    throw "u1 inv slot0";
    if (u1.inventory[0].item_id != "ofir") throw "u1 inv item0";
    if (u1.inventory[1].slot != 1)    throw "u1 inv slot1";
    if (u1.inventory[1].item_id != "pghe") throw "u1 inv item1";
    if (u1.skills.size() != 1)        throw "u1 skills count";
    if (u1.skills[0].ability_id != "AHtb") throw "u1 skill id";
    if (!u1.skills[0].autocast)       throw "u1 skill autocast";
    if (u1.skills[0].level != 1)      throw "u1 skill level";

    // Check random unit
    auto& u2 = result.units[2];
    if (u2.type_id != "uDNR")         throw "u2 type_id";
    if (u2.owner != 16)               throw "u2 owner";
    if (u2.random.flags != 2)         throw "u2 random flags";
    if (u2.random.table.size() != 2)  throw "u2 random table count";
    if (u2.random.table[0].unit_id != "hfoo") throw "u2 random entry0";
    if (u2.random.table[0].chance != 50)      throw "u2 random chance0";

    if (!read_buf.eof()) throw "not all data consumed";
}
