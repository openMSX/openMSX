// $Id$

#include "EmptyDiskPatch.hh"
#include "SectorBasedDisk.hh"
#include <cassert>

namespace openmsx {

EmptyDiskPatch::EmptyDiskPatch(SectorBasedDisk& disk_)
	: disk(disk_)
{
}

void EmptyDiskPatch::copyBlock(unsigned src, byte* dst, unsigned num) const
{
	assert(num == SectorBasedDisk::SECTOR_SIZE);
	disk.readLogicalSector(src / SectorBasedDisk::SECTOR_SIZE, dst);
}

unsigned EmptyDiskPatch::getSize() const
{
	return disk.getNbSectors() * SectorBasedDisk::SECTOR_SIZE;
}

} // namespace openmsx

