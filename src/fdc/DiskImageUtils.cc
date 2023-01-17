#include "DiskImageUtils.hh"
#include "DiskPartition.hh"
#include "CommandException.hh"
#include "BootBlocks.hh"
#include "endian.hh"
#include "enumerate.hh"
#include "one_of.hh"
#include "random.hh"
#include "ranges.hh"
#include "view.hh"
#include "xrange.hh"
#include <algorithm>
#include <bit>
#include <cassert>
#include <ctime>

namespace openmsx::DiskImageUtils {

static constexpr unsigned DOS1_MAX_CLUSTER_COUNT = 0x3FE;  // Max 3 sectors per FAT
static constexpr unsigned FAT12_MAX_CLUSTER_COUNT = 0xFF4;
static constexpr unsigned SECTOR_SIZE = sizeof(SectorBuffer);
static constexpr unsigned DIR_ENTRIES_PER_SECTOR = SECTOR_SIZE / sizeof(MSXDirEntry);

enum class PartitionTableType {
	SUNRISE_IDE,
	NEXTOR
};

static constexpr std::array<char, 11> SUNRISE_PARTITION_TABLE_HEADER = {
	'\353', '\376', '\220', 'M', 'S', 'X', '_', 'I', 'D', 'E', ' '
};
static constexpr std::array<char, 11> NEXTOR_PARTITION_TABLE_HEADER = {
	'\353', '\376', '\220', 'N', 'E', 'X', 'T', 'O', 'R', '2', '0'
};
[[nodiscard]] static std::optional<PartitionTableType> getPartitionTableType(const SectorBuffer& buf)
{
	if (buf.ptSunrise.header == SUNRISE_PARTITION_TABLE_HEADER) {
		return PartitionTableType::SUNRISE_IDE;
	} else if (buf.ptNextor.header == NEXTOR_PARTITION_TABLE_HEADER &&
	           buf.ptNextor.part[0].sys_ind != 0 && // Extra checks to distinguish MBR from VBR
	           buf.ptNextor.part[0].start == 1) {   // since they have the same OEM signature.
		return PartitionTableType::NEXTOR;
	}
	return {};
}

bool hasPartitionTable(SectorAccessibleDisk& disk)
{
	SectorBuffer buf;
	disk.readSector(0, buf);
	return getPartitionTableType(buf).has_value();
}

// Get partition from Nextor extended boot record (standard EBR) chain.
static Partition& getPartitionNextorExtended(
	SectorAccessibleDisk& disk, unsigned partition, SectorBuffer& buf,
	unsigned remaining, unsigned ebrOuterSector)
{
	unsigned ebrSector = ebrOuterSector;
	while (true) {
		disk.readSector(ebrSector, buf);

		if (remaining == 0) {
			// EBR partition entry. Start is relative to *this* EBR sector.
			auto& p = buf.ptNextor.part[0];
			if (p.start == 0) {
				break;
			}
			// Adjust to absolute address before returning.
			p.start = ebrSector + p.start;
			return p;
		}

		// EBR link entry. Start is relative to *outermost* EBR sector.
		auto& link = buf.ptNextor.part[1];
		if (link.start == 0) {
			break;
		} else if (link.sys_ind != one_of(0x05, 0x0F)) {
			throw CommandException("Invalid extended boot record.");
		}
		ebrSector = ebrOuterSector + link.start;
		remaining--;
	}
	throw CommandException("No partition number ", partition);
}

// Get partition from Nextor master boot record (standard MBR).
static Partition& getPartitionNextor(
	SectorAccessibleDisk& disk, unsigned partition, SectorBuffer& buf)
{
	unsigned remaining = partition - 1;
	for (auto& p : buf.ptNextor.part) {
		if (p.start == 0) {
			break;
		} else if (p.sys_ind == one_of(0x05, 0x0F)) {
			return getPartitionNextorExtended(disk, partition, buf, remaining, p.start);
		} else if (remaining == 0) {
			return p;
		}
		remaining--;
	}
	throw CommandException("No partition number ", partition);
}

// Get partition from Sunrise IDE master boot record.
static Partition& getPartitionSunrise(unsigned partition, SectorBuffer& buf)
{
	// check number in range
	if (partition < 1 || partition > buf.ptSunrise.part.size()) {
		throw CommandException(
			"Invalid partition number specified (must be 1-",
			buf.ptSunrise.part.size(), ").");
	}
	// check valid partition number
	auto& p = buf.ptSunrise.part[buf.ptSunrise.part.size() - partition];
	if (p.start == 0) {
		throw CommandException("No partition number ", partition);
	}
	return p;
}

Partition& getPartition(SectorAccessibleDisk& disk, unsigned partition, SectorBuffer& buf)
{
	// check drive has a partition table
	// check valid partition number and return the entry
	disk.readSector(0, buf);
	auto partitionTableType = getPartitionTableType(buf);
	if (partitionTableType == PartitionTableType::SUNRISE_IDE) {
		return getPartitionSunrise(partition, buf);
	} else if (partitionTableType == PartitionTableType::NEXTOR) {
		return getPartitionNextor(disk, partition, buf);
	} else {
		throw CommandException("No (or invalid) partition table.");
	}
}

void checkFAT12Partition(SectorAccessibleDisk& disk, unsigned partition)
{
	SectorBuffer buf;
	Partition& p = getPartition(disk, partition, buf);

	// check partition type
	if (p.sys_ind != 0x01) {
		throw CommandException("Only FAT12 partitions are supported.");
	}
}


// Create a correct boot sector depending on the required size of the filesystem
struct SetBootSectorResult {
	unsigned sectorsPerFat;
	unsigned fatCount;
	unsigned fatStart;
	unsigned rootDirStart;
	unsigned dataStart;
	uint8_t descriptor;
};
static SetBootSectorResult setBootSector(
	MSXBootSector& boot, MSXBootSectorType bootType, size_t nbSectors)
{
	// start from the default boot block ..
	if (bootType == MSXBootSectorType::DOS1) {
		boot = BootBlocks::dos1BootBlock.bootSector;
	} else if (bootType == MSXBootSectorType::DOS2) {
		boot = BootBlocks::dos2BootBlock.bootSector;
	} else if (bootType == MSXBootSectorType::NEXTOR) {
		boot = BootBlocks::nextorBootBlockFAT12.bootSector;
	} else {
		UNREACHABLE;
	}

	// .. and fill-in image-size dependent parameters ..
	// these are the same for most formats
	uint8_t nbReservedSectors = 1;
	uint8_t nbHiddenSectors = 1;
	uint32_t vol_id = random_32bit() & 0x7F7F7F7F; // why are bits masked?;

	// all these are set below (but initialize here to avoid warning)
	uint16_t nbSides = 2;
	uint8_t nbFats = 2;
	uint16_t nbSectorsPerFat = 3;
	uint8_t nbSectorsPerCluster = 2;
	uint16_t nbDirEntry = 112;
	uint8_t descriptor = 0xF9;

	// now set correct info according to size of image (in sectors!)
	if (bootType == MSXBootSectorType::NEXTOR) {
		// using the same layout as Nextor 2.1.1’s FDISK
		nbSides = 0;
		nbFats = 2;
		nbDirEntry = 112;
		descriptor = 0xF0;
		nbHiddenSectors = 0;

		// <= 2 MB: 1, <= 4 MB: 2, ..., <= 32 MB: 16
		nbSectorsPerCluster = narrow<uint8_t>(std::clamp(std::bit_ceil(nbSectors) >> 12, size_t(1), size_t(16)));

		// Partitions <= 16 MB are defined to have at most 3 sectors per FAT,
		// so that they can boot DOS 1. This limits the cluster count to 1022.
		// And for some unknown reason, Nextor limits it to one less than that.
		unsigned maxClusterCount = FAT12_MAX_CLUSTER_COUNT;
		if (nbSectors <= 0x8000) {
			maxClusterCount = DOS1_MAX_CLUSTER_COUNT - 1;
			nbSectorsPerCluster *= 4; // <= 2 MB: 4, <= 4 MB: 8, ..., <= 16 MB: 32
		}

		// Calculate fat size based on cluster count estimate
		unsigned fatStart = nbReservedSectors + nbDirEntry / DIR_ENTRIES_PER_SECTOR;
		unsigned estSectorCount = narrow<unsigned>(nbSectors - fatStart);
		unsigned estClusterCount = std::min(estSectorCount / nbSectorsPerCluster, maxClusterCount);
		unsigned fatSize = (3 * (estClusterCount + 2) + 1) / 2;  // round up
		nbSectorsPerFat = narrow<uint16_t>((fatSize + SECTOR_SIZE - 1) / SECTOR_SIZE);  // round up

		// Adjust sectors count down to match cluster count
		unsigned dataStart = fatStart + nbFats * nbSectorsPerFat;
		unsigned dataSectorCount = narrow<unsigned>(nbSectors - dataStart);
		unsigned clusterCount = std::min(dataSectorCount / nbSectorsPerCluster, maxClusterCount);
		nbSectors = dataStart + clusterCount * nbSectorsPerCluster;
	} else if (bootType == MSXBootSectorType::DOS1 && nbSectors > 1440) {
		// DOS1 supports up to 3 sectors per FAT, limiting the cluster count to 1022.
		nbSides = 0;
		nbFats = 2;
		nbDirEntry = 112;
		descriptor = 0xF0;
		nbHiddenSectors = 0;

		// <= 1 MB: 2, <= 2 MB: 4, ..., <= 32 MB: 64
		nbSectorsPerCluster = narrow<uint8_t>(std::clamp(std::bit_ceil(nbSectors) >> 10, size_t(2), size_t(64)));

		// Calculate fat size based on cluster count estimate
		unsigned fatStart = nbReservedSectors + nbDirEntry / DIR_ENTRIES_PER_SECTOR;
		unsigned estSectorCount = narrow<unsigned>(nbSectors - fatStart);
		unsigned estClusterCount = std::min(estSectorCount / nbSectorsPerCluster, DOS1_MAX_CLUSTER_COUNT);
		unsigned fatSize = (3 * (estClusterCount + 2) + 1) / 2;  // round up
		nbSectorsPerFat = narrow<uint16_t>((fatSize + SECTOR_SIZE - 1) / SECTOR_SIZE);  // round up

		// Adjust sectors count down to match cluster count
		unsigned dataStart = fatStart + nbFats * nbSectorsPerFat;
		unsigned dataSectorCount = narrow<unsigned>(nbSectors - dataStart);
		unsigned clusterCount = std::min(dataSectorCount / nbSectorsPerCluster, DOS1_MAX_CLUSTER_COUNT);
		nbSectors = dataStart + clusterCount * nbSectorsPerCluster;
	} else if (nbSectors > 32732) {
		// using the same layout as used by Jon in IDEFDISK v 3.1
		// 32732 < nbSectors
		// note: this format is only valid for nbSectors <= 65536
		nbSides = 32;		// copied from a partition from an IDE HD
		nbFats = 2;
		nbSectorsPerFat = 12;	// copied from a partition from an IDE HD
		nbSectorsPerCluster = 16;
		nbDirEntry = 256;
		descriptor = 0xF0;
		nbHiddenSectors = 16;	// override default from above
		// for a 32MB disk or greater the sectors would be >= 65536
		// since MSX use 16 bits for this, in case of sectors = 65536
		// the truncated word will be 0 -> formatted as 320 Kb disk!
		if (nbSectors > 65535) nbSectors = 65535; // this is the max size for fat12 :-)
	} else if (nbSectors > 16388) {
		// using the same layout as used by Jon in IDEFDISK v 3.1
		// 16388 < nbSectors <= 32732
		nbSides = 2;		// unknown yet
		nbFats = 2;
		nbSectorsPerFat = 12;
		nbSectorsPerCluster = 8;
		nbDirEntry = 256;
		descriptor = 0XF0;
	} else if (nbSectors > 8212) {
		// using the same layout as used by Jon in IDEFDISK v 3.1
		// 8212 < nbSectors <= 16388
		nbSides = 2;		// unknown yet
		nbFats = 2;
		nbSectorsPerFat = 12;
		nbSectorsPerCluster = 4;
		nbDirEntry = 256;
		descriptor = 0xF0;
	} else if (nbSectors > 4126) {
		// using the same layout as used by Jon in IDEFDISK v 3.1
		// 4126 < nbSectors <= 8212
		nbSides = 2;		// unknown yet
		nbFats = 2;
		nbSectorsPerFat = 12;
		nbSectorsPerCluster = 2;
		nbDirEntry = 256;
		descriptor = 0xF0;
	} else if (nbSectors > 2880) {
		// using the same layout as used by Jon in IDEFDISK v 3.1
		// 2880 < nbSectors <= 4126
		nbSides = 2;		// unknown yet
		nbFats = 2;
		nbSectorsPerFat = 6;
		nbSectorsPerCluster = 2;
		nbDirEntry = 224;
		descriptor = 0xF0;
	} else if (nbSectors > 1440) {
		// using the same layout as used by Jon in IDEFDISK v 3.1
		// 1440 < nbSectors <= 2880
		nbSides = 2;		// unknown yet
		nbFats = 2;
		nbSectorsPerFat = 5;
		nbSectorsPerCluster = 2;
		nbDirEntry = 112;
		descriptor = 0xF0;
	} else if (nbSectors > 720) {
		// normal double sided disk
		// 720 < nbSectors <= 1440
		nbSides = 2;
		nbFats = 2;
		nbSectorsPerFat = 3;
		nbSectorsPerCluster = 2;
		nbDirEntry = 112;
		descriptor = 0xF9;
		nbSectors = 1440;	// force nbSectors to 1440, why?
	} else {
		// normal single sided disk
		// nbSectors <= 720
		nbSides = 1;
		nbFats = 2;
		nbSectorsPerFat = 2;
		nbSectorsPerCluster = 2;
		nbDirEntry = 112;
		descriptor = 0xF8;
		nbSectors = 720;	// force nbSectors to 720, why?
	}

	assert(nbDirEntry % 16 == 0);  // Non multiples of 16 not supported.

	if (nbSectors < 0x10000) {
		boot.nrSectors = narrow<uint16_t>(nbSectors);
	} else {
		throw CommandException("Too many sectors for FAT12 ", nbSectors);
	}

	boot.nrSides = nbSides;
	boot.spCluster = nbSectorsPerCluster;
	boot.nrFats = nbFats;
	boot.sectorsFat = nbSectorsPerFat;
	boot.dirEntries = nbDirEntry;
	boot.descriptor = descriptor;
	boot.resvSectors = nbReservedSectors;

	if (bootType == MSXBootSectorType::DOS1) {
		auto& params = boot.params.dos1;
		params.hiddenSectors = nbHiddenSectors;
	} else if (bootType == MSXBootSectorType::DOS2 || bootType == MSXBootSectorType::NEXTOR) {
		auto& params = boot.params.dos2;
		params.hiddenSectors = nbHiddenSectors;
		params.vol_id = vol_id;
	} else {
		UNREACHABLE;
	}

	SetBootSectorResult result;
	result.sectorsPerFat = nbSectorsPerFat;
	result.fatCount = nbFats;
	result.fatStart = nbReservedSectors;
	result.rootDirStart = result.fatStart + nbFats * nbSectorsPerFat;
	result.dataStart = result.rootDirStart + nbDirEntry / 16;
	result.descriptor = descriptor;
	return result;
}

void format(SectorAccessibleDisk& disk, MSXBootSectorType bootType)
{
	// first create a boot sector for given partition size
	size_t nbSectors = disk.getNbSectors();
	SectorBuffer buf;
	SetBootSectorResult result = setBootSector(buf.bootSector, bootType, nbSectors);
	disk.writeSector(0, buf);

	// write empty FAT sectors (except for first sector, see below)
	ranges::fill(buf.raw, 0);
	for (auto fat : xrange(result.fatCount)) {
		for (auto i : xrange(1u, result.sectorsPerFat)) {
			disk.writeSector(i + result.fatStart + fat * result.sectorsPerFat, buf);
		}
	}

	// write empty directory sectors
	for (auto i : xrange(result.rootDirStart, result.dataStart)) {
		disk.writeSector(i, buf);
	}

	// first FAT sector is special:
	//  - first byte contains the media descriptor
	//  - first two clusters must be marked as EOF
	buf.raw[0] = result.descriptor;
	buf.raw[1] = 0xFF;
	buf.raw[2] = 0xFF;
	for (auto fat : xrange(result.fatCount)) {
		disk.writeSector(result.fatStart + fat * result.sectorsPerFat, buf);
	}

	// write 'empty' data sectors
	ranges::fill(buf.raw, 0xE5);
	for (auto i : xrange(result.dataStart, nbSectors)) {
		disk.writeSector(i, buf);
	}
}

struct CHS {
	unsigned cylinder;
	uint8_t head; // 0-15
	uint8_t sector; // 1-32
};
[[nodiscard]] static constexpr CHS logicalToCHS(unsigned logical)
{
	// This is made to fit the openMSX hard disk configuration:
	//  32 sectors/track   16 heads
	unsigned tmp = logical + 1;
	uint8_t sector = tmp % 32;
	if (sector == 0) sector = 32;
	tmp = (tmp - sector) / 32;
	uint8_t head = tmp % 16;
	unsigned cylinder = tmp / 16;
	return {cylinder, head, sector};
}

static void partitionNextor(SectorAccessibleDisk& disk, std::span<const unsigned> sizes)
{
	SectorBuffer buf;
	auto& pt = buf.ptNextor;

	unsigned ptSector = 0;
	for (auto [i, size] : enumerate(sizes)) {
		ranges::fill(buf.raw, 0);
		if (i == 0) {
			pt.header = NEXTOR_PARTITION_TABLE_HEADER;
		}
		pt.end = 0xAA55;

		// Add partition entry
		auto& p = pt.part[0];
		p.boot_ind = (i == 0) ? 0x80 : 0x00; // boot flag
		p.sys_ind = size > 0x10000 ? 0x0E : 0x01; // FAT16B (LBA), or FAT12
		p.start = 1;
		p.size = size - 1;

		// Add link if not the last partition
		if (i != sizes.size() - 1) {
			auto& link = pt.part[1];
			link.sys_ind = 0x05;
			if (i == 0) {
				link.start = ptSector + size;
				link.size = ranges::accumulate(view::drop(sizes, 1), 0u);
			} else {
				link.start = ptSector + size - sizes[0];
				link.size = sizes[i + 1];
			}
		}

		disk.writeSector(ptSector, buf);

		ptSector += size;
	}
}

static void partitionSunrise(SectorAccessibleDisk& disk, std::span<const unsigned> sizes)
{
	SectorBuffer buf;
	ranges::fill(buf.raw, 0);
	auto& pt = buf.ptSunrise;

	assert(sizes.size() <= pt.part.size());

	pt.header = SUNRISE_PARTITION_TABLE_HEADER;
	pt.end = 0xAA55;

	unsigned partitionOffset = 1;
	for (auto [i, size] : enumerate(sizes)) {
		unsigned partitionNbSectors = size;
		auto& p = pt.part[30 - i];
		auto [startCylinder, startHead, startSector] =
			logicalToCHS(partitionOffset);
		auto [endCylinder, endHead, endSector] =
			logicalToCHS(partitionOffset + partitionNbSectors - 1);
		p.boot_ind = (i == 0) ? 0x80 : 0x00; // boot flag
		p.head = startHead;
		p.sector = startSector;
		p.cyl = narrow_cast<uint8_t>(startCylinder); // wraps for size larger than 64MB
		p.sys_ind = 0x01; // FAT12
		p.end_head = endHead;
		p.end_sector = endSector;
		p.end_cyl = narrow_cast<uint8_t>(endCylinder);
		p.start = partitionOffset;
		p.size = partitionNbSectors;
		partitionOffset += partitionNbSectors;
	}
	disk.writeSector(0, buf);
}

void partition(SectorAccessibleDisk& disk, std::span<const unsigned> sizes, MSXBootSectorType bootType)
{
	if (bootType == MSXBootSectorType::NEXTOR) {
		partitionNextor(disk, sizes);
	} else {
		partitionSunrise(disk, sizes);
	}

	for (auto [i, size] : enumerate(sizes)) {
		DiskPartition diskPartition(disk, narrow<unsigned>(i + 1));
		format(diskPartition, bootType);
	}
}

} // namespace openmsx::DiskImageUtils
