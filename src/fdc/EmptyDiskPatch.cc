#include "EmptyDiskPatch.hh"
#include "SectorAccessibleDisk.hh"
#include <cassert>

namespace openmsx {

EmptyDiskPatch::EmptyDiskPatch(SectorAccessibleDisk& disk_)
	: disk(disk_)
{
}

void EmptyDiskPatch::copyBlock(size_t src, std::span<uint8_t> dst) const
{
	assert((dst.size() % SectorAccessibleDisk::SECTOR_SIZE) == 0);
	assert((src % SectorAccessibleDisk::SECTOR_SIZE) == 0);
	auto* buf = aligned_cast<SectorBuffer*>(dst.data());
	disk.readSectorsImpl(buf,
	                     src / SectorAccessibleDisk::SECTOR_SIZE,
	                     dst.size() / SectorAccessibleDisk::SECTOR_SIZE);
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
