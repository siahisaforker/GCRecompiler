#pragma once

#include <gcrecomp/analysis/ir.h>
#include <gcrecomp/analysis/instruction.h>
#include <gcrecomp/analysis/cfg.h>

namespace gcrecomp {

class Lifter {
public:
    Lifter() = default;

    // Lifts a single basic block into an IR block
    IRBlock liftBlock(const BasicBlock& block);

private:
    // Helper to lift a single instruction
    void liftInstruction(const Instruction& instr, std::vector<IRInstruction>& out);
};

} // namespace gcrecomp
