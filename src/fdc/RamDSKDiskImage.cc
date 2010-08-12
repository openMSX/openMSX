// $Id$

#include "RamDSKDiskImage.hh"
#include "DiskImageUtils.hh"
#include <cstring>

namespace openmsx {

RamDSKDiskImage::RamDSKDiskImage(unsigned size)
	: SectorBasedDisk(DiskName(Filename(), "ramdsk"))
	, diskdata(size)
{
	setNbSectors(size / SECTOR_SIZE);

	DiskImageUtils::format(*this);
}

RamDSKDiskImage::~RamDSKDiskImage()
{
}

void RamDSKDiskImage::readSectorImpl(unsigned sector, byte* buf)
{
	memcpy(buf, &diskdata[sector * SECTOR_SIZE], SECTOR_SIZE);
}

void RamDSKDiskImage::writeSectorImpl(unsigned sector, const byte* buf)
{
	memcpy(&diskdata[sector * SECTOR_SIZE], buf, SECTOR_SIZE);
}

bool RamDSKDiskImage::isWriteProtectedImpl() const
{
	return false;
}

} // namespace openmsx
