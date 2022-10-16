#include "DiskImageUtils.hh"
#include "DiskPartition.hh"
#include "CommandException.hh"
#include "BootBlocks.hh"
#include "endian.hh"
#include "enumerate.hh"
#include "random.hh"
#include "xrange.hh"
#include <cassert>
#include <ctime>

namespace openmsx::DiskImageUtils {

static constexpr std::array<char, 11> PARTAB_HEADER = {
	'\353', '\376', '\220', 'M', 'S', 'X', '_', 'I', 'D', 'E', ' '
};
[[nodiscard]] static bool isPartitionTableSector(const PartitionTable& pt)
{
	return pt.header == PARTAB_HEADER;
}

bool hasPartitionTable(SectorAccessibleDisk& disk)
{
	SectorBuffer buf;
	disk.readSector(0, buf);
	return isPartitionTableSector(buf.pt);
}


static Partition& checkImpl(
	SectorAccessibleDisk& disk, unsigned partition, SectorBuffer& buf)
{
	// check number in range
	if (partition < 1 || partition > 31) {
		throw CommandException(
			"Invalid partition number specified (must be 1-31).");
	}
	// check drive has a partition table
	disk.readSector(0, buf);
	if (!isPartitionTableSector(buf.pt)) {
		throw CommandException(
			"No (or invalid) partition table.");
	}
	// check valid partition number
	auto& p = buf.pt.part[31 - partition];
	if (p.start == 0) {
		throw CommandException("No partition number ", partition);
	}
	return p;
}

void checkValidPartition(SectorAccessibleDisk& disk, unsigned partition)
{
	SectorBuffer buf;
	checkImpl(disk, partition, buf);
}

void checkFAT12Partition(SectorAccessibleDisk& disk, unsigned partition)
{
	SectorBuffer buf;
	Partition& p = checkImpl(disk, partition, buf);

	// check partition type
	if (p.sys_ind != 0x01) {
		throw CommandException("Only FAT12 partitions are supported.");
	}
}


// Create a correct boot sector depending on the required size of the filesystem
struct SetBootSectorResult {
	unsigned firstDataSector;
	uint8_t descriptor;
};
static SetBootSectorResult setBootSector(
	MSXBootSector& boot, size_t nbSectors, bool dos1)
{
	// start from the default boot block ..
	const auto& defaultBootBlock = dos1 ? BootBlocks::dos1BootBlock : BootBlocks::dos2BootBlock;
	boot = defaultBootBlock.bootSector;

	// .. and fill-in image-size dependent parameters ..
	// these are the same for most formats
	uint8_t nbReservedSectors = 1;
	uint8_t nbHiddenSectors = 1;

	// all these are initialized below (in this order)
	uint16_t nbSides;
	uint8_t nbFats;
	uint8_t nbSectorsPerFat;
	uint8_t nbSectorsPerCluster;
	uint16_t nbDirEntry;
	uint8_t descriptor;

	// now set correct info according to size of image (in sectors!)
	// and using the same layout as used by Jon in IDEFDISK v 3.1
	if (nbSectors > 32732) {
		// 32732 < nbSectors
		// note: this format is only valid for nbSectors <= 65536
		nbSides = 32;		// copied from a partition from an IDE HD
		nbFats = 2;
		nbSectorsPerFat = 12;	// copied from a partition from an IDE HD
		nbSectorsPerCluster = 16;
		nbDirEntry = 256;
		descriptor = 0xF0;
		nbHiddenSectors = 16;	// override default from above
	} else if (nbSectors > 16388) {
		// 16388 < nbSectors <= 32732
		nbSides = 2;		// unknown yet
		nbFats = 2;
		nbSectorsPerFat = 12;
		nbSectorsPerCluster = 8;
		nbDirEntry = 256;
		descriptor = 0XF0;
	} else if (nbSectors > 8212) {
		// 8212 < nbSectors <= 16388
		nbSides = 2;		// unknown yet
		nbFats = 2;
		nbSectorsPerFat = 12;
		nbSectorsPerCluster = 4;
		nbDirEntry = 256;
		descriptor = 0xF0;
	} else if (nbSectors > 4126) {
		// 4126 < nbSectors <= 8212
		nbSides = 2;		// unknown yet
		nbFats = 2;
		nbSectorsPerFat = 12;
		nbSectorsPerCluster = 2;
		nbDirEntry = 256;
		descriptor = 0xF0;
	} else if (nbSectors > 2880) {
		// 2880 < nbSectors <= 4126
		nbSides = 2;		// unknown yet
		nbFats = 2;
		nbSectorsPerFat = 6;
		nbSectorsPerCluster = 2;
		nbDirEntry = 224;
		descriptor = 0xF0;
	} else if (nbSectors > 1440) {
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

	boot.nrSectors = uint16_t(nbSectors); // TODO check for overflow
	boot.nrSides = nbSides;
	boot.spCluster = nbSectorsPerCluster;
	boot.nrFats = nbFats;
	boot.sectorsFat = nbSectorsPerFat;
	boot.dirEntries = nbDirEntry;
	boot.descriptor = descriptor;
	boot.resvSectors = nbReservedSectors;
	boot.hiddenSectors = nbHiddenSectors;

	if (!dos1) {
		// set random volume id
		boot.vol_id = random_32bit() & 0x7F7F7F7F; // why are bits masked?
	}
	unsigned nbRootDirSectors = nbDirEntry / 16;
	unsigned rootDirStart = 1 + nbFats * nbSectorsPerFat;
	unsigned firstDataSector = rootDirStart + nbRootDirSectors;
	return {firstDataSector, descriptor};
}

void format(SectorAccessibleDisk& disk, bool dos1)
{
	// first create a boot sector for given partition size
	size_t nbSectors = disk.getNbSectors();
	SectorBuffer buf;
	auto [firstDataSector, descriptor] = setBootSector(buf.bootSector, nbSectors, dos1);
	disk.writeSector(0, buf);

	// write empty FAT and directory sectors
	ranges::fill(buf.raw, 0);
	for (auto i : xrange(2u, firstDataSector)) {
		disk.writeSector(i, buf);
	}
	// first FAT sector is special:
	//  - first byte contains the media descriptor
	//  - first two clusters must be marked as EOF
	buf.raw[0] = descriptor;
	buf.raw[1] = 0xFF;
	buf.raw[2] = 0xFF;
	disk.writeSector(1, buf);

	// write 'empty' data sectors
	ranges::fill(buf.raw, 0xE5);
	for (auto i : xrange(firstDataSector, nbSectors)) {
		disk.writeSector(i, buf);
	}
}

struct CHS {
	unsigned cylinder;
	unsigned head;
	unsigned sector;
};
[[nodiscard]] static constexpr CHS logicalToCHS(unsigned logical)
{
	// This is made to fit the openMSX harddisk configuration:
	//  32 sectors/track   16 heads
	unsigned tmp = logical + 1;
	unsigned sector = tmp % 32;
	if (sector == 0) sector = 32;
	tmp = (tmp - sector) / 32;
	unsigned head = tmp % 16;
	unsigned cylinder = tmp / 16;
	return {cylinder, head, sector};
}

void partition(SectorAccessibleDisk& disk, std::span<const unsigned> sizes)
{
	assert(sizes.size() <= 31);

	SectorBuffer buf;
	ranges::fill(buf.raw, 0);
	buf.pt.header = PARTAB_HEADER;
	buf.pt.end = 0xAA55;

	unsigned partitionOffset = 1;
	for (auto [i, size] : enumerate(sizes)) {
		unsigned partitionNbSectors = size;
		auto& p = buf.pt.part[30 - i];
		auto [startCylinder, startHead, startSector] =
			logicalToCHS(partitionOffset);
		auto [endCylinder, endHead, endSector] =
			logicalToCHS(partitionOffset + partitionNbSectors - 1);
		p.boot_ind = (i == 0) ? 0x80 : 0x00; // boot flag
		p.head = startHead;
		p.sector = startSector;
		p.cyl = startCylinder;
		p.sys_ind = 0x01; // FAT12
		p.end_head = endHead;
		p.end_sector = endSector;
		p.end_cyl = endCylinder;
		p.start = partitionOffset;
		p.size = size;
		DiskPartition diskPartition(disk, partitionOffset, partitionNbSectors);
		format(diskPartition);
		partitionOffset += partitionNbSectors;
	}
	disk.writeSector(0, buf);
}

} // namespace openmsx::DiskImageUtils
