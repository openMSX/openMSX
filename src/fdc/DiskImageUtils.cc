#include "DiskImageUtils.hh"
#include "DiskPartition.hh"
#include "CommandException.hh"
#include "BootBlocks.hh"
#include "endian.hh"
#include "enumerate.hh"
#include "one_of.hh"
#include "random.hh"
#include "ranges.hh"
#include "strCat.hh"
#include "view.hh"
#include "xrange.hh"
#include <algorithm>
#include <bit>
#include <cassert>
#include <ctime>

namespace openmsx::DiskImageUtils {

static constexpr unsigned DOS1_MAX_CLUSTER_COUNT = 0x3FE;  // Max 3 sectors per FAT
static constexpr unsigned FAT12_MAX_CLUSTER_COUNT = 0xFF4;
static constexpr unsigned FAT16_MAX_CLUSTER_COUNT = 0xFFF4;
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

bool hasPartitionTable(const SectorAccessibleDisk& disk)
{
	SectorBuffer buf;
	disk.readSector(0, buf);
	return getPartitionTableType(buf).has_value();
}

// Get partition from Nextor extended boot record (standard EBR) chain.
static Partition& getPartitionNextorExtended(
	const SectorAccessibleDisk& disk, unsigned partition, SectorBuffer& buf,
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
		const auto& link = buf.ptNextor.part[1];
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
	const SectorAccessibleDisk& disk, unsigned partition, SectorBuffer& buf)
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

Partition& getPartition(const SectorAccessibleDisk& disk, unsigned partition, SectorBuffer& buf)
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

void checkSupportedPartition(const SectorAccessibleDisk& disk, unsigned partition)
{
	SectorBuffer buf;
	const Partition& p = getPartition(disk, partition, buf);

	// check partition type
	if (p.sys_ind != one_of(0x01, 0x04, 0x06, 0x0E)) {
		throw CommandException("Only FAT12 and FAT16 partitions are supported.");
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
	bool fat16;
};
static SetBootSectorResult setBootSector(
	MSXBootSector& boot, MSXBootSectorType bootType, size_t nbSectors)
{
	// start from the default boot block ..
	if (bootType == MSXBootSectorType::DOS1) {
		boot = BootBlocks::dos1BootBlock.bootSector;
	} else if (bootType == MSXBootSectorType::DOS2) {
		boot = BootBlocks::dos2BootBlock.bootSector;
	} else if (bootType == MSXBootSectorType::NEXTOR && nbSectors > 0x10000) {
		boot = BootBlocks::nextorBootBlockFAT16.bootSector;
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
	bool fat16 = false;

	// now set correct info according to size of image (in sectors!)
	if (bootType == MSXBootSectorType::NEXTOR && nbSectors > 0x10000) {
		// using the same layout as Nextor 2.1.1’s FDISK
		nbSides = 0;
		nbFats = 2;
		nbDirEntry = 512;
		descriptor = 0xF0;
		nbHiddenSectors = 0;
		fat16 = true;

		// <= 128 MB: 4, <= 256 MB: 8, ..., <= 4 GB: 128
		nbSectorsPerCluster = narrow<uint8_t>(std::clamp(std::bit_ceil(nbSectors) >> 16, size_t(4), size_t(128)));

		// Calculate fat size based on cluster count estimate
		unsigned fatStart = nbReservedSectors + nbDirEntry / DIR_ENTRIES_PER_SECTOR;
		auto estSectorCount = narrow<unsigned>(nbSectors - fatStart);
		auto estClusterCount = std::min(estSectorCount / nbSectorsPerCluster, FAT16_MAX_CLUSTER_COUNT);
		auto fatSize = 2 * (estClusterCount + 2);
		nbSectorsPerFat = narrow<uint16_t>((fatSize + SECTOR_SIZE - 1) / SECTOR_SIZE);  // round up

		// Adjust sectors count down to match cluster count
		auto dataStart = fatStart + nbFats * nbSectorsPerFat;
		auto dataSectorCount = narrow<unsigned>(nbSectors - dataStart);
		auto clusterCount = std::min(dataSectorCount / nbSectorsPerCluster, FAT16_MAX_CLUSTER_COUNT);
		nbSectors = dataStart + clusterCount * nbSectorsPerCluster;
	} else if (bootType == MSXBootSectorType::NEXTOR) {
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
		auto estSectorCount = narrow<unsigned>(nbSectors - fatStart);
		auto estClusterCount = std::min(estSectorCount / nbSectorsPerCluster, maxClusterCount);
		auto fatSize = (3 * (estClusterCount + 2) + 1) / 2;  // round up
		nbSectorsPerFat = narrow<uint16_t>((fatSize + SECTOR_SIZE - 1) / SECTOR_SIZE);  // round up

		// Adjust sectors count down to match cluster count
		auto dataStart = fatStart + nbFats * nbSectorsPerFat;
		auto dataSectorCount = narrow<unsigned>(nbSectors - dataStart);
		auto clusterCount = std::min(dataSectorCount / nbSectorsPerCluster, maxClusterCount);
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
		auto estSectorCount = narrow<unsigned>(nbSectors - fatStart);
		auto estClusterCount = std::min(estSectorCount / nbSectorsPerCluster, DOS1_MAX_CLUSTER_COUNT);
		auto fatSize = (3 * (estClusterCount + 2) + 1) / 2;  // round up
		nbSectorsPerFat = narrow<uint16_t>((fatSize + SECTOR_SIZE - 1) / SECTOR_SIZE);  // round up

		// Adjust sectors count down to match cluster count
		auto dataStart = fatStart + nbFats * nbSectorsPerFat;
		auto dataSectorCount = narrow<unsigned>(nbSectors - dataStart);
		auto clusterCount = std::min(dataSectorCount / nbSectorsPerCluster, DOS1_MAX_CLUSTER_COUNT);
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
	} else if (bootType == MSXBootSectorType::NEXTOR && fat16) {
		boot.nrSectors = 0;
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
	} else if (bootType == MSXBootSectorType::DOS2 || (bootType == MSXBootSectorType::NEXTOR && !fat16)) {
		auto& params = boot.params.dos2;
		params.hiddenSectors = nbHiddenSectors;
		params.vol_id = vol_id;
	} else if (bootType == MSXBootSectorType::NEXTOR && fat16) {
		auto& params = boot.params.extended;
		if (nbSectors <= 0x80'0000) {
			params.nrSectors = narrow<unsigned>(nbSectors);
		} else {
			throw CommandException("Too many sectors for FAT16 ", nbSectors);
		}
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
	result.fat16 = fat16;
	return result;
}

void format(SectorAccessibleDisk& disk, MSXBootSectorType bootType)
{
	format(disk, bootType, disk.getNbSectors());
}

void format(SectorAccessibleDisk& disk, MSXBootSectorType bootType, size_t nbSectors)
{
	// first create a boot sector for given partition size
	nbSectors = std::min(nbSectors, disk.getNbSectors());
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
	if (result.fat16) {
		buf.raw[3] = 0xFF;
	}
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

static std::vector<unsigned> clampPartitionSizes(std::span<const unsigned> sizes,
	size_t diskSize, unsigned initialOffset, unsigned perPartitionOffset)
{
	std::vector<unsigned> clampedSizes;
	size_t sizeRemaining = std::min(diskSize, size_t(std::numeric_limits<unsigned>::max()));

	if (sizeRemaining >= initialOffset) {
		sizeRemaining -= initialOffset;
	} else {
		return clampedSizes;
	}

	for (auto size : sizes) {
		if (sizeRemaining <= perPartitionOffset) {
			break;
		}
		sizeRemaining -= perPartitionOffset;
		if (size <= perPartitionOffset) {
			throw CommandException("Partition size too small: ", size);
		}
		size -= perPartitionOffset;
		if (sizeRemaining > size) {
			clampedSizes.push_back(size);
			sizeRemaining -= size;
		} else {
			clampedSizes.push_back(narrow<unsigned>(sizeRemaining));
			break;
		}
	}
	return clampedSizes;
}

// Partition with standard master / extended boot record (MBR / EBR) for Nextor.
static std::vector<unsigned> partitionNextor(SectorAccessibleDisk& disk, std::span<const unsigned> sizes)
{
	SectorBuffer buf;
	auto& pt = buf.ptNextor;

	std::vector<unsigned> clampedSizes = clampPartitionSizes(sizes, disk.getNbSectors(), 0, 1);

	if (clampedSizes.empty()) {
		ranges::fill(buf.raw, 0);
		pt.header = NEXTOR_PARTITION_TABLE_HEADER;
		pt.end = 0xAA55;
		disk.writeSector(0, buf);
		return clampedSizes;
	}

	unsigned ptSector = 0;
	for (auto [i, size] : enumerate(clampedSizes)) {
		ranges::fill(buf.raw, 0);
		if (i == 0) {
			pt.header = NEXTOR_PARTITION_TABLE_HEADER;
		}
		pt.end = 0xAA55;

		// Add partition entry
		auto& p = pt.part[0];
		p.boot_ind = (i == 0) ? 0x80 : 0x00; // boot flag
		p.sys_ind = size > 0x1'0000 ? 0x0E : 0x01; // FAT16B (LBA), or FAT12
		p.start = 1;
		p.size = size;

		// Add link if not the last partition
		if (i != clampedSizes.size() - 1) {
			auto& link = pt.part[1];
			link.sys_ind = 0x05; // EBR
			if (i == 0) {
				link.start = ptSector + 1 + size;
				link.size = sum(view::drop(sizes, 1), [](unsigned s) { return 1 + s; });
			} else {
				link.start = ptSector + 1 + size - (1 + clampedSizes[0]);
				link.size = 1 + clampedSizes[i + 1];
			}
		}

		disk.writeSector(ptSector, buf);

		ptSector += 1 + size;
	}
	return clampedSizes;
}

// Partition with Sunrise IDE master boot record.
static std::vector<unsigned> partitionSunrise(SectorAccessibleDisk& disk, std::span<const unsigned> sizes)
{
	SectorBuffer buf;
	auto& pt = buf.ptSunrise;

	std::vector<unsigned> clampedSizes = clampPartitionSizes(sizes, disk.getNbSectors(), 1, 0);
	if (clampedSizes.size() > pt.part.size()) {
		clampedSizes.resize(pt.part.size());
	}

	ranges::fill(buf.raw, 0);
	pt.header = SUNRISE_PARTITION_TABLE_HEADER;
	pt.end = 0xAA55;

	unsigned partitionOffset = 1;
	for (auto [i, size] : enumerate(clampedSizes)) {
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
	return clampedSizes;
}

// Partition with standard master boot record (MBR) for Beer IDE 1.9.
static std::vector<unsigned> partitionBeer(SectorAccessibleDisk& disk, std::span<const unsigned> sizes)
{
	SectorBuffer buf;
	auto& pt = buf.ptNextor;

	std::vector<unsigned> clampedSizes = clampPartitionSizes(sizes, disk.getNbSectors(), 1, 0);
	if (clampedSizes.size() > pt.part.size()) {
		clampedSizes.resize(pt.part.size());
	}

	ranges::fill(buf.raw, 0);
	pt.header = NEXTOR_PARTITION_TABLE_HEADER; // TODO: Find out BEER IDE signature
	pt.end = 0xAA55;

	unsigned partitionOffset = 1;
	for (auto [i, size] : enumerate(clampedSizes)) {
		auto& p = pt.part[i];
		p.boot_ind = (i == 0) ? 0x80 : 0x00; // boot flag
		p.sys_ind = 0x01; // FAT12
		p.start = partitionOffset;
		p.size = size;
		partitionOffset += size;
	}

	disk.writeSector(0, buf);
	return clampedSizes;
}

unsigned partition(SectorAccessibleDisk& disk, std::span<const unsigned> sizes, MSXBootSectorType bootType)
{
	std::vector<unsigned> clampedSizes = [&] {
		if (bootType == MSXBootSectorType::NEXTOR) {
			return partitionNextor(disk, sizes);
		} else if (bootType == MSXBootSectorType::DOS2) {
			return partitionSunrise(disk, sizes);
		} else if (bootType == MSXBootSectorType::DOS1) {
			return partitionBeer(disk, sizes);
		} else {
			UNREACHABLE;
		}
	}();

	for (auto [i, size] : enumerate(clampedSizes)) {
		DiskPartition diskPartition(disk, narrow<unsigned>(i + 1));
		format(diskPartition, bootType, size);
	}

	return narrow<unsigned>(clampedSizes.size());
}

FatTimeDate toTimeDate(time_t totalSeconds)
{
	if (const tm* mtim = localtime(&totalSeconds)) {
		auto time = narrow<uint16_t>(
			(std::min(mtim->tm_sec, 59) >> 1) + (mtim->tm_min << 5) +
			(mtim->tm_hour << 11));
		auto date = narrow<uint16_t>(
			mtim->tm_mday + ((mtim->tm_mon + 1) << 5) +
			(std::clamp(mtim->tm_year + 1900 - 1980, 0, 119) << 9));
		return {time, date};
	}
	return {0, 0};
}

time_t fromTimeDate(FatTimeDate timeDate)
{
	struct tm tm{};
	tm.tm_sec  = std::clamp(((timeDate.time >>  0) & 31) * 2, 0, 60);
	tm.tm_min  = std::clamp(((timeDate.time >>  5) & 63), 0, 59);
	tm.tm_hour = std::clamp(((timeDate.time >> 11) & 31), 0, 23);
	tm.tm_mday = std::clamp((timeDate.date >> 0) & 31, 1, 31);
	tm.tm_mon  = std::clamp(((timeDate.date >> 5) & 15) - 1, 0, 11);
	tm.tm_year = (timeDate.date >> 9) + 1980 - 1900;
	tm.tm_isdst = -1;
	return mktime(&tm);
}

std::string formatAttrib(MSXDirEntry::AttribValue attrib)
{
	using enum MSXDirEntry::Attrib;
	return strCat((attrib & DIRECTORY ? 'd' : '-'),
	              (attrib & READONLY  ? 'r' : '-'),
	              (attrib & HIDDEN    ? 'h' : '-'),
	              (attrib & VOLUME    ? 'v' : '-'),  // TODO check if this is the output of files,l
	              (attrib & ARCHIVE   ? 'a' : '-')); // TODO check if this is the output of files,l
}

} // namespace openmsx::DiskImageUtils
