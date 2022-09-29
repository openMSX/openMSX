#ifndef RAMDSKDISKIMAGE_HH
#define RAMDSKDISKIMAGE_HH

#include "SectorBasedDisk.hh"
#include "MemBuffer.hh"

namespace openmsx {

class RamDSKDiskImage final : public SectorBasedDisk
{
public:
	explicit RamDSKDiskImage(size_t size = 720 * 1024);

private:
	// SectorBasedDisk
	void readSectorsImpl(
		std::span<SectorBuffer> buffers, size_t startSector) override;
	void writeSectorImpl(size_t sector, const SectorBuffer& buf) override;
	[[nodiscard]] bool isWriteProtectedImpl() const override;

private:
	MemBuffer<SectorBuffer> data;
};

} // namespace openmsx

#endif
