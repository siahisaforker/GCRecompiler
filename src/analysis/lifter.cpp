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

    u32 rs = rd; // RD and RS occupy the same bits (6-10)

    // Simplified lifting for core PPC instructions
    switch (op) {
        case 4: { // Paired-Single Arithmetic
            u32 xop = (raw >> 1) & 0x1F;
            u32 frc = (raw >> 6) & 0x1F;
            switch (xop) {
                case 18: out.push_back({ IROp::FDiv,  { IROperand::Reg(rd), IROperand::Reg(ra), IROperand::Reg(rb) } }); break;
                case 20: out.push_back({ IROp::FSub,  { IROperand::Reg(rd), IROperand::Reg(ra), IROperand::Reg(rb) } }); break;
                case 21: out.push_back({ IROp::FAdd,  { IROperand::Reg(rd), IROperand::Reg(ra), IROperand::Reg(rb) } }); break;
                case 25: out.push_back({ IROp::FMul,  { IROperand::Reg(rd), IROperand::Reg(ra), IROperand::Reg(rb) } }); break;
                case 28: out.push_back({ IROp::FMsub, { IROperand::Reg(rd), IROperand::Reg(ra), IROperand::Reg(rb), IROperand::Reg(frc) } }); break;
                case 29: out.push_back({ IROp::FMadd, { IROperand::Reg(rd), IROperand::Reg(ra), IROperand::Reg(rb), IROperand::Reg(frc) } }); break;
                case 23: out.push_back({ IROp::FSel,  { IROperand::Reg(rd), IROperand::Reg(ra), IROperand::Reg(rb), IROperand::Reg(frc) } }); break;
            }
            break;
        }
        case 3: // twi
            out.push_back({ IROp::Trap, { IROperand::Imm(rd), IROperand::Reg(ra), IROperand::Imm((u32)simm) } });
            break;
        case 7: // mulli
            out.push_back({ IROp::Mul, { IROperand::Reg(rd), IROperand::Reg(ra), IROperand::Imm((u32)simm) } });
            break;
        case 8: // subfic
            out.push_back({ IROp::Sub, { IROperand::Reg(rd), IROperand::Imm((u32)simm), IROperand::Reg(ra) } });
            break;
        case 10: // cmpli
            out.push_back({ IROp::Cmpl, { IROperand::Reg(ra), IROperand::Imm(imm) } });
            break;
        case 11: // cmpi
            out.push_back({ IROp::Cmp, { IROperand::Reg(ra), IROperand::Imm((u32)simm) } });
            break;
        case 14: // addi
            if (ra == 0) { // li
                out.push_back({ IROp::SetImm, { IROperand::Reg(rd), IROperand::Imm((u32)simm) } });
            } else {
                out.push_back({ IROp::Add, { IROperand::Reg(rd), IROperand::Reg(ra), IROperand::Imm((u32)simm) } });
            }
            break;
        case 12: // addic
            out.push_back({ IROp::Addic, { IROperand::Reg(rd), IROperand::Reg(ra), IROperand::Imm((u32)simm) } });
            break;
        case 13: // addic.
            out.push_back({ IROp::Addic, { IROperand::Reg(rd), IROperand::Reg(ra), IROperand::Imm((u32)simm) } });
            out.push_back({ IROp::Cmp,   { IROperand::Reg(rd), IROperand::Imm(0) } }); // Update CR0
            break;
        case 15: // addis
            if (ra == 0) { // lis
                out.push_back({ IROp::SetImm, { IROperand::Reg(rd), IROperand::Imm((u32)simm << 16) } });
            } else {
                out.push_back({ IROp::Add, { IROperand::Reg(rd), IROperand::Reg(ra), IROperand::Imm((u32)simm << 16) } });
            }
            break;
        case 17: // sc
            out.push_back({ IROp::Syscall, {} });
            break;
        case 19: { // rfi, bclr, bcctr, CR logic
            u32 xop = (raw >> 1) & 0x3FF;
            if (instr.type == InstructionType::Return) {
                out.push_back({ IROp::Return });
            } else if (instr.type == InstructionType::Call) {
                out.push_back({ IROp::CallIndirect, { IROperand::Imm(instr.raw) } });
            } else if (instr.type == InstructionType::Branch) {
                out.push_back({ IROp::BranchIndirect, { IROperand::Imm(instr.raw) } });
            } else if (xop == 257) { // crand
                out.push_back({ IROp::And, { IROperand::Reg(rd), IROperand::Reg(ra), IROperand::Reg(rb) } });
            } else if (xop == 449) { // cror
                out.push_back({ IROp::Or, { IROperand::Reg(rd), IROperand::Reg(ra), IROperand::Reg(rb) } });
            } else if (xop == 193) { // crxor
                out.push_back({ IROp::Xor, { IROperand::Reg(rd), IROperand::Reg(ra), IROperand::Reg(rb) } });
            } else if (xop == 33) { // crnor
                out.push_back({ IROp::Nor, { IROperand::Reg(rd), IROperand::Reg(ra), IROperand::Reg(rb) } });
            }
            break;
        }
        case 20: { // rlwimi
            u32 sh = (raw >> 11) & 0x1F;
            u32 mb = (raw >> 6) & 0x1F;
            u32 me = (raw >> 1) & 0x1F;
            out.push_back({ IROp::Rlwimi, { IROperand::Reg(ra), IROperand::Reg(rs), IROperand::Imm(sh), IROperand::Imm(mb), IROperand::Imm(me) } });
            break;
        }
        case 21: { // rlwinm
            u32 sh = (raw >> 11) & 0x1F;
            u32 mb = (raw >> 6) & 0x1F;
            u32 me = (raw >> 1) & 0x1F;
            out.push_back({ IROp::Rol, { IROperand::Reg(ra), IROperand::Reg(rs), IROperand::Imm(sh) } });
            out.push_back({ IROp::Mask, { IROperand::Reg(ra), IROperand::Reg(ra), IROperand::Imm(mb), IROperand::Imm(me) } });
            break;
        }
        case 23: { // rlwnm
            u32 mb = (raw >> 6) & 0x1F;
            u32 me = (raw >> 1) & 0x1F;
            out.push_back({ IROp::Rol, { IROperand::Reg(ra), IROperand::Reg(rs), IROperand::Reg(rb) } });
            out.push_back({ IROp::Mask, { IROperand::Reg(ra), IROperand::Reg(ra), IROperand::Imm(mb), IROperand::Imm(me) } });
            break;
        }
        case 24: // ori
            out.push_back({ IROp::Or, { IROperand::Reg(ra), IROperand::Reg(rs), IROperand::Imm(imm) } });
            break;
        case 25: // oris
            out.push_back({ IROp::Or, { IROperand::Reg(ra), IROperand::Reg(rs), IROperand::Imm(imm << 16) } });
            break;
        case 26: // xori
            out.push_back({ IROp::Xor, { IROperand::Reg(ra), IROperand::Reg(rs), IROperand::Imm(imm) } });
            break;
        case 27: // xoris
            out.push_back({ IROp::Xor, { IROperand::Reg(ra), IROperand::Reg(rs), IROperand::Imm(imm << 16) } });
            break;
        case 28: // andi.
            out.push_back({ IROp::And, { IROperand::Reg(ra), IROperand::Reg(rs), IROperand::Imm(imm) } });
            break;
        case 29: // andis.
            out.push_back({ IROp::And, { IROperand::Reg(ra), IROperand::Reg(rs), IROperand::Imm(imm << 16) } });
            break;
        case 31: { // X-form
            u32 xop = (raw >> 1) & 0x3FF;
            switch (xop) {
                // Integer Arithmetic/Logic (Existing)
                case 266: out.push_back({ IROp::Add, { IROperand::Reg(rd), IROperand::Reg(ra), IROperand::Reg(rb) } }); break;
                case 40:  out.push_back({ IROp::Sub, { IROperand::Reg(rd), IROperand::Reg(rb), IROperand::Reg(ra) } }); break;
                case 235: out.push_back({ IROp::Mul, { IROperand::Reg(rd), IROperand::Reg(ra), IROperand::Reg(rb) } }); break;
                case 491: out.push_back({ IROp::Div, { IROperand::Reg(rd), IROperand::Reg(ra), IROperand::Reg(rb) } }); break;
                case 28:  out.push_back({ IROp::And, { IROperand::Reg(ra), IROperand::Reg(rs), IROperand::Reg(rb) } }); break;
                case 444: out.push_back({ IROp::Or,  { IROperand::Reg(ra), IROperand::Reg(rs), IROperand::Reg(rb) } }); break;
                case 316: out.push_back({ IROp::Xor, { IROperand::Reg(ra), IROperand::Reg(rs), IROperand::Reg(rb) } }); break;
                case 124: out.push_back({ IROp::Nor, { IROperand::Reg(ra), IROperand::Reg(rs), IROperand::Reg(rb) } }); break;
                case 24:  out.push_back({ IROp::Shl, { IROperand::Reg(ra), IROperand::Reg(rs), IROperand::Reg(rb) } }); break;
                case 536: out.push_back({ IROp::Shr, { IROperand::Reg(ra), IROperand::Reg(rs), IROperand::Reg(rb) } }); break;
                case 792: out.push_back({ IROp::Sar, { IROperand::Reg(ra), IROperand::Reg(rs), IROperand::Reg(rb) } }); break;
                case 824: out.push_back({ IROp::Sar, { IROperand::Reg(ra), IROperand::Reg(rs), IROperand::Imm(rb) } }); break;
                case 954: out.push_back({ IROp::Extsb, { IROperand::Reg(ra), IROperand::Reg(rs) } }); break;
                case 922: out.push_back({ IROp::Extsh, { IROperand::Reg(ra), IROperand::Reg(rs) } }); break;
                case 26:  out.push_back({ IROp::Cntlzw, { IROperand::Reg(ra), IROperand::Reg(rs) } }); break;
                case 60:  out.push_back({ IROp::Andc,   { IROperand::Reg(ra), IROperand::Reg(rs), IROperand::Reg(rb) } }); break;
                case 412: out.push_back({ IROp::Orc,    { IROperand::Reg(ra), IROperand::Reg(rs), IROperand::Reg(rb) } }); break;
                case 476: out.push_back({ IROp::Nand,   { IROperand::Reg(ra), IROperand::Reg(rs), IROperand::Reg(rb) } }); break;
                case 104: out.push_back({ IROp::Sub,    { IROperand::Reg(rd), IROperand::Imm(0), IROperand::Reg(ra) } }); break; // neg

                // Special Purpose Registers
                case 339: out.push_back({ IROp::Mfspr, { IROperand::Reg(rd), IROperand::Imm((rb << 5) | ra) } }); break;
                case 467: out.push_back({ IROp::Mtspr, { IROperand::Imm((rb << 5) | ra), IROperand::Reg(rd) } }); break;

                // Condition Register
                case 19:  out.push_back({ IROp::Mfcr,  { IROperand::Reg(rd) } }); break;
                case 144: out.push_back({ IROp::Mtcrf, { IROperand::Imm(rd), IROperand::Reg(rs) } }); break;
                
                // System / Atomic
                case 20:  out.push_back({ IROp::ReservationLoad, { IROperand::Reg(rd), IROperand::Reg(ra), IROperand::Reg(rb) } }); break;
                case 150: out.push_back({ IROp::ReservationStore, { IROperand::Reg(rs), IROperand::Reg(ra), IROperand::Reg(rb) } }); break;
                case 4:   out.push_back({ IROp::Trap, { IROperand::Imm(rd), IROperand::Reg(ra), IROperand::Reg(rb) } }); break;
                case 854: out.push_back({ IROp::Sync, {} }); break; // eieio is a sync

                // Load Indexed
                case 87:  out.push_back({ IROp::Load8,    { IROperand::Reg(rd), IROperand::Reg(ra), IROperand::Reg(rb) } }); break;
                case 279: out.push_back({ IROp::Load16,   { IROperand::Reg(rd), IROperand::Reg(ra), IROperand::Reg(rb) } }); break;
                case 343: out.push_back({ IROp::Load16a,  { IROperand::Reg(rd), IROperand::Reg(ra), IROperand::Reg(rb) } }); break;
                case 23:  out.push_back({ IROp::Load32,   { IROperand::Reg(rd), IROperand::Reg(ra), IROperand::Reg(rb) } }); break;
                
                // Load Indexed with Update
                case 119: out.push_back({ IROp::Load8,    { IROperand::Reg(rd), IROperand::Reg(ra), IROperand::Reg(rb) } });
                          out.push_back({ IROp::Add,      { IROperand::Reg(ra), IROperand::Reg(ra), IROperand::Reg(rb) } }); break;
                case 311: out.push_back({ IROp::Load16,   { IROperand::Reg(rd), IROperand::Reg(ra), IROperand::Reg(rb) } });
                          out.push_back({ IROp::Add,      { IROperand::Reg(ra), IROperand::Reg(ra), IROperand::Reg(rb) } }); break;
                case 375: out.push_back({ IROp::Load16a,  { IROperand::Reg(rd), IROperand::Reg(ra), IROperand::Reg(rb) } });
                          out.push_back({ IROp::Add,      { IROperand::Reg(ra), IROperand::Reg(ra), IROperand::Reg(rb) } }); break;
                case 55:  out.push_back({ IROp::Load32,   { IROperand::Reg(rd), IROperand::Reg(ra), IROperand::Reg(rb) } });
                          out.push_back({ IROp::Add,      { IROperand::Reg(ra), IROperand::Reg(ra), IROperand::Reg(rb) } }); break;

                // Store Indexed
                case 215: out.push_back({ IROp::Store8,   { IROperand::Reg(rs), IROperand::Reg(ra), IROperand::Reg(rb) } }); break;
                case 407: out.push_back({ IROp::Store16,  { IROperand::Reg(rs), IROperand::Reg(ra), IROperand::Reg(rb) } }); break;
                case 151: out.push_back({ IROp::Store32,  { IROperand::Reg(rs), IROperand::Reg(ra), IROperand::Reg(rb) } }); break;

                // Store Indexed with Update
                case 247: out.push_back({ IROp::Store8,   { IROperand::Reg(rs), IROperand::Reg(ra), IROperand::Reg(rb) } });
                          out.push_back({ IROp::Add,      { IROperand::Reg(ra), IROperand::Reg(ra), IROperand::Reg(rb) } }); break;
                case 439: out.push_back({ IROp::Store16,  { IROperand::Reg(rs), IROperand::Reg(ra), IROperand::Reg(rb) } });
                          out.push_back({ IROp::Add,      { IROperand::Reg(ra), IROperand::Reg(ra), IROperand::Reg(rb) } }); break;
                case 183: out.push_back({ IROp::Store32,  { IROperand::Reg(rs), IROperand::Reg(ra), IROperand::Reg(rb) } });
                          out.push_back({ IROp::Add,      { IROperand::Reg(ra), IROperand::Reg(ra), IROperand::Reg(rb) } }); break;

                // Cache Management
                case 86:  out.push_back({ IROp::Sync,    { IROperand::Reg(ra), IROperand::Reg(rb) } }); break; // dcbf
                case 54:  out.push_back({ IROp::Sync,    { IROperand::Reg(ra), IROperand::Reg(rb) } }); break; // dcbst
                case 278: out.push_back({ IROp::Sync,    { IROperand::Reg(ra), IROperand::Reg(rb) } }); break; // dcbt
                case 246: out.push_back({ IROp::Sync,    { IROperand::Reg(ra), IROperand::Reg(rb) } }); break; // dcbtst
                case 1014: out.push_back({ IROp::Isync,  { IROperand::Reg(ra), IROperand::Reg(rb) } }); break; // dcbz
                case 982: out.push_back({ IROp::Isync,   { IROperand::Reg(ra), IROperand::Reg(rb) } }); break; // icbi
            }
            break;
        }
        case 32: // lwz
            out.push_back({ IROp::Load32, { IROperand::Reg(rd), IROperand::Reg(ra), IROperand::Imm((u32)simm) } });
            break;
        case 33: // lwzu
            out.push_back({ IROp::Load32, { IROperand::Reg(rd), IROperand::Reg(ra), IROperand::Imm((u32)simm) } });
            out.push_back({ IROp::Add,    { IROperand::Reg(ra), IROperand::Reg(ra), IROperand::Imm((u32)simm) } });
            break;
        case 34: // lbz
            out.push_back({ IROp::Load8, { IROperand::Reg(rd), IROperand::Reg(ra), IROperand::Imm((u32)simm) } });
            break;
        case 35: // lbzu
            out.push_back({ IROp::Load8, { IROperand::Reg(rd), IROperand::Reg(ra), IROperand::Imm((u32)simm) } });
            out.push_back({ IROp::Add,   { IROperand::Reg(ra), IROperand::Reg(ra), IROperand::Imm((u32)simm) } });
            break;
        case 36: // stw
            out.push_back({ IROp::Store32, { IROperand::Reg(rd), IROperand::Reg(ra), IROperand::Imm((u32)simm) } });
            break;
        case 37: // stwu
            out.push_back({ IROp::Store32, { IROperand::Reg(rd), IROperand::Reg(ra), IROperand::Imm((u32)simm) } });
            out.push_back({ IROp::Add,    { IROperand::Reg(ra), IROperand::Reg(ra), IROperand::Imm((u32)simm) } });
            break;
        case 38: // stb
            out.push_back({ IROp::Store8, { IROperand::Reg(rd), IROperand::Reg(ra), IROperand::Imm((u32)simm) } });
            break;
        case 39: // stbu
            out.push_back({ IROp::Store8, { IROperand::Reg(rd), IROperand::Reg(ra), IROperand::Imm((u32)simm) } });
            out.push_back({ IROp::Add,   { IROperand::Reg(ra), IROperand::Reg(ra), IROperand::Imm((u32)simm) } });
            break;
        case 40: // lhz
            out.push_back({ IROp::Load16, { IROperand::Reg(rd), IROperand::Reg(ra), IROperand::Imm((u32)simm) } });
            break;
        case 41: // lhzu
            out.push_back({ IROp::Load16, { IROperand::Reg(rd), IROperand::Reg(ra), IROperand::Imm((u32)simm) } });
            out.push_back({ IROp::Add,    { IROperand::Reg(ra), IROperand::Reg(ra), IROperand::Imm((u32)simm) } });
            break;
        case 42: // lha
            out.push_back({ IROp::Load16a, { IROperand::Reg(rd), IROperand::Reg(ra), IROperand::Imm((u32)simm) } });
            break;
        case 43: // lhau
            out.push_back({ IROp::Load16a, { IROperand::Reg(rd), IROperand::Reg(ra), IROperand::Imm((u32)simm) } });
            out.push_back({ IROp::Add,     { IROperand::Reg(ra), IROperand::Reg(ra), IROperand::Imm((u32)simm) } });
            break;
        case 45: // sthu
            out.push_back({ IROp::Store16, { IROperand::Reg(rd), IROperand::Reg(ra), IROperand::Imm((u32)simm) } });
            out.push_back({ IROp::Add,     { IROperand::Reg(ra), IROperand::Reg(ra), IROperand::Imm((u32)simm) } });
            break;
        case 46: // lmw
            out.push_back({ IROp::Lmw, { IROperand::Reg(rd), IROperand::Reg(ra), IROperand::Imm((u32)simm) } });
            break;
        case 47: // stmw
            out.push_back({ IROp::Stmw, { IROperand::Reg(rd), IROperand::Reg(ra), IROperand::Imm((u32)simm) } });
            break;
        case 48: // lfs
            out.push_back({ IROp::LoadFloat, { IROperand::Reg(rd), IROperand::Reg(ra), IROperand::Imm((u32)simm) } });
            break;
        case 49: // lfsu
            out.push_back({ IROp::LoadFloat, { IROperand::Reg(rd), IROperand::Reg(ra), IROperand::Imm((u32)simm) } });
            out.push_back({ IROp::Add,       { IROperand::Reg(ra), IROperand::Reg(ra), IROperand::Imm((u32)simm) } });
            break;
        case 50: // lfd
            out.push_back({ IROp::LoadDouble, { IROperand::Reg(rd), IROperand::Reg(ra), IROperand::Imm((u32)simm) } });
            break;
        case 51: // lfdu
            out.push_back({ IROp::LoadDouble, { IROperand::Reg(rd), IROperand::Reg(ra), IROperand::Imm((u32)simm) } });
            out.push_back({ IROp::Add,        { IROperand::Reg(ra), IROperand::Reg(ra), IROperand::Imm((u32)simm) } });
            break;
        case 52: // stfs
            out.push_back({ IROp::StoreFloat, { IROperand::Reg(rd), IROperand::Reg(ra), IROperand::Imm((u32)simm) } });
            break;
        case 53: // stfsu
            out.push_back({ IROp::StoreFloat, { IROperand::Reg(rd), IROperand::Reg(ra), IROperand::Imm((u32)simm) } });
            out.push_back({ IROp::Add,        { IROperand::Reg(ra), IROperand::Reg(ra), IROperand::Imm((u32)simm) } });
            break;
        case 54: // stfd
            out.push_back({ IROp::StoreDouble, { IROperand::Reg(rd), IROperand::Reg(ra), IROperand::Imm((u32)simm) } });
            break;
        case 55: // stfdu
            out.push_back({ IROp::StoreDouble, { IROperand::Reg(rd), IROperand::Reg(ra), IROperand::Imm((u32)simm) } });
            out.push_back({ IROp::Add,         { IROperand::Reg(ra), IROperand::Reg(ra), IROperand::Imm((u32)simm) } });
            break;
        case 56: // psq_l
            out.push_back({ IROp::LoadDouble, { IROperand::Reg(rd), IROperand::Reg(ra), IROperand::Imm((u32)simm) } });
            break;
        case 57: // psq_lux
            out.push_back({ IROp::LoadDouble, { IROperand::Reg(rd), IROperand::Reg(ra), IROperand::Reg(rb) } });
            out.push_back({ IROp::Add,        { IROperand::Reg(ra), IROperand::Reg(ra), IROperand::Reg(rb) } });
            break;
        case 60: // psq_st
            out.push_back({ IROp::StoreDouble, { IROperand::Reg(rd), IROperand::Reg(ra), IROperand::Imm((u32)simm) } });
            break;
        case 61: // psq_stux
            out.push_back({ IROp::StoreDouble, { IROperand::Reg(rd), IROperand::Reg(ra), IROperand::Reg(rb) } });
            out.push_back({ IROp::Add,         { IROperand::Reg(ra), IROperand::Reg(ra), IROperand::Reg(rb) } });
            break;
        case 59: { // Single precision FPU
            u32 xop = (raw >> 1) & 0x1F;
            u32 frc = (raw >> 6) & 0x1F;
            switch (xop) {
                case 18: out.push_back({ IROp::FDiv,  { IROperand::Reg(rd), IROperand::Reg(ra), IROperand::Reg(rb) } }); break;
                case 20: out.push_back({ IROp::FSub,  { IROperand::Reg(rd), IROperand::Reg(ra), IROperand::Reg(rb) } }); break;
                case 21: out.push_back({ IROp::FAdd,  { IROperand::Reg(rd), IROperand::Reg(ra), IROperand::Reg(rb) } }); break;
                case 25: out.push_back({ IROp::FMul,  { IROperand::Reg(rd), IROperand::Reg(ra), IROperand::Reg(rb) } }); break;
                case 28: out.push_back({ IROp::FMsub, { IROperand::Reg(rd), IROperand::Reg(ra), IROperand::Reg(rb), IROperand::Reg(frc) } }); break;
                case 29: out.push_back({ IROp::FMadd, { IROperand::Reg(rd), IROperand::Reg(ra), IROperand::Reg(rb), IROperand::Reg(frc) } }); break;
            }
            break;
        }
        case 63: { // Double precision FPU / Misc
            u32 xop = (raw >> 1) & 0x3FF;
            u32 frc = (raw >> 6) & 0x1F;
            if ((xop & 0x1F) == 18) { out.push_back({ IROp::FDiv, { IROperand::Reg(rd), IROperand::Reg(ra), IROperand::Reg(rb) } }); break; }
            if ((xop & 0x1F) == 20) { out.push_back({ IROp::FSub, { IROperand::Reg(rd), IROperand::Reg(ra), IROperand::Reg(rb) } }); break; }
            if ((xop & 0x1F) == 21) { out.push_back({ IROp::FAdd, { IROperand::Reg(rd), IROperand::Reg(ra), IROperand::Reg(rb) } }); break; }
            if ((xop & 0x1F) == 25) { out.push_back({ IROp::FMul, { IROperand::Reg(rd), IROperand::Reg(ra), IROperand::Reg(rb) } }); break; }
            if ((xop & 0x1F) == 28) { out.push_back({ IROp::FMsub, { IROperand::Reg(rd), IROperand::Reg(ra), IROperand::Reg(rb), IROperand::Reg(frc) } }); break; }
            if ((xop & 0x1F) == 29) { out.push_back({ IROp::FMadd, { IROperand::Reg(rd), IROperand::Reg(ra), IROperand::Reg(rb), IROperand::Reg(frc) } }); break; }
            
            switch (xop) {
                case 23:  out.push_back({ IROp::FSel,  { IROperand::Reg(rd), IROperand::Reg(ra), IROperand::Reg(rb), IROperand::Reg(frc) } }); break;
                case 72:  out.push_back({ IROp::SetReg, { IROperand::Reg(rd), IROperand::Reg(rb) } }); break; // fmr
                case 12:  out.push_back({ IROp::Frsp,  { IROperand::Reg(rd), IROperand::Reg(rb) } }); break;
                case 15:  out.push_back({ IROp::Fctiw, { IROperand::Reg(rd), IROperand::Reg(rb) } }); break;
            }
            break;
        }
        
        // Control flow
        case 16: // bc
            out.push_back({ IROp::BranchCond, { IROperand::Imm(rd), IROperand::Imm(ra), IROperand::Addr(instr.branchTarget) } });
            break;
        case 18: // b/bl
            if (instr.isLink) {
                 out.push_back({ IROp::Call, { IROperand::Addr(instr.branchTarget) } });
            } else {
                 out.push_back({ IROp::Branch, { IROperand::Addr(instr.branchTarget) } });
            }
            break;
    }
}

} // namespace gcrecomp
