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
	(void)num;
	assert(num == SectorBasedDisk::SECTOR_SIZE);
	disk.readSectorImpl(src / SectorBasedDisk::SECTOR_SIZE, dst);
}

unsigned EmptyDiskPatch::getSize() const
{
	return disk.getNbSectors() * SectorBasedDisk::SECTOR_SIZE;
}

void EmptyDiskPatch::getFilenames(std::vector<Filename>& /*result*/) const
{
	// nothing
}

} // namespace openmsx

