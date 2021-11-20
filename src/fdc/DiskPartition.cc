#include "DiskPartition.hh"
#include "DiskImageUtils.hh"
#include "Filename.hh"
#include "strCat.hh"
#include <cassert>

namespace openmsx {

using namespace DiskImageUtils;

[[nodiscard]] static DiskName getDiskName(SectorAccessibleDisk* disk, unsigned partition)
{
	if (auto* dsk = dynamic_cast<Disk*>(disk)) {
		return DiskName(dsk->getName().getFilename(),
		                strCat(':', partition));
	} else {
		return DiskName(Filename("dummy"));
	}
}

DiskPartition::DiskPartition(SectorAccessibleDisk& disk, unsigned partition,
                             std::shared_ptr<SectorAccessibleDisk> owned_)
	: SectorBasedDisk(getDiskName(&disk, partition))
	, parent(disk)
	, owned(std::move(owned_))
{
	assert(!owned || (owned.get() == &disk));

	if (partition == 0) {
		start = 0;
		setNbSectors(disk.getNbSectors());
	} else {
		checkValidPartition(disk, partition); // throws
		SectorBuffer buf;
		disk.readSector(0, buf);
		auto& p = buf.pt.part[31 - partition];
		start = p.start;
		setNbSectors(p.size);
	}
}

DiskPartition::DiskPartition(SectorAccessibleDisk& parent_,
                             size_t start_, size_t length)
	: SectorBasedDisk(getDiskName(nullptr, 0))
	, parent(parent_)
	, start(start_)
{
	setNbSectors(length);
}

void DiskPartition::readSectorImpl(size_t sector, SectorBuffer& buf)
{
	parent.readSector(start + sector, buf);
}

void DiskPartition::writeSectorImpl(size_t sector, const SectorBuffer& buf)
{
	parent.writeSector(start + sector, buf);
}

bool DiskPartition::isWriteProtectedImpl() const
{
	return parent.isWriteProtected();
}

} // namespace openmsx
