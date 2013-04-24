#include "RamDSKDiskImage.hh"
#include "DiskImageUtils.hh"
#include <cstring>

namespace openmsx {

RamDSKDiskImage::RamDSKDiskImage(size_t size)
	: SectorBasedDisk(DiskName(Filename(), "ramdsk"))
	, diskdata(size)
{
	setNbSectors(size / SECTOR_SIZE);

	DiskImageUtils::format(*this);
}

RamDSKDiskImage::~RamDSKDiskImage()
{
}

void RamDSKDiskImage::readSectorImpl(size_t sector, byte* buf)
{
	memcpy(buf, &diskdata[sector * SECTOR_SIZE], SECTOR_SIZE);
}

void RamDSKDiskImage::writeSectorImpl(size_t sector, const byte* buf)
{
	memcpy(&diskdata[sector * SECTOR_SIZE], buf, SECTOR_SIZE);
}

bool RamDSKDiskImage::isWriteProtectedImpl() const
{
	return false;
}

} // namespace openmsx
