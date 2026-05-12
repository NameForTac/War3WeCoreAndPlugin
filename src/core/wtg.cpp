#include "wtg.h"
#include <stdexcept>

// ============================================================
// TriggerParam special member functions
// (structs are in global namespace, free functions in wtg::)
// ============================================================
TriggerParam::~TriggerParam() = default;
TriggerParam::TriggerParam(TriggerParam&&) noexcept = default;
TriggerParam& TriggerParam::operator=(TriggerParam&&) noexcept = default;

TriggerParam::TriggerParam(const TriggerParam& o)
    : type(o.type)
    , str_val(o.str_val)
    , int_val(o.int_val)
    , float_val(o.float_val)
{
    if (o.func_eca)
        func_eca = std::make_unique<TriggerECA>(*o.func_eca);
    sub_params.reserve(o.sub_params.size());
    for (auto& p : o.sub_params)
        sub_params.push_back(p);
}

TriggerParam& TriggerParam::operator=(const TriggerParam& o) {
    if (this != &o) {
        type = o.type;
        str_val = o.str_val;
        int_val = o.int_val;
        float_val = o.float_val;
        if (o.func_eca)
            func_eca = std::make_unique<TriggerECA>(*o.func_eca);
        else
            func_eca.reset();
        sub_params.clear();
        sub_params.reserve(o.sub_params.size());
        for (auto& p : o.sub_params)
            sub_params.push_back(p);
    }
    return *this;
}

