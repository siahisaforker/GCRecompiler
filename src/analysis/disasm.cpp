#include <gcrecomp/analysis/disasm.h>
#include <gcrecomp/log.h>
#include <iomanip>
#include <sstream>

namespace gcrecomp {

bool Disassembler::disassemble(const Binary& binary, u32 addr, Instruction& out) {
    if (!binary.read32(addr, out.raw)) {
        return false;
    }

    out.address = addr;
    out.mnemonic = "unknown";
    out.operands = "";
    out.isBranch = false;
    out.branchTarget = 0;
    out.type = InstructionType::Unknown;

    u32 op = out.raw >> 26;
    
    // Branch (b, bl, ba, bla)
    if (op == 18) {
        out.isBranch = true;
        u32 li = (out.raw >> 2) & 0xFFFFFF;
        if (li & 0x800000) li |= 0xFF000000; // Sign extend
        bool aa = (out.raw >> 1) & 1;
        bool lk = out.raw & 1;

        out.branchTarget = aa ? (li << 2) : (addr + (li << 2));
        out.mnemonic = lk ? "bl" : "b";
        out.type = lk ? InstructionType::Call : InstructionType::Branch;
        
        std::stringstream ss;
        ss << "0x" << std::hex << out.branchTarget;
        out.operands = ss.str();
        return true;
    }

    // Branch Conditional (bc, bcl, bca, bcla)
    if (op == 16) {
        out.isBranch = true;
        u32 bd = (out.raw >> 2) & 0x3FFF;
        if (bd & 0x2000) bd |= 0xFFFFC000; // Sign extend
        bool aa = (out.raw >> 1) & 1;
        bool lk = out.raw & 1;
        
        out.branchTarget = aa ? (bd << 2) : (addr + (bd << 2));
        out.mnemonic = lk ? "bcl" : "bc";
        out.type = lk ? InstructionType::Call : InstructionType::Branch;
        
        std::stringstream ss;
        ss << "0x" << std::hex << out.branchTarget;
        out.operands = ss.str();
        return true;
    }

    // Branch to LR / CTR
    if (op == 19) {
        u32 xop = (out.raw >> 1) & 0x3FF;
        bool lk = out.raw & 1;
        
        if (xop == 16) { // bclr
            out.isBranch = true;
            out.mnemonic = lk ? "bclrl" : "bclr";
            out.type = InstructionType::Return; // Simplification: bclr is often blr
            return true;
        }
        if (xop == 528) { // bcctr
            out.isBranch = true;
            out.mnemonic = lk ? "bcctrl" : "bcctr";
            out.type = lk ? InstructionType::Call : InstructionType::Branch;
            return true;
        }
    }

    // Not a branch we care about for CFG (for now)
    return true;
}

} // namespace gcrecomp
