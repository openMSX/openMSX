// $Id$

#include "DiskImageUtils.hh"
#include "DiskPartition.hh"
#include "CommandException.hh"
#include "StringOp.hh"
#include "BootBlocks.hh"
#include "endian.hh"
#include <cstdlib>
#include <cstring>
#include <cassert>
#include <ctime>

namespace openmsx {
namespace DiskImageUtils {

static const unsigned SECTOR_SIZE = SectorAccessibleDisk::SECTOR_SIZE;

static const char PARTAB_HEADER[11] = {
	'\353', '\376', '\220', 'M', 'S', 'X', '_', 'I', 'D', 'E', ' '
};
static bool isPartitionTableSector(const PartitionTable& pt)
{
	return memcmp(pt.header, PARTAB_HEADER, sizeof(PARTAB_HEADER)) == 0;
}

bool hasPartitionTable(SectorAccessibleDisk& disk)
{
	PartitionTable pt;
	disk.readSector(0, reinterpret_cast<byte*>(&pt));
	return isPartitionTableSector(pt);
}


static Partition& checkImpl(SectorAccessibleDisk& disk, unsigned partition,
                            PartitionTable& pt)
{
	// check number in range
	if (partition < 1 || partition > 31) {
		throw CommandException(
			"Invalid partition number specified (must be 1-31).");
	}
	// check drive has a partition table
	disk.readSector(0, reinterpret_cast<byte*>(&pt));
	if (!isPartitionTableSector(pt)) {
		throw CommandException(
			"No (or invalid) partition table.");
	}
	// check valid partition number
	Partition& p = pt.part[31 - partition];
	if (p.start == 0) {
		throw CommandException(StringOp::Builder() <<
			"No partition number " << partition);
	}
	return p;
}

void checkValidPartition(SectorAccessibleDisk& disk, unsigned partition)
{
	PartitionTable pt;
	checkImpl(disk, partition, pt);
}

void checkFAT12Partition(SectorAccessibleDisk& disk, unsigned partition)
{
	PartitionTable pt;
	Partition& p = checkImpl(disk, partition, pt);

	// check partition type
	if (p.sys_ind != 0x01) {
		throw CommandException("Only FAT12 partitions are supported.");
	}
}


// Create a correct bootsector depending on the required size of the filesystem
static void setBootSector(MSXBootSector& boot, size_t nbSectors,
                          unsigned& firstDataSector, byte& descriptor)
{
	// start from the default bootblock ..
	const byte* defaultBootBlock = BootBlocks::dos2BootBlock;
	memcpy(&boot, defaultBootBlock, SECTOR_SIZE);

	// .. and fill-in image-size dependent parameters ..
	// these are the same for most formats
	byte nbReservedSectors = 1;
	byte nbHiddenSectors = 1;

	// all these are initialized below (in this order)
	word nbSides;
	byte nbFats;
	byte nbSectorsPerFat;
	byte nbSectorsPerCluster;
	word nbDirEntry;

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

	// set random volume id
	static bool init = false;
	if (!init) {
		init = true;
		srand(unsigned(time(nullptr)));
	}
	auto raw = reinterpret_cast<byte*>(&boot);
	for (int i = 0x27; i < 0x2B; ++i) {
		raw[i] = rand() & 0x7F;
	}

	unsigned nbRootDirSectors = nbDirEntry / 16;
	unsigned rootDirStart = 1 + nbFats * nbSectorsPerFat;
	firstDataSector = rootDirStart + nbRootDirSectors;
}

void format(SectorAccessibleDisk& disk)
{
	// first create a bootsector for given partition size
	size_t nbSectors = disk.getNbSectors();
	MSXBootSector boot;
	unsigned firstDataSector;
	byte descriptor;
	setBootSector(boot, nbSectors, firstDataSector, descriptor);
	disk.writeSector(0, reinterpret_cast<byte*>(&boot));

	// write empty FAT and directory sectors
	byte buf[SECTOR_SIZE];
	memset(buf, 0x00, sizeof(buf));
	for (unsigned i = 2; i < firstDataSector; ++i) {
		disk.writeSector(i, buf);
	}
	// first FAT sector is special:
	//  - first byte contains the media descriptor
	//  - first two clusters must be marked as EOF
	buf[0] = descriptor;
	buf[1] = 0xFF;
	buf[2] = 0xFF;
	disk.writeSector(1, buf);

	// write 'empty' data sectors
	memset(buf, 0xE5, sizeof(buf));
	for (size_t i = firstDataSector; i < nbSectors; ++i) {
		disk.writeSector(i, buf);
	}
}

static void logicalToCHS(unsigned logical, unsigned& cylinder,
                         unsigned& head, unsigned& sector)
{
	// This is made to fit the openMSX harddisk configuration:
	//  32 sectors/track   16 heads
	unsigned tmp = logical + 1;
	sector = tmp % 32;
	if (sector == 0) sector = 32;
	tmp = (tmp - sector) / 32;
	head = tmp % 16;
	cylinder = tmp / 16;
}

void partition(SectorAccessibleDisk& disk, const std::vector<unsigned>& sizes)
{
	assert(sizes.size() <= 31);

	PartitionTable pt;
	memset(&pt, 0, sizeof(pt));
	memcpy(pt.header, PARTAB_HEADER, sizeof(PARTAB_HEADER));
	pt.end[0] = 0x55;
	pt.end[1] = 0xAA;

	unsigned partitionOffset = 1;
	for (unsigned i = 0; i < sizes.size(); ++i) {
		unsigned partitionNbSectors = sizes[i];
		Partition& p = pt.part[30 - i];
		unsigned startCylinder, startHead, startSector;
		logicalToCHS(partitionOffset,
		             startCylinder, startHead, startSector);
		unsigned endCylinder, endHead, endSector;
		logicalToCHS(partitionOffset + partitionNbSectors - 1,
		             endCylinder, endHead, endSector);
		p.boot_ind = (i == 0) ? 0x80 : 0x00; // bootflag
		p.head = startHead;
		p.sector = startSector;
		p.cyl = startCylinder;
		p.sys_ind = 0x01; // FAT12
		p.end_head = endHead;
		p.end_sector = endSector;
		p.end_cyl = endCylinder;
		p.start = partitionOffset;
		p.size = sizes[i];
		DiskPartition diskPartition(disk, partitionOffset, partitionNbSectors);
		format(diskPartition);
		partitionOffset += partitionNbSectors;
	}
	disk.writeSector(0, reinterpret_cast<byte*>(&pt));
}

} // namespace DiskImageUtils
} // namespace openmsx
