#include <gcrecomp/loader/dol.h>
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

    u32 entry = dol.getEntryPoint();
    std::cout << "\nFirst Instructions:" << std::endl;
    for (int i = 0; i < 5; ++i) {
        u32 addr = entry + i * 4;
        if (dol.isValidAddress(addr)) {
            u32 instr = dol.read32(addr);
            std::cout << "0x" << std::hex << std::setw(8) << std::setfill('0') << addr << ": "
                      << std::hex << std::setw(8) << std::setfill('0') << instr << std::endl;
        }
    }

    return 0;
}
