#pragma once

#include <cstdint>
#include <vector>
#include <string>
#include <memory>

namespace gcrecomp {

using u8  = uint8_t;
using u16 = uint16_t;
using u32 = uint32_t;
using u64 = uint64_t;

using s8  = int8_t;
using s16 = int16_t;
using s32 = int32_t;
using s64 = int64_t;

struct Section {
    u32 address;
    u32 size;
    u32 fileOffset;
    bool isText;
};

struct MemoryRegion {
    u32 start;
    u32 size;
    std::vector<u8> data;

    bool contains(u32 addr) const {
        return addr >= start && addr < start + size;
    }
};

} // namespace gcrecomp
