#include <gcrecomp/codegen/emitter.h>
#include <gcrecomp/analysis/cfg.h>
#include <gcrecomp/analysis/ir.h>
#include <gcrecomp/log.h>
#include <recomp_runtime.h>
#include <sstream>
#include <iomanip>

namespace gcrecomp {

Emitter::Emitter() {}
Emitter::~Emitter() {}

std::string Emitter::emitFunction(const Function& func, const ControlFlowGraph& cfg) {
    LOG_INFO("Emitter::emitFunction for %s", func.name.c_str());
    std::stringstream ss;
    ss << "// Function: " << func.name << "\n";
    ss << "void fn_0x" << std::hex << std::setw(8) << std::setfill('0') << func.startAddr << "(CPUContext* ctx) {\n";
    
    for (u32 blockAddr : func.blocks) {
        const BasicBlock* block = const_cast<ControlFlowGraph&>(cfg).getBlock(blockAddr);
        if (!block) continue;

        ss << "label_0x" << std::hex << blockAddr << ":\n";
        for (const auto& instr : block->irInstructions) {
            ss << "    " << emitInstruction(instr, func) << "\n";
        }
    }
    
    ss << "}\n";
    return ss.str();
}

std::string Emitter::emitInstruction(const IRInstruction& instr, const Function& func) {
    auto gpr = [&](int index) {
        if (instr.operands[index].type == IROperandType::Register) {
            return "ctx->gpr[" + std::to_string(instr.operands[index].value) + "]";
        }
        return operandToC(instr.operands[index]);
    };
    auto fpr = [&](int index) {
        if (instr.operands[index].type == IROperandType::Register) {
            return "ctx->fpr[" + std::to_string(instr.operands[index].value) + "]";
        }
        return operandToC(instr.operands[index]);
    };
    auto dest_gpr = [&](int index) { return "ctx->gpr[" + operandToC(instr.operands[index]) + "]"; };
    auto dest_fpr = [&](int index) { return "ctx->fpr[" + operandToC(instr.operands[index]) + "]"; };

    switch (instr.op) {
        case IROp::SetImm:    return dest_gpr(0) + " = " + operandToC(instr.operands[1]) + ";";
        case IROp::Add:       return dest_gpr(0) + " = " + gpr(1) + " + " + gpr(2) + ";";
        case IROp::Sub:       return dest_gpr(0) + " = " + gpr(1) + " - " + gpr(2) + ";";
        case IROp::Mul:       return dest_gpr(0) + " = " + gpr(1) + " * " + gpr(2) + ";";
        case IROp::Div:       return dest_gpr(0) + " = " + gpr(1) + " / " + gpr(2) + ";";
        case IROp::And:       return dest_gpr(0) + " = " + gpr(1) + " & " + gpr(2) + ";";
        case IROp::Or:        return dest_gpr(0) + " = " + gpr(1) + " | " + gpr(2) + ";";
        case IROp::Xor:       return dest_gpr(0) + " = " + gpr(1) + " ^ " + gpr(2) + ";";
        case IROp::Addic:     return dest_gpr(0) + " = ADDIC(ctx, " + gpr(1) + ", " + operandToC(instr.operands[2]) + ");";
        case IROp::Shl:       return dest_gpr(0) + " = " + gpr(1) + " << " + gpr(2) + ";";
        case IROp::Nor:       return dest_gpr(0) + " = ~(" + gpr(1) + " | " + gpr(2) + ");";
        case IROp::Andc:      return dest_gpr(0) + " = " + gpr(1) + " & ~" + gpr(2) + ";";
        case IROp::Orc:       return dest_gpr(0) + " = " + gpr(1) + " | ~" + gpr(2) + ";";
        case IROp::Nand:      return dest_gpr(0) + " = ~(" + gpr(1) + " & " + gpr(2) + ");";
        case IROp::Sar:       return dest_gpr(0) + " = (int32_t)" + gpr(1) + " >> " + gpr(2) + ";";
        
        case IROp::Rol:       return dest_gpr(0) + " = rotl32(" + gpr(1) + ", " + gpr(2) + ");";
        case IROp::Mask:      return dest_gpr(0) + " = get_mask(" + gpr(1) + ", " + gpr(2) + ");";
        case IROp::Rlwimi: {
            u32 sh = instr.operands[2].value;
            u32 mb = instr.operands[3].value;
            u32 me = instr.operands[4].value;
            return dest_gpr(0) + " = RLWIMI(" + gpr(0) + ", " + gpr(1) + ", " + std::to_string(sh) + ", " + std::to_string(mb) + ", " + std::to_string(me) + ");";
        }
        
        case IROp::Extsb:     return dest_gpr(0) + " = EXTSB(" + gpr(1) + ");";
        case IROp::Extsh:     return dest_gpr(0) + " = EXTSH(" + gpr(1) + ");";
        case IROp::Cntlzw:    return dest_gpr(0) + " = cntlzw(" + gpr(1) + ");";
        
        case IROp::Cmp:       return "set_cr_field(ctx, 0, " + gpr(0) + ", " + gpr(1) + ", 1);";
        case IROp::Cmpl:      return "set_cr_field(ctx, 0, " + gpr(0) + ", " + gpr(1) + ", 0);";
        
        case IROp::Mfcr:      return dest_gpr(0) + " = ctx->cr;";
        case IROp::Mtcrf:     return "ctx->cr = " + gpr(1) + "; // TODO: Mask";
        
        case IROp::Mfspr:     return dest_gpr(0) + " = get_spr(ctx, " + operandToC(instr.operands[1]) + ");";
        case IROp::Mtspr:     return "set_spr(ctx, " + operandToC(instr.operands[0]) + ", " + gpr(1) + ");";
        
        case IROp::Sync:      return "/* sync */";
        case IROp::Isync:     return "/* isync */";
        case IROp::Trap:      return "abort(); // Trap";
        case IROp::Syscall:   return "/* syscall (sc) */";
        
        case IROp::Load8:     return dest_gpr(0) + " = MEM_READ8(" + gpr(1) + " + " + gpr(2) + ");";
        case IROp::Load16:    return dest_gpr(0) + " = MEM_READ16(" + gpr(1) + " + " + gpr(2) + ");";
        case IROp::Load32:    return dest_gpr(0) + " = MEM_READ32(" + gpr(1) + " + " + gpr(2) + ");";
        case IROp::Store8:    return "MEM_WRITE8(" + gpr(1) + " + " + gpr(2) + ", " + gpr(0) + ");";
        case IROp::Store16:   return "MEM_WRITE16(" + gpr(1) + " + " + gpr(2) + ", " + gpr(0) + ");";
        case IROp::Store32:   return "MEM_WRITE32(" + gpr(1) + " + " + gpr(2) + ", " + gpr(0) + ");";
        
        case IROp::FAdd:      return dest_fpr(0) + " = " + fpr(1) + " + " + fpr(2) + ";";
        case IROp::FSub:      return dest_fpr(0) + " = " + fpr(1) + " - " + fpr(2) + ";";
        case IROp::FMul:      return dest_fpr(0) + " = " + fpr(1) + " * " + fpr(2) + ";";
        case IROp::FDiv:      return dest_fpr(0) + " = " + fpr(1) + " / " + fpr(2) + ";";
        case IROp::LoadFloat: return dest_fpr(0) + " = MEM_READ_FLOAT(" + gpr(1) + " + " + gpr(2) + ");";
        case IROp::StoreFloat:return "MEM_WRITE_FLOAT(" + gpr(1) + " + " + gpr(2) + ", " + fpr(0) + ");";
        
        case IROp::Load16a:    return dest_gpr(0) + " = MEM_READ16A(" + gpr(1) + " + " + gpr(2) + ");";
        case IROp::LoadDouble: return dest_fpr(0) + " = MEM_READ_DOUBLE(" + gpr(1) + " + " + gpr(2) + ");";
        case IROp::StoreDouble:return "MEM_WRITE_DOUBLE(" + gpr(1) + " + " + gpr(2) + ", " + fpr(0) + ");";
        
        case IROp::FMadd:      return dest_fpr(0) + " = (" + fpr(1) + " * " + fpr(2) + ") + " + fpr(3) + ";";
        case IROp::FMsub:      return dest_fpr(0) + " = (" + fpr(1) + " * " + fpr(2) + ") - " + fpr(3) + ";";
        case IROp::FSel:       return dest_fpr(0) + " = FSEL(" + fpr(1) + ", " + fpr(2) + ", " + fpr(3) + ");";
        case IROp::Frsp:       return dest_fpr(0) + " = FRSP(" + fpr(1) + ");";
        
        case IROp::Lmw:       return "LMW(ctx, " + gpr(0) + ", " + gpr(1) + ", " + operandToC(instr.operands[2]) + ");";
        case IROp::Stmw:      return "STMW(ctx, " + gpr(0) + ", " + gpr(1) + ", " + operandToC(instr.operands[2]) + ");";
        
        case IROp::Fctiw:     return dest_fpr(0) + " = (int32_t)" + fpr(1) + ";";
        case IROp::SetReg:    return dest_gpr(0) + " = " + gpr(1) + ";";
        
        case IROp::ReservationLoad:  return dest_gpr(0) + " = MEM_READ32(" + gpr(1) + " + " + gpr(2) + "); ctx->xer = 1; // lwarx";
        case IROp::ReservationStore: return "MEM_WRITE32(" + gpr(1) + " + " + gpr(2) + ", " + gpr(0) + "); // stwcx";
        
        case IROp::Mfmsr:     return dest_gpr(0) + " = 0; // mfmsr (placeholder)";
        case IROp::Mtmsr:     return "/* mtmsr */";
        
        case IROp::Branch: {
            u32 target = instr.operands[0].value;
            std::stringstream ss;
            if (func.blocks.count(target)) {
                ss << "goto label_0x" << std::hex << target << ";";
            } else {
                ss << "fn_0x" << std::hex << std::setfill('0') << std::setw(8) << target << "(ctx); return;";
            }
            return ss.str();
        }
        case IROp::BranchCond: {
            u32 bo = instr.operands[0].value;
            u32 bi = instr.operands[1].value;
            u32 target = instr.operands[2].value;
            std::stringstream ss;
            ss << "if (CHECK_COND(ctx, " << std::dec << bo << ", " << bi << ")) { ";
            if (func.blocks.count(target)) {
                ss << "goto label_0x" << std::hex << target << ";";
            } else {
                ss << "fn_0x" << std::hex << std::setfill('0') << std::setw(8) << target << "(ctx); return;";
            }
            ss << " }";
            return ss.str();
        }
        case IROp::BranchIndirect: return "call_by_addr(ctx, " + gpr(0) + "); return;";
        case IROp::Call: {
            u32 target = instr.operands[0].value;
            std::stringstream ss;
            ss << "fn_0x" << std::hex << std::setfill('0') << std::setw(8) << target << "(ctx);";
            return ss.str();
        }
        case IROp::CallIndirect:   return "call_by_addr(ctx, " + gpr(0) + ");";
        case IROp::Return:         return "return;";
        
        default: return "// Unsupported IROp (" + std::to_string((int)instr.op) + ")";
    }
    return "// Error";
}

std::string Emitter::regOrImm(const IROperand& op) {
    if (op.type == IROperandType::Register) {
        return "ctx->gpr[" + std::to_string(op.value) + "]";
    }
    return operandToC(op);
}

std::string Emitter::fRegOrImm(const IROperand& op) {
    if (op.type == IROperandType::Register) {
        return "ctx->fpr[" + std::to_string(op.value) + "]";
    }
    return operandToC(op);
}

std::string Emitter::operandToC(const IROperand& op) {
    switch (op.type) {
        case IROperandType::Register:
            return std::to_string(op.value);
        case IROperandType::Immediate:
            return "0x" + ([&]() {
                std::stringstream ss;
                ss << std::hex << op.value;
                return ss.str();
            }());
        case IROperandType::Address: {
            std::stringstream ss;
            ss << std::hex << op.value;
            return ss.str();
        }
        default:
            return "0";
    }
}

} // namespace gcrecomp
