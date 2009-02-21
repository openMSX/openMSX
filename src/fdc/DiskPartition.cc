// $Id$

#include "DiskPartition.hh"
#include "DiskImageUtils.hh"
#include "Filename.hh"
#include "MSXException.hh"
#include "StringOp.hh"
#include <cassert>

namespace openmsx {

using namespace DiskImageUtils;

static inline unsigned read32LE(const byte* x)
{
	return (x[0] << 0) + (x[1] << 8) + (x[2] << 16) + (x[3] << 24);
}

static DiskName getDiskName(SectorAccessibleDisk* disk, unsigned partition)
{
	if (Disk* dsk = dynamic_cast<Disk*>(disk)) {
		return DiskName(dsk->getName().getFilename(),
		                ':' + StringOp::toString(partition));
	} else {
		return DiskName(Filename("dummy"));
	}
}

DiskPartition::DiskPartition(SectorAccessibleDisk& disk, unsigned partition,
                             shared_ptr<SectorAccessibleDisk> owned_)
	: SectorBasedDisk(getDiskName(&disk, partition))
	, parent(disk)
	, owned(owned_)
{
	assert(!owned.get() || (owned.get() == &disk));

	if (partition == 0) {
		start = 0;
		setNbSectors(disk.getNbSectors());
	} else {
		checkValidPartition(disk, partition); // throws
		byte buf[SECTOR_SIZE];
		disk.readSector(0, buf);
		Partition* p = reinterpret_cast<Partition*>(
			&buf[14 + (31 - partition) * 16]);
		start = read32LE(p->start4);
		setNbSectors(read32LE(p->size4));
	}
}

DiskPartition::DiskPartition(SectorAccessibleDisk& parent_,
                             unsigned start_, unsigned length)
	: SectorBasedDisk(getDiskName(NULL, 0))
	, parent(parent_)
	, start(start_)
{
	setNbSectors(length);
}

void DiskPartition::readSectorImpl(unsigned sector, byte* buf)
{
	parent.readSector(start + sector, buf);
}

void DiskPartition::writeSectorImpl(unsigned sector, const byte* buf)
{
	parent.writeSector(start + sector, buf);
}

bool DiskPartition::isWriteProtectedImpl() const
{
	return parent.isWriteProtected();
}

} // namespace openmsx
