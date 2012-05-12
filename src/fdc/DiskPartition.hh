// $Id$

#ifndef DISKPARTITION_HH
#define DISKPARTITION_HH

#include "SectorBasedDisk.hh"
#include "shared_ptr.hh"

namespace openmsx {

class DiskPartition : public SectorBasedDisk
{
public:
	/** Return a partition (as a SectorbasedDisk) from another Disk.
	 * @param disk The whole disk.
	 * @param partition The partition number.
	 *        0 for the whole disk
	 *        1-31 for a specific partition, this must be a valid partition.
	 * @param owned If specified it should be a shared_ptr to the Disk
	 *              object passed as first parameter. This DiskPartition
	 *              will then take (shared) ownership of that Disk.
	 */
	DiskPartition(SectorAccessibleDisk& disk, unsigned partition,
	              const shared_ptr<SectorAccessibleDisk>& owned =
	                  shared_ptr<SectorAccessibleDisk>());

	DiskPartition(SectorAccessibleDisk& parent,
	              unsigned start, unsigned length);

private:
	virtual void readSectorImpl(unsigned sector, byte* buf);
	virtual void writeSectorImpl(unsigned sector, const byte* buf);
	virtual bool isWriteProtectedImpl() const;

	SectorAccessibleDisk& parent;
	shared_ptr<SectorAccessibleDisk> owned;
	unsigned start;
};

} // namespace openmsx

#endif
