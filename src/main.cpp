#include <gcrecomp/loader/dol.h>
#include <gcrecomp/analysis/analyzer.h>
#include <gcrecomp/log.h>
#include <iostream>
#include <iomanip>

using namespace gcrecomp;

int main(int argc, char** argv) {
    if (argc < 2) {
        std::cout << "Usage: gcrecomp <game.dol>" << std::endl;
        return 1;
    }

    std::string dolPath = argv[1];
    DolBinary dol;

    if (!dol.load(dolPath)) {
        LOG_ERROR("Failed to load DOL: %s", dolPath.c_str());
        return 1;
    }

    LOG_INFO("Loaded DOL: %s", dolPath.c_str());
    LOG_INFO("Entry Point: 0x%08X", dol.getEntryPoint());

    std::cout << "\nMemory Regions:" << std::endl;
    for (const auto& sec : dol.getSections()) {
        std::cout << "[" << (sec.isText ? "TEXT" : "DATA") << "] "
                  << "0x" << std::hex << std::setw(8) << std::setfill('0') << sec.address
                  << " - 0x" << std::hex << std::setw(8) << std::setfill('0') << (sec.address + sec.size)
                  << " (size: 0x" << std::hex << sec.size << ")" << std::endl;
    }

    LOG_INFO("Starting Control Flow Analysis...");
    Analyzer analyzer(dol);
    analyzer.analyze(dol.getEntryPoint());

    LOG_INFO("Analysis Complete. Discovered %zu basic blocks and %zu functions.", 
             analyzer.getCfg().getBlocks().size(), 
             analyzer.getCfg().getFunctions().size());

    const auto& cfg = analyzer.getCfg();
    LOG_INFO("  Basic Blocks: %zu", cfg.getBlocks().size());
    LOG_INFO("  Functions:    %zu", cfg.getFunctions().size());

    std::string dotPath = "cfg.dot";
    cfg.exportDot(dotPath);
    LOG_INFO("CFG exported to %s", dotPath.c_str());

    return 0;
}
