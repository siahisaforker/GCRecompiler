#pragma once

#include <gcrecomp/types.h>
#include <gcrecomp/loader/binary.h>
#include <gcrecomp/analysis/instruction.h>

namespace gcrecomp {

class Disassembler {
public:
    Disassembler() = default;
    ~Disassembler() = default;

    bool disassemble(const Binary& binary, u32 addr, Instruction& out);
};

} // namespace gcrecomp
