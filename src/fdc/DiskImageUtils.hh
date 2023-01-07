#ifndef DISK_IMAGE_UTILS_HH
#define DISK_IMAGE_UTILS_HH

#include "AlignedBuffer.hh"
#include "endian.hh"
#include "ranges.hh"
#include <array>
#include <span>

namespace openmsx {

class SectorAccessibleDisk;

struct MSXBootSector {
	std::array<uint8_t, 3>      jumpCode;      // + 0 0xE5 to boot program
	std::array<uint8_t, 8>      name;          // + 3
	Endian::UA_L16              bpSector;      // +11 bytes per sector (always 512)
	uint8_t                     spCluster;     // +13 sectors per cluster (always 2)
	Endian::L16                 resvSectors;   // +14 nb of non-data sectors (incl boot sector)
	uint8_t                     nrFats;        // +16 nb of fats
	Endian::UA_L16              dirEntries;    // +17 max nb of files in root directory
	Endian::UA_L16              nrSectors;     // +19 nb of sectors on this disk (< 65536)
	uint8_t                     descriptor;    // +21 media descriptor
	Endian::L16                 sectorsFat;    // +22 sectors per FAT
	Endian::L16                 sectorsTrack;  // +24 sectors per track (0 for LBA volumes)
	Endian::L16                 nrSides;       // +26 number of sides (heads) (0 for LBA volumes)
	Endian::L16                 hiddenSectors; // +28 not used
	std::array<uint8_t, 2>      pad1;          // +30
	Endian::L32                 nrSectorsBig;  // +32 nb of sectors on this disk (>= 65536)
	std::array<uint8_t, 2>      pad2;          // +36
	uint8_t                     extendedBootSignature;  // +38
	Endian::UA_L32              vol_id;        // +39
	std::array<uint8_t, 512-43> pad3;          // +43
};
static_assert(sizeof(MSXBootSector) == 512);

struct MSXDirEntry {
	static constexpr uint8_t ATT_REGULAR   = 0x00; // Normal file
	static constexpr uint8_t ATT_READONLY  = 0x01; // Read-Only file
	static constexpr uint8_t ATT_HIDDEN    = 0x02; // Hidden file
	static constexpr uint8_t ATT_SYSTEM    = 0x04; // System file
	static constexpr uint8_t ATT_VOLUME    = 0x08; // filename is Volume Label
	static constexpr uint8_t ATT_DIRECTORY = 0x10; // entry is a subdir
	static constexpr uint8_t ATT_ARCHIVE   = 0x20; // Archive bit

	std::array<char, 8 + 3> filename;     // + 0
	uint8_t                 attrib;       // +11
	std::array<uint8_t, 10> reserved;     // +12 unused
	Endian::L16             time;         // +22
	Endian::L16             date;         // +24
	Endian::L16             startCluster; // +26
	Endian::L32             size;         // +28

	[[nodiscard]] auto base() const { return subspan<8>(filename, 0); }
	[[nodiscard]] auto ext () const { return subspan<3>(filename, 8); }

	bool operator==(const MSXDirEntry& other) const = default;
};
static_assert(sizeof(MSXDirEntry) == 32);

// Note: can't use Endian::L32 for 'start' and 'size' because the Partition
//       struct itself is not 4-bytes aligned.
struct Partition {
	uint8_t        boot_ind;   // + 0 0x80 - active
	uint8_t        head;       // + 1 starting head
	uint8_t        sector;     // + 2 starting sector
	uint8_t        cyl;        // + 3 starting cylinder
	uint8_t        sys_ind;    // + 4 what partition type
	uint8_t        end_head;   // + 5 end head
	uint8_t        end_sector; // + 6 end sector
	uint8_t        end_cyl;    // + 7 end cylinder
	Endian::UA_L32 start;      // + 8 starting sector counting from 0
	Endian::UA_L32 size;       // +12 nr of sectors in partition
};
static_assert(sizeof(Partition) == 16);
static_assert(alignof(Partition) == 1, "must not have alignment requirements");

struct PartitionTable {
	std::array<char, 11>      header; // +  0
	std::array<char,  3>      pad;    // +  3
	std::array<Partition, 31> part;   // + 14,+30,..,+494    Not 4-byte aligned!!
	Endian::L16               end;    // +510
};
static_assert(sizeof(PartitionTable) == 512);


// Buffer that can hold a (512-byte) disk sector.
// The main advantages of this type over something like 'byte buf[512]' are:
//  - No need for reinterpret_cast<> when interpreting the data in a
//    specific way (this could in theory cause alignment problems).
//  - This type has a stricter alignment, so memcpy() and memset() can work
//    faster compared to using a raw byte array.
union SectorBuffer {
	std::array<uint8_t, 512>    raw;        // raw byte data
	MSXBootSector               bootSector; // interpreted as bootSector
	std::array<MSXDirEntry, 16> dirEntry;   // interpreted as 16 dir entries
	PartitionTable              pt;         // interpreted as Sunrise-IDE partition table
	AlignedBuffer               aligned;    // force big alignment (for faster memcpy)
};
static_assert(sizeof(SectorBuffer) == 512);


namespace DiskImageUtils {

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
	[[nodiscard]] bool hasPartitionTable(SectorAccessibleDisk& disk);

	/** Format the given disk (= a single partition).
	 * The formatting depends on the size of the image.
	 * @param disk the disk/partition image to be formatted
	 * @param dos1 set to true if you want to force dos1 formatting (boot sector)
	 */
	void format(SectorAccessibleDisk& disk, bool dos1 = false);

	/** Write a partition table to the given disk and format each partition
	 * @param disk The disk to partition.
	 * @param sizes The number of sectors for each partition.
	 */
	void partition(SectorAccessibleDisk& disk,
	               std::span<const unsigned> sizes);
};

} // namespace openmsx

#endif
