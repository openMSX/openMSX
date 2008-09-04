// $Id$

#include "RamDSKDiskImage.hh"
#include "DiskImageUtils.hh"
#include <cstring>

namespace openmsx {

RamDSKDiskImage::RamDSKDiskImage(unsigned size)
	: SectorBasedDisk(Filename("ramdsk"))
{
	setNbSectors(size / SECTOR_SIZE);
	diskdata = new byte[size];

	DiskImageUtils::format(*this);
}

RamDSKDiskImage::~RamDSKDiskImage()
{
	delete[] diskdata;
}

void RamDSKDiskImage::readSectorSBD(unsigned sector, byte* buf)
{
	memcpy(buf, &diskdata[sector * SECTOR_SIZE], SECTOR_SIZE);
}

void RamDSKDiskImage::writeSectorSBD(unsigned sector, const byte* buf)
{
	memcpy(&diskdata[sector * SECTOR_SIZE], buf, SECTOR_SIZE);
}

bool RamDSKDiskImage::isWriteProtectedImpl() const
{
	return false;
}

} // namespace openmsx