namespace wtg {

// ============================================================
// Forward declarations for mutually recursive read functions
// ============================================================
TriggerECA read_eca(Buffer& buf);
TriggerParam read_param(Buffer& buf);
void write_eca(Buffer& buf, const TriggerECA& eca);
void write_param(Buffer& buf, const TriggerParam& p);

// ============================================================
// ECA parameter reading
//
// Each parameter is self-delimiting:
//   type 0 (preset) — has sub-params
//   type 1 (string) — null-terminated string
//   type 2 (int)    — int32
//   type 3 (real)   — float
//   type 4 (empty)  — no value
//   type 5 (var)    — null-terminated string
//   type 6 (func)   — full sub-ECA
// ============================================================

// Try to parse remaining data as ECA tail: unknown(i4) + count(i4) + children.
// Uses a BUFFER COPY so the original is unmodified on failure.
bool try_parse_eca_tail(Buffer& src, int32_t& out_unknown,
                               std::vector<TriggerECA>& out_children)
{
    if (src.eof()) {
        out_unknown = 0;
        out_children.clear();
        return true;
    }
    try {
        Buffer copy(src); // deep copy — snapshot of seek + data state
        out_unknown = copy.read_i32();
        int32_t count = copy.read_i32();
        if (count < 0 || count > 65536)
            return false;
        out_children.clear();
        out_children.reserve(count);
        for (int32_t i = 0; i < count; ++i)
            out_children.push_back(read_eca(copy));
        return true;
    } catch (...) {
        return false;
    }
}

TriggerParam read_param(Buffer& buf) {
    TriggerParam p;
    p.type = buf.read_i32();
    switch (p.type) {
    case 0: // preset — has nested sub-params
        while (true) {
            if (buf.eof()) break;
            size_t saved_pos = buf.tell();
            int32_t dummy_unknown;
            std::vector<TriggerECA> dummy_children;
            if (try_parse_eca_tail(buf, dummy_unknown, dummy_children)) {
                // Remaining data is the tail, not a sub-param
                break;
            }
            // Not a tail — read as sub-param
            buf.seek(saved_pos);
            p.sub_params.push_back(read_param(buf));
        }
        break;
    case 1: // string
    case 5: // variable
        p.str_val = buf.read_string();
        break;
    case 2: // integer
        p.int_val = buf.read_i32();
        break;
    case 3: // real
        p.float_val = buf.read_f32();
        break;
    case 4: // empty
        break;
    case 6: // function call — sub-ECA
        p.func_eca = std::make_unique<TriggerECA>(read_eca(buf));
        break;
    default:
        throw std::runtime_error("wtg: unknown param type " + std::to_string(p.type));
    }
    return p;
}

void write_param(Buffer& buf, const TriggerParam& p) {
    buf.write_i32(p.type);
    switch (p.type) {
    case 0:
        for (auto& sp : p.sub_params)
            write_param(buf, sp);
        break;
    case 1:
    case 5:
        buf.write_string(p.str_val);
        break;
    case 2:
        buf.write_i32(p.int_val);
        break;
    case 3:
        buf.write_f32(p.float_val);
        break;
    case 4:
        break;
    case 6:
        if (p.func_eca)
            write_eca(buf, *p.func_eca);
        break;
    }
}

// ============================================================
// ECA read/write
// ============================================================
TriggerECA read_eca(Buffer& buf) {
    TriggerECA eca;
    eca.type    = buf.read_i32();
    eca.name    = buf.read_string();
    eca.enabled = buf.read_i32();

    // Read params until remaining data forms a valid tail
    // (unknown(i4) + children_count(i4) + children)
    while (true) {
        if (buf.eof()) break;
        size_t saved_pos = buf.tell();
        int32_t unknown;
        std::vector<TriggerECA> children;
        if (try_parse_eca_tail(buf, unknown, children)) {
            // This IS the tail — consume it
            buf.seek(saved_pos);
            eca.unknown = buf.read_i32();
            int32_t count = buf.read_i32();
            if (count > 0) {
                eca.children.reserve(count);
                for (int32_t i = 0; i < count; ++i)
                    eca.children.push_back(read_eca(buf));
            }
            break;
        }
        // Not a tail — read as param
        buf.seek(saved_pos);
        eca.params.push_back(read_param(buf));
    }

    return eca;
}

void write_eca(Buffer& buf, const TriggerECA& eca) {
    buf.write_i32(eca.type);
    buf.write_string(eca.name);
    buf.write_i32(eca.enabled);

    // Write params
    for (auto& p : eca.params)
        write_param(buf, p);

    // Write tail: unknown + children_count + children
    buf.write_i32(eca.unknown);
    buf.write_i32(static_cast<int32_t>(eca.children.size()));
    for (auto& child : eca.children)
        write_eca(buf, child);
}

// ============================================================
// Top-level WTG read/write
// ============================================================
TriggerData read(Buffer& buf) {
    // Magic "WTG!"
    uint32_t magic = buf.read_u32();
    if (magic != 0x21475457)
        throw std::runtime_error("wtg: bad magic");

    TriggerData td;
    td.version = buf.read_u32();

    // Categories
    uint32_t cat_count = buf.read_u32();
    td.categories.reserve(cat_count);
    for (uint32_t i = 0; i < cat_count; ++i) {
        TriggerCategory cat;
        cat.index = buf.read_i32();
        cat.name  = buf.read_string();
        cat.type  = buf.read_i32();
        td.categories.push_back(std::move(cat));
    }

    // Unknown field between categories and variables
    td.unknown_middle = buf.read_i32();

    // Variables
    uint32_t var_count = buf.read_u32();
    td.variables.reserve(var_count);
    for (uint32_t i = 0; i < var_count; ++i) {
        TriggerVariable v;
        v.name       = buf.read_string();
        v.type       = buf.read_string();
        v.unknown    = buf.read_i32();
        v.is_array   = buf.read_i32();
        v.array_size = buf.read_i32();
        v.init_flag  = buf.read_i32();
        v.init_value = buf.read_string();
        td.variables.push_back(std::move(v));
    }

    // Triggers
    uint32_t trig_count = buf.read_u32();
    td.triggers.reserve(trig_count);
    for (uint32_t i = 0; i < trig_count; ++i) {
        TriggerDef t;
        t.name            = buf.read_string();
        t.description     = buf.read_string();
        t.type            = buf.read_i32();
        t.enabled         = buf.read_i32();
        t.custom_text     = buf.read_i32();
        t.initial_state   = buf.read_i32();
        t.run_on_map_init = buf.read_i32();
        t.category_index  = buf.read_i32();

        uint32_t eca_count = buf.read_u32();
        t.ecas.reserve(eca_count);
        for (uint32_t j = 0; j < eca_count; ++j)
            t.ecas.push_back(read_eca(buf));

        td.triggers.push_back(std::move(t));
    }

    return td;
}

void write(Buffer& buf, const TriggerData& td) {
    buf.write_u32(0x21475457); // "WTG!"
    buf.write_u32(td.version);

    // Categories
    buf.write_u32(static_cast<uint32_t>(td.categories.size()));
    for (auto& cat : td.categories) {
        buf.write_i32(cat.index);
        buf.write_string(cat.name);
        buf.write_i32(cat.type);
    }

    // Unknown middle field
    buf.write_i32(td.unknown_middle);

    // Variables
    buf.write_u32(static_cast<uint32_t>(td.variables.size()));
    for (auto& v : td.variables) {
        buf.write_string(v.name);
        buf.write_string(v.type);
        buf.write_i32(v.unknown);
        buf.write_i32(v.is_array);
        buf.write_i32(v.array_size);
        buf.write_i32(v.init_flag);
        buf.write_string(v.init_value);
    }

    // Triggers
    buf.write_u32(static_cast<uint32_t>(td.triggers.size()));
    for (auto& t : td.triggers) {
        buf.write_string(t.name);
        buf.write_string(t.description);
        buf.write_i32(t.type);
        buf.write_i32(t.enabled);
        buf.write_i32(t.custom_text);
        buf.write_i32(t.initial_state);
        buf.write_i32(t.run_on_map_init);
        buf.write_i32(t.category_index);

        buf.write_u32(static_cast<uint32_t>(t.ecas.size()));
        for (auto& eca : t.ecas)
            write_eca(buf, eca);
    }
}

} // namespace wtg
