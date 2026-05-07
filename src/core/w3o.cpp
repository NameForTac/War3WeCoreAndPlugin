#include "w3o.h"
#include "w3u.h"

namespace w3o {

void read(Buffer& buf, std::vector<std::pair<ObjectFileType, ObjectFile>>& out) {
    out.clear();
    /*uint32_t version =*/ buf.read_u32();

    static const ObjectFileType types[] = {
        ObjectFileType::Units,
        ObjectFileType::Items,
        ObjectFileType::Destructables,
        ObjectFileType::Doodads,
        ObjectFileType::Abilities,
        ObjectFileType::Buffs,
        ObjectFileType::Upgrades,
    };

    for (auto ft : types) {
        uint32_t present = buf.read_u32();
        if (present) {
            out.emplace_back(ft, w3u::read(buf, ft));
        }
    }
}

void write(Buffer& buf, const std::vector<std::pair<ObjectFileType, ObjectFile>>& sections) {
    buf.write_u32(1); // version

    static const ObjectFileType all_types[] = {
        ObjectFileType::Units,
        ObjectFileType::Items,
        ObjectFileType::Destructables,
        ObjectFileType::Doodads,
        ObjectFileType::Abilities,
        ObjectFileType::Buffs,
        ObjectFileType::Upgrades,
    };

    size_t idx = 0;
    for (auto ft : all_types) {
        // Find matching section
        auto it = std::find_if(sections.begin(), sections.end(),
            [ft](const auto& pair) { return pair.first == ft; });

        if (it != sections.end()) {
            buf.write_u32(1); // present
            w3u::write(buf, it->second);
        } else {
            buf.write_u32(0); // not present
        }
    }
}

} // namespace w3o
