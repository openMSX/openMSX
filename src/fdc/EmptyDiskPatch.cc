// $Id$

#include "EmptyDiskPatch.hh"
#include "SectorBasedDisk.hh"

namespace openmsx {

EmptyDiskPatch::EmptyDiskPatch(SectorBasedDisk& disk_)
	: disk(disk_)
{
}

void EmptyDiskPatch::copyBlock(unsigned src, byte* dst, unsigned num) const
{
	disk.readLogicalSector(src / SectorBasedDisk::SECTOR_SIZE, dst);
}

unsigned EmptyDiskPatch::getSize() const
{
	return disk.getNbSectors() * SectorBasedDisk::SECTOR_SIZE;
}

} // namespace openmsx

