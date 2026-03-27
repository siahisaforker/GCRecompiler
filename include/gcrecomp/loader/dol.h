#pragma once

#include <gcrecomp/loader/binary.h>
#include <vector>

namespace gcrecomp {

struct DolHeader {
    u32 textOffsets[7];
    u32 dataOffsets[11];
    u32 textAddresses[7];
    u32 dataAddresses[11];
    u32 textSizes[7];
    u32 dataSizes[11];
    u32 bssAddress;
    u32 bssSize;
    u32 entryPoint;
    u32 padding[7];
};

class DolBinary : public Binary {
public:
    bool load(const std::string& path) override;

    u32 getEntryPoint() const override { return m_entryPoint; }
    const std::vector<Section>& getSections() const override { return m_sections; }

private:
    bool parseHeader(const std::vector<u8>& data, DolHeader& header);
    bool mapSections(const std::vector<u8>& fileData, const DolHeader& header);
};

} // namespace gcrecomp
