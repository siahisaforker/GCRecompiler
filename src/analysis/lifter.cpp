#include <gcrecomp/analysis/lifter.h>
#include <gcrecomp/log.h>

namespace gcrecomp {

IRBlock Lifter::liftBlock(const BasicBlock& block) {
    IRBlock irBlock;
    irBlock.startAddr = block.startAddr;

    for (const auto& instr : block.instructions) {
        liftInstruction(instr, irBlock.instructions);
    }

    return irBlock;
}

void Lifter::liftInstruction(const Instruction& instr, std::vector<IRInstruction>& out) {
    u32 raw = instr.raw;
    u32 op = raw >> 26;
    u32 rd = (raw >> 21) & 0x1F;
    u32 ra = (raw >> 16) & 0x1F;
    u32 rb = (raw >> 11) & 0x1F;
    u16 imm = raw & 0xFFFF;
    s16 simm = (s16)imm;

    // Simplified lifting for core PPC instructions
    switch (op) {
        case 14: // addi
            if (ra == 0) { // li
                out.push_back({ IROp::SetImm, { IROperand::Reg(rd), IROperand::Imm((u32)simm) } });
            } else {
                out.push_back({ IROp::Add, { IROperand::Reg(rd), IROperand::Reg(ra), IROperand::Imm((u32)simm) } });
            }
            break;
        case 15: // addis
            if (ra == 0) { // lis
                out.push_back({ IROp::SetImm, { IROperand::Reg(rd), IROperand::Imm((u32)simm << 16) } });
            } else {
                out.push_back({ IROp::Add, { IROperand::Reg(rd), IROperand::Reg(ra), IROperand::Imm((u32)simm << 16) } });
            }
            break;
        case 31: { // Integer and other X-form ops
            u32 xop = (raw >> 1) & 0x3FF;
            if (xop == 266) { // add
                 out.push_back({ IROp::Add, { IROperand::Reg(rd), IROperand::Reg(ra), IROperand::Reg(rb) } });
            }
            break;
        }
        case 32: // lwz
            out.push_back({ IROp::Load32, { IROperand::Reg(rd), IROperand::Reg(ra), IROperand::Imm((u32)simm) } });
            break;
        case 36: // stw
            out.push_back({ IROp::Store32, { IROperand::Reg(rd), IROperand::Reg(ra), IROperand::Imm((u32)simm) } });
            break;
        
        // Control flow is handled by the block structure, 
        // but we can lift them for completeness or to help the emitter.
        case 18: // b/bl
            if (instr.isLink) {
                 out.push_back({ IROp::Call, { IROperand::Addr(instr.branchTarget) } });
            } else {
                 out.push_back({ IROp::Branch, { IROperand::Addr(instr.branchTarget) } });
            }
            break;
        case 19: { // rfi, bclr, etc.
             u32 xop = (raw >> 1) & 0x3FF;
             if (xop == 16) { // bclr
                 out.push_back({ IROp::Return, {} });
             }
             break;
        }
    }
}

} // namespace gcrecomp
