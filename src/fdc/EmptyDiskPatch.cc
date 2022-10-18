#include "EmptyDiskPatch.hh"
#include "SectorAccessibleDisk.hh"
#include <cassert>

namespace openmsx {

EmptyDiskPatch::EmptyDiskPatch(SectorAccessibleDisk& disk_)
	: disk(disk_)
{
}

void EmptyDiskPatch::copyBlock(size_t src, uint8_t* dst, size_t num) const
{
	assert((num % SectorAccessibleDisk::SECTOR_SIZE) == 0);
	assert((src % SectorAccessibleDisk::SECTOR_SIZE) == 0);
	auto* buf = aligned_cast<SectorBuffer*>(dst);
	disk.readSectorsImpl(buf,
	                     src / SectorAccessibleDisk::SECTOR_SIZE,
	                     num / SectorAccessibleDisk::SECTOR_SIZE);
}

size_t EmptyDiskPatch::getSize() const
{
	return disk.getNbSectors() * SectorAccessibleDisk::SECTOR_SIZE;
}

std::vector<Filename> EmptyDiskPatch::getFilenames() const
{
	return {};
}

} // namespace openmsx
