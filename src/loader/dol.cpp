#include <gcrecomp/loader/dol.h>
#include <gcrecomp/log.h>
#include <gcrecomp/common.h>
#include <fstream>
#include <cstring>

namespace gcrecomp {

bool DolBinary::load(const std::string& path) {
    std::ifstream file(path, std::ios::binary | std::ios::ate);
    if (!file.is_open()) {
        LOG_ERROR("Failed to open DOL file: %s", path.c_str());
        return false;
    }

    std::streamsize size = file.tellg();
    file.seekg(0, std::ios::beg);

    std::vector<u8> fileData(size);
    if (!file.read(reinterpret_cast<char*>(fileData.data()), size)) {
        LOG_ERROR("Failed to read DOL file data");
        return false;
    }

    DolHeader header;
    if (!parseHeader(fileData, header)) {
        return false;
    }

    if (!mapSections(fileData, header)) {
        return false;
    }

    m_entryPoint = header.entryPoint;

    if (!isValidAddress(m_entryPoint)) {
        LOG_ERROR("Entry point 0x%08X is not in a valid memory region", m_entryPoint);
        return false;
    }

    if (!isExecutable(m_entryPoint)) {
        LOG_INFO("Warning: Entry point 0x%08X is not in a text section", m_entryPoint);
    }

    return true;
}

bool DolBinary::parseHeader(const std::vector<u8>& fileData, DolHeader& header) {
    if (fileData.size() < sizeof(DolHeader)) {
        LOG_ERROR("File too small to contain DOL header");
        return false;
    }

    const u8* ptr = fileData.data();

    for (int i = 0; i < 7; ++i) header.textOffsets[i] = read_be32(ptr + i * 4);
    ptr += 7 * 4;
    for (int i = 0; i < 11; ++i) header.dataOffsets[i] = read_be32(ptr + i * 4);
    ptr += 11 * 4;
    for (int i = 0; i < 7; ++i) header.textAddresses[i] = read_be32(ptr + i * 4);
    ptr += 7 * 4;
    for (int i = 0; i < 11; ++i) header.dataAddresses[i] = read_be32(ptr + i * 4);
    ptr += 11 * 4;
    for (int i = 0; i < 7; ++i) header.textSizes[i] = read_be32(ptr + i * 4);
    ptr += 7 * 4;
    for (int i = 0; i < 11; ++i) header.dataSizes[i] = read_be32(ptr + i * 4);
    ptr += 11 * 4;

    header.bssAddress = read_be32(ptr);
    header.bssSize = read_be32(ptr + 4);
    header.entryPoint = read_be32(ptr + 8);

    return true;
}

bool DolBinary::mapSections(const std::vector<u8>& fileData, const DolHeader& header) {
    // Map text sections
    for (int i = 0; i < 7; ++i) {
        if (header.textSizes[i] == 0) continue;

        if (header.textOffsets[i] + header.textSizes[i] > fileData.size()) {
            LOG_ERROR("Text section %d exceeds file bounds", i);
            return false;
        }

        MemoryRegion region;
        region.start = header.textAddresses[i];
        region.size = header.textSizes[i];
        region.data.assign(fileData.begin() + header.textOffsets[i], 
                           fileData.begin() + header.textOffsets[i] + header.textSizes[i]);
        
        m_regions.push_back(std::move(region));
        m_sections.push_back({header.textAddresses[i], header.textSizes[i], header.textOffsets[i], true});
    }

    // Map data sections
    for (int i = 0; i < 11; ++i) {
        if (header.dataSizes[i] == 0) continue;

        if (header.dataOffsets[i] + header.dataSizes[i] > fileData.size()) {
            LOG_ERROR("Data section %d exceeds file bounds", i);
            return false;
        }

        MemoryRegion region;
        region.start = header.dataAddresses[i];
        region.size = header.dataSizes[i];
        region.data.assign(fileData.begin() + header.dataOffsets[i], 
                           fileData.begin() + header.dataOffsets[i] + header.dataSizes[i]);
        
        m_regions.push_back(std::move(region));
        m_sections.push_back({header.dataAddresses[i], header.dataSizes[i], header.dataOffsets[i], false});
    }

    // Map BSS
    if (header.bssSize > 0) {
        MemoryRegion region;
        region.start = header.bssAddress;
        region.size = header.bssSize;
        region.data.resize(header.bssSize, 0); // Zero-initialized
        
        m_regions.push_back(std::move(region));
        m_sections.push_back({header.bssAddress, header.bssSize, 0, false});
    }

    return true;
}

} // namespace gcrecomp
