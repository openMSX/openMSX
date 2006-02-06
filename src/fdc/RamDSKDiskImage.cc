// $Id$

#include "RamDSKDiskImage.hh"
#include "MSXtar.hh"
#include <string.h>

namespace openmsx {

RamDSKDiskImage::RamDSKDiskImage(unsigned size)
	: SectorBasedDisk("-ramdsk")
{
	nbSectors = size / SECTOR_SIZE;
	diskdata = new byte[size];

	MSXtar workhorse(*this);
	workhorse.format();
}

RamDSKDiskImage::~RamDSKDiskImage()
{
	delete[] diskdata;
}

void RamDSKDiskImage::readLogicalSector(unsigned sector, byte* buf)
{
	memcpy(buf, &diskdata[sector * SECTOR_SIZE], SECTOR_SIZE);
}

void RamDSKDiskImage::writeLogicalSector(unsigned sector, const byte* buf)
{
	memcpy(&diskdata[sector * SECTOR_SIZE], buf, SECTOR_SIZE);
}

bool RamDSKDiskImage::writeProtected()
{
	return false;
}

} // namespace openmsx
