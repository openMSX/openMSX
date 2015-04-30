#include "RamDSKDiskImage.hh"
#include "DiskImageUtils.hh"
#include <cstring>

namespace openmsx {

RamDSKDiskImage::RamDSKDiskImage(size_t size)
	: SectorBasedDisk(DiskName(Filename(), "ramdsk"))
	, data(size / sizeof(SectorBuffer))
{
	setNbSectors(size / sizeof(SectorBuffer));

	DiskImageUtils::format(*this);
}

RamDSKDiskImage::~RamDSKDiskImage()
{
}

void RamDSKDiskImage::readSectorImpl(size_t sector, SectorBuffer& buf)
{
	memcpy(&buf, &data[sector], sizeof(buf));
}

void RamDSKDiskImage::writeSectorImpl(size_t sector, const SectorBuffer& buf)
{
	memcpy(&data[sector], &buf, sizeof(buf));
}

bool RamDSKDiskImage::isWriteProtectedImpl() const
{
	return false;
}

} // namespace openmsx
