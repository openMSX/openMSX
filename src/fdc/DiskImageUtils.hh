// $Id$

#ifndef DISK_IMAGE_UTILS_HH
#define DISK_IMAGE_UTILS_HH

#include "SectorAccessibleDisk.hh"
#include <vector>
#include <memory>

namespace openmsx {

namespace DiskImageUtils {

	/** Checks whether
	 *   the disk is partitioned
	 *   the specified partition
	 *   partition is of type FAT12
	 * throws a CommandException if one of these conditions is false
	 * @param disk The disk to check.
	 * @param partition Partition number, in range [1..31].
	 */
	void checkValidPartition(SectorAccessibleDisk& disk, unsigned partition);

	/** Check whether the given disk is partitioned.
	 */
	bool hasPartitionTable(SectorAccessibleDisk& disk);

	/** Format the given disk (= a single partition).
	 * The formatting depends on the size of the image.
	 */
	void format(SectorAccessibleDisk& disk);

	/** Write a partition table to the given disk and format each partition
	 * @param disk The disk to partition.
	 * @param sizes The number of sectors for each partition.
	 */
	void partition(SectorAccessibleDisk& disk, std::vector<unsigned> sizes);

	/** Return a partition (as a SectorAccessibleDisk) from another
	 *  SectorAccessibleDisk.
	 * @param disk The whole disk.
	 * @param partition The partition number.
	 *        0 for the whole disk
	 *        1-31 for a specific partition, this must be a valid partition.
	 */
	std::auto_ptr<SectorAccessibleDisk> getPartition(
		SectorAccessibleDisk& disk, unsigned partition);
};

class DiskPartition : public SectorAccessibleDisk
{
public:
	DiskPartition(SectorAccessibleDisk& parent,
	              unsigned start, unsigned length);

private:
	virtual void readSectorImpl(unsigned sector, byte* buf);
	virtual void writeSectorImpl(unsigned sector, const byte* buf);
	virtual unsigned getNbSectorsImpl() const;
	virtual bool isWriteProtectedImpl() const;

	SectorAccessibleDisk& parent;
	const unsigned start;
	const unsigned length;
};

} // namespace openmsx

#endif
