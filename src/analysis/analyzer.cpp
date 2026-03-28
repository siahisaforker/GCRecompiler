#include <gcrecomp/analysis/analyzer.h>
#include <gcrecomp/codegen/emitter.h>
#include <gcrecomp/log.h>
#include <fstream>
#include <filesystem>
#include <set>
#include <string>

namespace gcrecomp {

Analyzer::Analyzer(const Binary &binary) : m_binary(binary) {}

void Analyzer::analyze(u32 entryPoint) {
  std::queue<u32> functionWorkList;
  
  auto hexStr = [](u32 val) {
      std::stringstream ss;
      ss << std::hex << std::setw(8) << std::setfill('0') << val;
      return ss.str();
  };

  auto discover = [&](u32 addr, const std::string& name) {
      if (m_discoveredFunctions.find(addr) == m_discoveredFunctions.end()) {
          m_discoveredFunctions.insert(addr);
          if (m_cfg.getFunction(addr) == nullptr) {
              Function f;
              f.startAddr = addr;
              f.name = name;
              m_cfg.addFunction(f);
              return true;
          }
      }
      return false;
  };

  // 1. Scan for prologues across all text sections
  for (const auto& sec : m_binary.getSections()) {
      if (sec.isText && sec.size > 0) {
          LOG_INFO("Scanning text section 0x%08X (size 0x%X) for prologues...", sec.address, sec.size);
          for (u32 addr = sec.address; addr < sec.address + sec.size; addr += 4) {
              u32 raw;
              if (m_binary.read32(addr, raw)) {
                  // stwu r1, -xx(r1) -> 0x9421XXXX
                  if ((raw >> 16) == 0x9421) {
                      discover(addr, "fn_0x" + hexStr(addr));
                  }
              }
          }
          // Also discover the very start of the section just in case
          discover(sec.address, "entry_0x" + hexStr(sec.address));
      }
  }

  // 2. Discover entry point
  discover(entryPoint, "main_entry");

  // 3. Populate worklist
  for (u32 addr : m_discoveredFunctions) {
      functionWorkList.push(addr);
  }

  LOG_INFO("Seeded analysis with %zu discovered functions", m_discoveredFunctions.size());

  while (!functionWorkList.empty()) {
      u32 addr = functionWorkList.front();
      functionWorkList.pop();

      if (m_analyzedFunctions.count(addr)) continue;
      m_analyzedFunctions.insert(addr);

      analyzeFunction(addr);

      // Check for newly discovered functions from analyzeFunction/analyzeBlock calls
      for (u32 discovered : m_discoveredFunctions) {
          if (m_analyzedFunctions.find(discovered) == m_analyzedFunctions.end()) {
              functionWorkList.push(discovered);
          }
      }
  }
}

void Analyzer::analyzeFunction(u32 entryAddr) {
    Function* func = m_cfg.getFunction(entryAddr);
    if (!func) return;

    std::set<u32> seeds;
    seeds.insert(entryAddr);
    
    // Pass 1: Identify all branch targets and block starts within reachable code
    std::queue<u32> scanQueue;
    scanQueue.push(entryAddr);
    std::set<u32> scanned;

    while (!scanQueue.empty()) {
        u32 addr = scanQueue.front();
        scanQueue.pop();

        if (scanned.count(addr)) continue;
        scanned.insert(addr);

        u32 current = addr;
        bool blockEnded = false;
        while (!blockEnded) {
            Instruction instr;
            if (!m_disasm.disassemble(m_binary, current, instr)) break;

            if (instr.isBranch) {
                if (instr.branchTarget != 0 && instr.type != InstructionType::Call) {
                    if (m_binary.isExecutable(instr.branchTarget)) {
                        if (seeds.find(instr.branchTarget) == seeds.end()) {
                            seeds.insert(instr.branchTarget);
                            scanQueue.push(instr.branchTarget);
                        }
                    }
                }
                
                if (instr.type == InstructionType::Return) {
                    blockEnded = true;
                } else if (instr.type == InstructionType::Call) {
                    // Discover function at target
                    if (instr.branchTarget != 0) {
                        m_discoveredFunctions.insert(instr.branchTarget);
                    }
                    current += 4;
                    // Seed the return point
                    if (seeds.find(current) == seeds.end()) {
                        seeds.insert(current);
                        scanQueue.push(current);
                    }
                    blockEnded = true;
                } else if (instr.type == InstructionType::Branch) {
                    blockEnded = true;
                } else {
                    // Conditional branch: both target and next are seeds
                    if (seeds.find(current + 4) == seeds.end()) {
                        seeds.insert(current + 4);
                        scanQueue.push(current + 4);
                    }
                    blockEnded = true;
                }
            } else {
                current += 4;
                if (seeds.count(current)) blockEnded = true;
            }
        }
    }

    // Pass 2: Build the blocks starting from seeds
    for (u32 seed : seeds) {
        func->blocks.insert(seed);
        analyzeBlock(seed, *func, seeds);
    }
}

void Analyzer::analyzeBlock(u32 startAddr, Function& currentFunc, const std::set<u32>& seeds) {
  if (m_visitedBlocks.count(startAddr)) {
      return;
  }

  BasicBlock block;
  block.startAddr = startAddr;
  u32 currentAddr = startAddr;

  LOG_DEBUG("Analyzing block at 0x%08X", startAddr);

  while (true) {
    Instruction instr;
    if (!m_disasm.disassemble(m_binary, currentAddr, instr)) {
      block.type = BlockType::Invalid;
      break;
    }

    block.instructions.push_back(instr);
    m_visitedBlocks.insert(currentAddr);

    if (instr.isBranch) {
      block.endAddr = currentAddr;

      if (instr.type == InstructionType::Return) {
        block.type = BlockType::Return;
      } else if (instr.type == InstructionType::Call) {
        block.type = BlockType::Call;
        u32 next = currentAddr + 4;
        block.successors.insert(next);

        if (instr.branchTarget != 0) {
          if (m_discoveredFunctions.find(instr.branchTarget) == m_discoveredFunctions.end()) {
            m_discoveredFunctions.insert(instr.branchTarget);
            if (m_cfg.getFunction(instr.branchTarget) == nullptr) {
                Function newFunc;
                newFunc.startAddr = instr.branchTarget;
                newFunc.name = "sub_0x" + ([&]() {
                    std::stringstream ss;
                    ss << std::hex << std::setw(8) << std::setfill('0') << instr.branchTarget;
                    return ss.str();
                }());
                m_cfg.addFunction(newFunc);
            }
          }
        }
      } else if (instr.type == InstructionType::Call || instr.type == InstructionType::BranchLink) {
        if (instr.branchTarget != 0) {
          if (m_discoveredFunctions.find(instr.branchTarget) == m_discoveredFunctions.end()) {
            m_discoveredFunctions.insert(instr.branchTarget);
            if (m_cfg.getFunction(instr.branchTarget) == nullptr) {
                Function newFunc;
                newFunc.startAddr = instr.branchTarget;
                newFunc.name = "fn_0x" + ([&]() {
                    std::stringstream ss;
                    ss << std::hex << std::setw(8) << std::setfill('0') << instr.branchTarget;
                    return ss.str();
                }());
                m_cfg.addFunction(newFunc);
            }
          }
        }
      } else if (instr.type == InstructionType::Branch) {
        if (instr.branchTarget != 0) {
          block.successors.insert(instr.branchTarget);
        }

        if (instr.mnemonic.find('b') == 0 && instr.mnemonic.length() > 1 &&
            instr.mnemonic[1] != ' ') {
          u32 next = currentAddr + 4;
          block.successors.insert(next);
        }
      }
      break;
    }

    currentAddr += 4;
    // Termination condition: hit another seed or already visited block
    if (seeds.count(currentAddr) || m_visitedBlocks.count(currentAddr)) {
        block.endAddr = currentAddr - 4;
        block.successors.insert(currentAddr);
        break;
    }
  }

  block.endAddr = currentAddr;
  block.isAnalyzed = true;

  IRBlock ir = m_lifter.liftBlock(block);
  m_optimizer.optimizeBlock(ir);
  block.irInstructions = ir.instructions;

  m_cfg.addBlock(block);
}

void Analyzer::emitAllFunctions(const std::string& outputDir) {
    std::filesystem::create_directories(outputDir);
    Emitter emitter;
    
    // First, generate functions.h
    std::string fhPath = outputDir + "/functions.h";
    std::ofstream fh(fhPath);
    if (fh.is_open()) {
        fh << "#ifndef FUNCTIONS_H\n#define FUNCTIONS_H\n\n";
        fh << "#include \"recomp_runtime.h\"\n\n";
        for (const auto& [addr, func] : m_cfg.getFunctions()) {
            fh << "extern void fn_0x" << std::hex << std::setw(8) << std::setfill('0') << addr << "(CPUContext* ctx);\n";
        }
        fh << "\n#endif\n";
        fh.close();
    }

    // Now emit each function in batches of 100
    int funcIdx = 0;
    int batchIdx = 0;
    std::ofstream out;
    
    for (const auto& [addr, func] : m_cfg.getFunctions()) {
        if (funcIdx % 100 == 0) {
            if (out.is_open()) out.close();
            std::string fileName = outputDir + "/batch_" + std::to_string(batchIdx++) + ".c";
            out.open(fileName);
            if (out.is_open()) {
                out << "#include \"recomp_runtime.h\"\n";
                out << "#include \"functions.h\"\n\n";
            }
        }
        
        if (out.is_open()) {
            out << "// Function: " << func.name << " at 0x" << std::hex << addr << "\n";
            out << emitter.emitFunction(func, m_cfg) << "\n\n";
        }
        funcIdx++;
    }
    if (out.is_open()) out.close();

    // Emit jump_table.c
    std::ofstream jt(outputDir + "/jump_table.c");
    if (jt.is_open()) {
        jt << "#include \"recomp_runtime.h\"\n";
        jt << "#include \"functions.h\"\n\n";
        jt << "void call_by_addr(CPUContext* ctx, uint32_t addr) {\n";
        jt << "    switch (addr) {\n";
        for (const auto& [addr, func] : m_cfg.getFunctions()) {
            jt << "        case 0x" << std::hex << addr << ": fn_0x" << std::hex << std::setw(8) << std::setfill('0') << addr << "(ctx); break;\n";
        }
        jt << "        default: printf(\"[RUNTIME] Unknown function call: 0x%08X\\n\", addr); break;\n";
        jt << "    }\n";
        jt << "}\n";
    }
}

} // namespace gcrecomp
