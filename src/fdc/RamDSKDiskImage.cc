#include "RamDSKDiskImage.hh"
#include "DiskImageUtils.hh"
#include "ranges.hh"

namespace openmsx {

RamDSKDiskImage::RamDSKDiskImage(size_t size)
	: SectorBasedDisk(DiskName(Filename(), "ramdsk"))
	, data(size / sizeof(SectorBuffer))
{
	setNbSectors(size / sizeof(SectorBuffer));

	DiskImageUtils::format(*this, MSXBootSectorType::DOS2);
}

void RamDSKDiskImage::readSectorsImpl(
	std::span<SectorBuffer> buffers, size_t startSector)
{
	ranges::copy(data.subspan(startSector, buffers.size()), buffers);
}

void RamDSKDiskImage::writeSectorImpl(size_t sector, const SectorBuffer& buf)
{
	data[sector] = buf;
}

bool RamDSKDiskImage::isWriteProtectedImpl() const
{
	return false;
}

} // namespace openmsx
