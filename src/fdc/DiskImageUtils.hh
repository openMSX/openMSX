// $Id$

#ifndef DISK_IMAGE_UTILS_HH
#define DISK_IMAGE_UTILS_HH

#include "openmsx.hh"
#include <vector>
#include <memory>

namespace openmsx {

class SectorAccessibleDisk;

namespace DiskImageUtils {

	struct Partition {
		byte boot_ind;     // 0x80 - active
		byte head;         // starting head
		byte sector;       // starting sector
		byte cyl;          // starting cylinder
		byte sys_ind;      // what partition type
		byte end_head;     // end head
		byte end_sector;   // end sector
		byte end_cyl;      // end cylinder
		byte start4[4];    // starting sector counting from 0
		byte size4[4];     // nr of sectors in partition
	};

	/** Checks whether
	 *   the disk is partitioned
	 *   the specified partition exists
	 * throws a CommandException if one of these conditions is false
	 * @param disk The disk to check.
	 * @param partition Partition number, in range [1..31].
	 */
	void checkValidPartition(SectorAccessibleDisk& disk, unsigned partition);

	/** Like above, but also check whether partition is of type FAT12.
	 */
	void checkFAT12Partition(SectorAccessibleDisk& disk, unsigned partition);

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
	void partition(SectorAccessibleDisk& disk,
	               const std::vector<unsigned>& sizes);
};

} // namespace openmsx

#endif
