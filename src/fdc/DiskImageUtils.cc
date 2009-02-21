// $Id$

#include "DiskImageUtils.hh"
#include "DiskPartition.hh"
#include "CommandException.hh"
#include "StringOp.hh"
#include "BootBlocks.hh"
#include <cstdlib>
#include <cstring>
#include <cassert>
#include <ctime>

namespace openmsx {
namespace DiskImageUtils {

static const unsigned SECTOR_SIZE = SectorAccessibleDisk::SECTOR_SIZE;

struct MSXBootSector {
	byte jumpcode[3];        // 0xE5 to bootprogram
	byte name[8];            //
	byte bpsector[2];        // bytes per sector (always 512)
	byte spcluster[1];       // sectors per cluster (always 2)
	byte reservedsectors[2]; // amount of non-data sectors (ex bootsector)
	byte nrfats[1];          // number of fats
	byte direntries[2];      // max number of files in root directory
	byte nrsectors[2];       // number of sectors on this disk
	byte descriptor[1];      // media descriptor
	byte sectorsfat[2];      // sectors per FAT
	byte sectorstrack[2];    // sectors per track
	byte nrsides[2];         // number of sides
	byte hiddensectors[2];   // not used
	byte bootprogram[512-30];// actual bootprogram
};

static inline void write16LE(byte* x, unsigned y)
{
	x[0] = (y >> 0) & 0xFF;
	x[1] = (y >> 8) & 0xFF;
}
static inline void write32LE(byte* x, unsigned y)
{
	x[0] = (y >>  0) & 0xFF;
	x[1] = (y >>  8) & 0xFF;
	x[2] = (y >> 16) & 0xFF;
	x[3] = (y >> 24) & 0xFF;
}
static inline unsigned read32LE(const byte* x)
{
	return (x[0] << 0) + (x[1] << 8) + (x[2] << 16) + (x[3] << 24);
}


static const char* const PARTAB_HEADER= "\353\376\220MSX_IDE ";

static bool isPartitionTableSector(byte* buf)
{
	return memcmp(buf, PARTAB_HEADER, 11) == 0;
}

bool hasPartitionTable(SectorAccessibleDisk& disk)
{
	byte buf[SECTOR_SIZE];
	disk.readSector(0, buf);
	return isPartitionTableSector(buf);
}


static Partition* checkImpl(SectorAccessibleDisk& disk, unsigned partition, byte* buf)
{
	// check number in range
	if (partition < 1 || partition > 31) {
		throw CommandException(
			"Invalid partition number specified (must be 1-31).");
	}
	// check drive has a partition table
	disk.readSector(0, buf);
	if (!isPartitionTableSector(buf)) {
		throw CommandException(
			"No (or invalid) partition table.");
	}
	// check valid partition number
	Partition* p = reinterpret_cast<Partition*>(&buf[14 + (31 - partition) * 16]);
	if (read32LE(p->start4) == 0) {
		throw CommandException(
			"No partition number " + StringOp::toString(partition));
	}
	return p;
}

void checkValidPartition(SectorAccessibleDisk& disk, unsigned partition)
{
	byte buf[SECTOR_SIZE];
	checkImpl(disk, partition, buf);
}

void checkFAT12Partition(SectorAccessibleDisk& disk, unsigned partition)
{
	byte buf[SECTOR_SIZE];
	Partition* p = checkImpl(disk, partition, buf);

	// check partition type
	if (p->sys_ind != 0x01) {
		throw CommandException(
			"Only FAT12 partitions are supported.");
	}
}


// Create a correct bootsector depending on the required size of the filesystem
static void setBootSector(byte* buf, unsigned nbSectors,
                          unsigned& firstDataSector, byte& descriptor)
{
	// start from the default bootblock ..
	const byte* defaultBootBlock = BootBlocks::dos2BootBlock;
	memcpy(buf, defaultBootBlock, SECTOR_SIZE);

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
		nbSides = 32;		// copied from a partition from an IDE HD
		nbFats = 2;		// unknown yet
		nbSectorsPerFat = 12;	// copied from a partition from an IDE HD
		nbSectorsPerCluster = 16;
		nbDirEntry = 256;
		descriptor = 0xF0;
		nbHiddenSectors = 16;	// override default from above
	} else if (nbSectors > 16388) {
		nbSides = 2;		// unknown yet
		nbFats = 2;		// unknown yet
		nbSectorsPerFat = 3;	// unknown yet
		nbSectorsPerCluster = 8;
		nbDirEntry = 256;
		descriptor = 0XF0;
	} else if (nbSectors > 8212) {
		nbSides = 2;		// unknown yet
		nbFats = 2;		// unknown yet
		nbSectorsPerFat = 3;	// unknown yet
		nbSectorsPerCluster = 4;
		nbDirEntry = 256;
		descriptor = 0xF0;
	} else if (nbSectors > 4126) {
		nbSides = 2;		// unknown yet
		nbFats = 2;		// unknown yet
		nbSectorsPerFat = 3;	// unknown yet
		nbSectorsPerCluster = 2;
		nbDirEntry = 256;
		descriptor = 0xF0;
	} else if (nbSectors > 2880) {
		nbSides = 2;		// unknown yet
		nbFats = 2;		// unknown yet
		nbSectorsPerFat = 3;	// unknown yet
		nbSectorsPerCluster = 1;
		nbDirEntry = 224;
		descriptor = 0xF0;
	} else if (nbSectors > 1440) {
		nbSides = 2;		// unknown yet
		nbFats = 2;		// unknown yet
		nbSectorsPerFat = 3;	// unknown yet
		nbSectorsPerCluster = 2;
		nbDirEntry = 112;
		descriptor = 0xF0;
	} else if (nbSectors > 720) {
		// normal double sided disk
		nbSides = 2;
		nbFats = 2;
		nbSectorsPerFat = 3;
		nbSectorsPerCluster = 2;
		nbDirEntry = 112;
		descriptor = 0xF9;
		nbSectors = 1440;	// force nbSectors to 1440, why?
	} else {
		// normal single sided disk
		nbSides = 1;
		nbFats = 2;
		nbSectorsPerFat = 2;
		nbSectorsPerCluster = 2;
		nbDirEntry = 112;
		descriptor = 0xF8;
		nbSectors = 720;	// force nbSectors to 720, why?
	}
	MSXBootSector* boot = reinterpret_cast<MSXBootSector*>(buf);

