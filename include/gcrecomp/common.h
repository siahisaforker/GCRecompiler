#pragma once

#include <bit>
#include <gcrecomp/types.h>

namespace gcrecomp {

// GameCube is Big-Endian. PC is typically Little-Endian.
// We manually swap bytes to ensure correctness.
// Big-Endian reads are no-ops on Big-Endian machines, so update later if we
// actually end up wanting to support Big-Endian PCs.
inline u32 swap32(u32 val) {
  return ((val >> 24) & 0xff) | ((val << 8) & 0xff0000) |
         ((val >> 8) & 0xff00) | ((val << 24) & 0xff000000);
}

inline u16 swap16(u16 val) {
  return ((val >> 8) & 0xff) | ((val << 8) & 0xff00);
}

inline u32 read_be32(const u8 *data) {
  return (static_cast<u32>(data[0]) << 24) | (static_cast<u32>(data[1]) << 16) |
         (static_cast<u32>(data[2]) << 8) | (static_cast<u32>(data[3]));
}

inline u16 read_be16(const u8 *data) {
  return (static_cast<u16>(data[0]) << 8) | (static_cast<u16>(data[1]));
}

} // namespace gcrecomp
