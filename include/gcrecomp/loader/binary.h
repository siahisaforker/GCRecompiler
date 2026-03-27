#pragma once

#include <gcrecomp/types.h>
#include <gcrecomp/log.h>
#include <gcrecomp/common.h>
#include <vector>
#include <string>
#include <optional>

namespace gcrecomp {

class Binary {
public:
    virtual ~Binary() = default;

    virtual bool load(const std::string& path) = 0;

    virtual u32 getEntryPoint() const = 0;
    virtual const std::vector<Section>& getSections() const = 0;

    bool isValidAddress(u32 addr) const {
        return findRegion(addr) != nullptr;
    }

    bool isExecutable(u32 addr) const {
        for (const auto& sec : getSections()) {
            if (sec.isText && addr >= sec.address && addr < sec.address + sec.size) {
                return true;
            }
        }
        return false;
    }

    u8 read8(u32 addr) const {
        const auto* region = findRegion(addr);
        if (!region || !region->contains(addr)) {
            LOG_ERROR("Invalid read8 at 0x%08X", addr);
            return 0;
        }
        return region->data[addr - region->start];
    }

    u16 read16(u32 addr) const {
        if (addr % 2 != 0) {
            LOG_ERROR("Unaligned read16 at 0x%08X", addr);
            return 0;
        }
        const auto* region = findRegion(addr);
        if (!region || !region->contains(addr) || !region->contains(addr + 1)) {
            LOG_ERROR("Invalid or cross-region read16 at 0x%08X", addr);
            return 0;
        }
        return read_be16(&region->data[addr - region->start]);
    }

    u32 read32(u32 addr) const {
        if (addr % 4 != 0) {
            LOG_ERROR("Unaligned read32 at 0x%08X", addr);
            return 0;
        }
        const auto* region = findRegion(addr);
        if (!region || !region->contains(addr) || !region->contains(addr + 3)) {
            LOG_ERROR("Invalid or cross-region read32 at 0x%08X", addr);
            return 0;
        }
        return read_be32(&region->data[addr - region->start]);
    }

protected:
    const MemoryRegion* findRegion(u32 addr) const {
        if (m_lastRegion && m_lastRegion->contains(addr)) {
            return m_lastRegion;
        }

        for (const auto& region : m_regions) {
            if (region.contains(addr)) {
                m_lastRegion = &region;
                return m_lastRegion;
            }
        }
        return nullptr;
    }

    std::vector<MemoryRegion> m_regions;
    mutable const MemoryRegion* m_lastRegion = nullptr;
    std::vector<Section> m_sections;
    u32 m_entryPoint = 0;
};

} // namespace gcrecomp