	write16LE(boot->nrsectors, nbSectors);
	write16LE(boot->nrsides, nbSides);
	boot->spcluster[0] = nbSectorsPerCluster;
	boot->nrfats[0] = nbFats;
	write16LE(boot->sectorsfat, nbSectorsPerFat);
	write16LE(boot->direntries, nbDirEntry);
	boot->descriptor[0] = descriptor;
	write16LE(boot->reservedsectors, nbReservedSectors);
	write16LE(boot->hiddensectors, nbHiddenSectors);

	// set random volume id
	static bool init = false;
	if (!init) {
		init = true;
		srand(unsigned(time(0)));
	}
	for (int i = 0x27; i < 0x2B; ++i) {
		buf[i] = rand() & 0x7F;
	}

	unsigned nbRootDirSectors = nbDirEntry / 16;
	unsigned rootDirStart = 1 + nbFats * nbSectorsPerFat;
	firstDataSector = rootDirStart + nbRootDirSectors;
}

void format(SectorAccessibleDisk& disk)
{
	// first create a bootsector for given partition size
	unsigned nbSectors = disk.getNbSectors();
	byte buf[SECTOR_SIZE];
	unsigned firstDataSector;
	byte descriptor;
	setBootSector(buf, nbSectors, firstDataSector, descriptor);
	disk.writeSector(0, buf);

	// write empty FAT and directory sectors
	memset(buf, 0x00, SECTOR_SIZE);
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
	memset(buf, 0xE5, SECTOR_SIZE);
	for (unsigned i = firstDataSector; i < nbSectors; ++i) {
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

void partition(SectorAccessibleDisk& disk, std::vector<unsigned> sizes)
{
	assert(sizes.size() <= 31);

	byte buf[SECTOR_SIZE];
	memset(buf, 0, SECTOR_SIZE);
	memcpy(buf, PARTAB_HEADER, 11);
	buf[SECTOR_SIZE - 2] = 0x55;
	buf[SECTOR_SIZE - 1] = 0xAA;

	unsigned partitionOffset = 1;
	for (unsigned i = 0; i < sizes.size(); ++i) {
		unsigned partitionNbSectors = sizes[i];
		Partition* p = reinterpret_cast<Partition*>(
			&buf[14 + (30 - i) * 16]);
		unsigned startCylinder, startHead, startSector;
		logicalToCHS(partitionOffset,
		             startCylinder, startHead, startSector);
		unsigned endCylinder, endHead, endSector;
		logicalToCHS(partitionOffset + partitionNbSectors - 1,
		             endCylinder, endHead, endSector);
		p->boot_ind = (i == 0) ? 0x80 : 0x00; // bootflag
		p->head = startHead;
		p->sector = startSector;
		p->cyl = startCylinder;
		p->sys_ind = 0x01; // FAT12
		p->end_head = endHead;
		p->end_sector = endSector;
		p->end_cyl = endCylinder;
		write32LE(p->start4, partitionOffset);
		write32LE(p->size4, sizes[i]);
		DiskPartition diskPartition(disk, partitionOffset, partitionNbSectors);
		format(diskPartition);
		partitionOffset += partitionNbSectors;
	}
	disk.writeSector(0, buf);
}

} // namespace DiskImageUtils
} // namespace openmsx
