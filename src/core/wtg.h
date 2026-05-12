#pragma once

#include "buffer.h"
#include <memory>
#include <string>
#include <vector>

// ============================================================
// war3map.wtg — GUI Trigger names / definitions (version 7)
// ============================================================

struct TriggerCategory {
    int32_t index = 0;
    std::string name;
    int32_t type = 0; // 0=normal, 1=comment
};

struct TriggerVariable {
    std::string name;
    std::string type;
    int32_t unknown = 0;
    int32_t is_array = 0;
    int32_t array_size = 0;
    int32_t init_flag = 0;
    std::string init_value;
};

// Forward declaration for recursive ECA structure
struct TriggerParam;
struct TriggerECA {
    int32_t type = 0;       // 0=event, 1=condition, 2=action
    std::string name;        // function name
    int32_t enabled = 1;
    int32_t unknown = 0;
    std::vector<TriggerParam> params;
    std::vector<TriggerECA> children;
};

struct TriggerParam {
    int32_t type = 4; // 0=preset, 1=string, 2=int, 3=real, 4=empty, 5=variable, 6=function

    std::string str_val;               // types 1, 5
    int32_t int_val = 0;               // type 2
    float   float_val = 0;             // type 3
    std::vector<TriggerParam> sub_params; // type 0 (nested preset values)
    std::unique_ptr<TriggerECA> func_eca; // type 6 (function call sub-ECA)

    ~TriggerParam();
    TriggerParam() = default;
    TriggerParam(TriggerParam&&) noexcept;
    TriggerParam& operator=(TriggerParam&&) noexcept;
    TriggerParam(const TriggerParam&);
    TriggerParam& operator=(const TriggerParam&);
};

struct TriggerDef {
    std::string name;
    std::string description;
    int32_t type = 0;
    int32_t enabled = 1;
    int32_t custom_text = 0;
    int32_t initial_state = 0;
    int32_t run_on_map_init = 0;
    int32_t category_index = 0;
    std::vector<TriggerECA> ecas;
};

struct TriggerData {
    uint32_t version = 7;
    std::vector<TriggerCategory> categories;
    int32_t unknown_middle = 0; // int32 field between categories and variables
    std::vector<TriggerVariable> variables;
    std::vector<TriggerDef> triggers;
};

namespace wtg {

TriggerData read(Buffer& buf);
void write(Buffer& buf, const TriggerData& td);

// Low-level ECA read/write (exposed for recursive use)
TriggerECA read_eca(Buffer& buf);
void write_eca(Buffer& buf, const TriggerECA& eca);

} // namespace wtg
