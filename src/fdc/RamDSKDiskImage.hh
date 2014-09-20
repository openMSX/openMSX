#ifndef RAMDSKDISKIMAGE_HH
#define RAMDSKDISKIMAGE_HH

#include "SectorBasedDisk.hh"
#include "MemBuffer.hh"

namespace openmsx {

class RamDSKDiskImage final : public SectorBasedDisk
{
public:
	explicit RamDSKDiskImage(size_t size = 720 * 1024);
	~RamDSKDiskImage();

private:
	// SectorBasedDisk
	void readSectorImpl (size_t sector,       SectorBuffer& buf) override;
	void writeSectorImpl(size_t sector, const SectorBuffer& buf) override;
	bool isWriteProtectedImpl() const override;

	MemBuffer<SectorBuffer> data;
};

} // namespace openmsx

#endif
