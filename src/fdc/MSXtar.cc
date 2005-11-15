// $Id$

#include "MSXtar.hh"
#include "ReadDir.hh"
#include "SectorAccessibleDisk.hh"
#include "FileOperations.hh"
#include "MSXException.hh"
#include "StringOp.hh"
#include <sys/stat.h>
#include <utime.h>
#include <unistd.h>
#include <algorithm>
#include <cassert>

using std::string;

namespace openmsx {

static const word EOF_FAT = 0x0FFF; // signals EOF in FAT12
static const int SECTOR_SIZE = 512;

static const byte T_MSX_REG  = 0x00; // Normal file
static const byte T_MSX_READ = 0x01; // Read-Only file
static const byte T_MSX_HID  = 0x02; // Hidden file
static const byte T_MSX_SYS  = 0x04; // System file
static const byte T_MSX_VOL  = 0x08; // filename is Volume Label
static const byte T_MSX_DIR  = 0x10; // entry is a subdir
static const byte T_MSX_ARC  = 0x20; // Archive bit

// bootblock created with regular nms8250 and '_format'
static const byte dos1BootBlock[512] =
{
0xeb,0xfe,0x90,0x4e,0x4d,0x53,0x20,0x32,0x2e,0x30,0x50,0x00,0x02,0x02,0x01,0x00,
0x02,0x70,0x00,0xa0,0x05,0xf9,0x03,0x00,0x09,0x00,0x02,0x00,0x00,0x00,0xd0,0xed,
0x53,0x59,0xc0,0x32,0xd0,0xc0,0x36,0x56,0x23,0x36,0xc0,0x31,0x1f,0xf5,0x11,0xab,
0xc0,0x0e,0x0f,0xcd,0x7d,0xf3,0x3c,0xca,0x63,0xc0,0x11,0x00,0x01,0x0e,0x1a,0xcd,
0x7d,0xf3,0x21,0x01,0x00,0x22,0xb9,0xc0,0x21,0x00,0x3f,0x11,0xab,0xc0,0x0e,0x27,
0xcd,0x7d,0xf3,0xc3,0x00,0x01,0x58,0xc0,0xcd,0x00,0x00,0x79,0xe6,0xfe,0xfe,0x02,
0xc2,0x6a,0xc0,0x3a,0xd0,0xc0,0xa7,0xca,0x22,0x40,0x11,0x85,0xc0,0xcd,0x77,0xc0,
0x0e,0x07,0xcd,0x7d,0xf3,0x18,0xb4,0x1a,0xb7,0xc8,0xd5,0x5f,0x0e,0x06,0xcd,0x7d,
0xf3,0xd1,0x13,0x18,0xf2,0x42,0x6f,0x6f,0x74,0x20,0x65,0x72,0x72,0x6f,0x72,0x0d,
0x0a,0x50,0x72,0x65,0x73,0x73,0x20,0x61,0x6e,0x79,0x20,0x6b,0x65,0x79,0x20,0x66,
0x6f,0x72,0x20,0x72,0x65,0x74,0x72,0x79,0x0d,0x0a,0x00,0x00,0x4d,0x53,0x58,0x44,
0x4f,0x53,0x20,0x20,0x53,0x59,0x53,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00
};

// bootblock created with nms8250 and MSX-DOS 2.20
static const byte dos2BootBlock[512] =
{
0xeb,0xfe,0x90,0x4e,0x4d,0x53,0x20,0x32,0x2e,0x30,0x50,0x00,0x02,0x02,0x01,0x00,
0x02,0x70,0x00,0xa0,0x05,0xf9,0x03,0x00,0x09,0x00,0x02,0x00,0x00,0x00,0x18,0x10,
0x56,0x4f,0x4c,0x5f,0x49,0x44,0x00,0x71,0x60,0x03,0x19,0x00,0x00,0x00,0x00,0x00,
0xd0,0xed,0x53,0x6a,0xc0,0x32,0x72,0xc0,0x36,0x67,0x23,0x36,0xc0,0x31,0x1f,0xf5,
0x11,0xab,0xc0,0x0e,0x0f,0xcd,0x7d,0xf3,0x3c,0x28,0x26,0x11,0x00,0x01,0x0e,0x1a,
0xcd,0x7d,0xf3,0x21,0x01,0x00,0x22,0xb9,0xc0,0x21,0x00,0x3f,0x11,0xab,0xc0,0x0e,
0x27,0xcd,0x7d,0xf3,0xc3,0x00,0x01,0x69,0xc0,0xcd,0x00,0x00,0x79,0xe6,0xfe,0xd6,
0x02,0xf6,0x00,0xca,0x22,0x40,0x11,0x85,0xc0,0x0e,0x09,0xcd,0x7d,0xf3,0x0e,0x07,
0xcd,0x7d,0xf3,0x18,0xb8,0x42,0x6f,0x6f,0x74,0x20,0x65,0x72,0x72,0x6f,0x72,0x0d,
0x0a,0x50,0x72,0x65,0x73,0x73,0x20,0x61,0x6e,0x79,0x20,0x6b,0x65,0x79,0x20,0x66,
0x6f,0x72,0x20,0x72,0x65,0x74,0x72,0x79,0x0d,0x0a,0x24,0x00,0x4d,0x53,0x58,0x44,
0x4f,0x53,0x20,0x20,0x53,0x59,0x53,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00
};

// functions to change DirEntries
static inline void setsh(byte* x, word y)
{
	x[0] = (y >> 0) & 0xFF;
	x[1] = (y >> 8) & 0xFF;
}
static inline void setlg(byte* x, int y)
{
	x[0] = (y >>  0) & 0xFF;
	x[1] = (y >>  8) & 0xFF;
	x[2] = (y >> 16) & 0xFF;
	x[3] = (y >> 24) & 0xFF;
}

// functions to read DirEntries
static inline word rdsh(byte* x)
{
	return (x[0] << 0) + (x[1] << 8);
}
static inline int rdlg(byte* x)
{
	return (x[0] << 0) + (x[1] << 8) + (x[2] << 16) + (x[3] << 24);
}

/** Transforms a clusternumber towards the first sector of this cluster
  * The calculation uses info read fom the bootsector
  */
int MSXtar::clusterToSector(int cluster)
{
	return 1 + rootDirEnd + sectorsPerCluster * (cluster - 2);
}

/** Transforms a sectornumber towards it containing cluster
  * The calculation uses info read fom the bootsector
  */
word MSXtar::sectorToCluster(int sector)
{
	return 2 + (int)((sector - (1 + rootDirEnd)) / sectorsPerCluster);
}


/** Initialize object variables by reading info from the bootsector
  */
void MSXtar::parseBootSector(const byte* buf)
{
	MSXBootSector* boot = (MSXBootSector*)buf;
	nbSectors = rdsh(boot->nrsectors);
	nbSides = rdsh(boot->nrsides);
	nbFats = boot->nrfats[0];
	sectorsPerFat = rdsh(boot->sectorsfat);
	nbRootDirSectors = rdsh(boot->direntries) / 16;
	sectorsPerCluster = boot->spcluster[0];
	rootDirStart = 1 + nbFats * sectorsPerFat;
	MSXchrootSector = rootDirStart;
	rootDirEnd = rootDirStart + nbRootDirSectors - 1;
	maxCluster = sectorToCluster(nbSectors);

	//PRT_DEBUG("Bootsector info");
	//PRT_DEBUG("nbSectors         :" << nbSectors);
	//PRT_DEBUG("nbSides           :" << nbSides);
	//PRT_DEBUG("nbFats            :" << nbFats);
	//PRT_DEBUG("sectorsPerFat     :" << sectorsPerFat);
	//PRT_DEBUG("nbRootDirSectors  :" << nbRootDirSectors);
	//PRT_DEBUG("sectorsPerCluster :" << sectorsPerCluster);
	//PRT_DEBUG("rootDirStart      :" << rootDirStart);
	//PRT_DEBUG("rootDirEnd        :" << rootDirEnd);
	//PRT_DEBUG("maxCluster        :" << maxCluster);
}

void MSXtar::parseBootSectorFAT(const byte* buf)
{
	parseBootSector(buf);
	
	// cache complete FAT
	assert(fatBuffer.empty()); // can only cache once
	fatBuffer.resize(SECTOR_SIZE * sectorsPerFat);
	for (int i = 0; i < sectorsPerFat; ++i) {
		disk.readLogicalSector(partitionOffset + 1 + i,
		                       &fatBuffer[SECTOR_SIZE * i]);
	}
}

MSXtar::MSXtar(SectorAccessibleDisk& sectordisk)
	: disk(sectordisk)
{
	nbSectorsPerCluster = 2;
	partitionNbSectors = 0;
	partitionOffset = 0;
	defaultBootBlock = dos2BootBlock;
}

MSXtar::~MSXtar()
{
	// write cached FAT back to disk image
	for (unsigned i = 0; i < fatBuffer.size() / SECTOR_SIZE; ++i) {
		disk.writeLogicalSector(partitionOffset + 1 + i,
		                        &fatBuffer[SECTOR_SIZE * i]);
	}
}

// Create a correct bootsector depending on the required size of the filesystem
void MSXtar::setBootSector(byte* buf, unsigned nbSectors)
{
	// variables set to single sided disk by default
	word nbSides = 1;
	byte nbFats = 2;
	byte nbReservedSectors = 1; // Just copied from a 32MB IDE partition
	byte nbSectorsPerFat = 2;
	byte nbHiddenSectors = 1;
	word nbDirEntry = 112;
	byte descriptor = 0xF8;

	// now set correct info according to size of image (in sectors!)
	// and using the same layout as used by Jon in IDEFDISK v 3.1
	if (nbSectors >= 32733) {
		nbSides = 2;		// unknown yet
		nbFats = 2;		// unknown yet
		nbSectorsPerFat = 12;	// copied from a partition from an IDE HD
		nbSectorsPerCluster = 16;
		nbDirEntry = 256;
		nbSides = 32;		// copied from a partition from an IDE HD
		nbHiddenSectors = 16;
		descriptor = 0xF0;
	} else if (nbSectors >= 16389) {
		nbSides = 2;		// unknown yet
		nbFats = 2;		// unknown yet
		nbSectorsPerFat = 3;	// unknown yet
		nbSectorsPerCluster = 8;
		nbDirEntry = 256;
		descriptor = 0XF0;
	} else if (nbSectors >= 8213) {
		nbSides = 2;		// unknown yet
		nbFats = 2;		// unknown yet
		nbSectorsPerFat = 3;	// unknown yet
		nbSectorsPerCluster = 4;
		nbDirEntry = 256;
		descriptor = 0xF0;
	} else if (nbSectors >= 4127) {
		nbSides = 2;		// unknown yet
		nbFats = 2;		// unknown yet
		nbSectorsPerFat = 3;	// unknown yet
		nbSectorsPerCluster = 2;
		nbDirEntry = 256;
		descriptor = 0xF0;
	} else if (nbSectors >= 2880) {
		nbSides = 2;		// unknown yet
		nbFats = 2;		// unknown yet
		nbSectorsPerFat = 3;	// unknown yet
		nbSectorsPerCluster = 1;
		nbDirEntry = 224;
		descriptor = 0xF0;
	} else if (nbSectors >= 1441) {
		nbSides = 2;		// unknown yet
		nbFats = 2;		// unknown yet
		nbSectorsPerFat = 3;	// unknown yet
		nbSectorsPerCluster = 2;
		nbDirEntry = 112;
		descriptor = 0xF0;

	} else if (nbSectors <= 720) {
		// normal single sided disk
		nbSectors = 720;
	} else {
		// normal double sided disk
		nbSectors = 1440;
		nbSides = 2;
		nbFats = 2;
		nbSectorsPerFat = 3;
		nbSectorsPerCluster = 2;
		nbDirEntry = 112;
		descriptor = 0xF9;
	}
	MSXBootSector* boot = (MSXBootSector*)buf;

	setsh(boot->nrsectors, nbSectors);
	setsh(boot->nrsides, nbSides);
	boot->spcluster[0] = nbSectorsPerCluster;
	boot->nrfats[0] = nbFats;
	setsh(boot->sectorsfat, nbSectorsPerFat);
	setsh(boot->direntries, nbDirEntry);
	boot->descriptor[0] = descriptor;
	setsh(boot->reservedsectors, nbReservedSectors);
	setsh(boot->hiddensectors, nbHiddenSectors);
}

// Format a diskimage with correct bootsector, FAT etc.
void MSXtar::format()
{
	if (hasPartitionTable()) {
		format(partitionNbSectors);
	} else {
		format(disk.getNbSectors());
	}
}

void MSXtar::format(unsigned partitionsectorsize)
{
	// first create a bootsector and start from the default bootblock
	byte sectorbuf[512];
	memcpy(sectorbuf, defaultBootBlock, SECTOR_SIZE);
	setBootSector(sectorbuf, partitionsectorsize);
	parseBootSector(sectorbuf); //set object variables to correct values
	disk.writeLogicalSector(partitionOffset + 0, sectorbuf);

	MSXBootSector* boot = (MSXBootSector*)sectorbuf;
	byte descriptor = boot->descriptor[0];

	// Assign default empty values to disk
	memset(sectorbuf, 0x00, SECTOR_SIZE);
	for (int i = 2; i <= rootDirEnd; ++i) {
		disk.writeLogicalSector(partitionOffset + i, sectorbuf);
	}
	// for some reason the first 3 bytes are used to indicate the end of a
	// cluster, making the first available cluster nr 2 some sources say
	// that this indicates the disk format and FAT[0] should 0xF7 for
	// single sided disk, and 0xF9 for double sided disks
	// TODO: check this :-)
	// for now I simply repeat the media descriptor here
	sectorbuf[0] = descriptor;
	sectorbuf[1] = 0xFF;
	sectorbuf[2] = 0xFF;
	disk.writeLogicalSector(partitionOffset + 1, sectorbuf);

	memset(sectorbuf, 0xE5, SECTOR_SIZE);
	for (unsigned i = 1 + rootDirEnd; i < partitionsectorsize ; ++i) {
		disk.writeLogicalSector(partitionOffset + i, sectorbuf);
	}
}

// Get the next clusternumber from the FAT chain
word MSXtar::readFAT(word clnr) const
{
	assert(!fatBuffer.empty()); // FAT must already be cached
	const byte* p = &fatBuffer[(clnr * 3) / 2];
	return (clnr & 1)
	       ? (p[0] >> 4) + (p[1] << 4)
	       : p[0] + ((p[1] & 0x0F) << 8);
}

// Write an entry to the FAT
void MSXtar::writeFAT(word clnr, word val)
{
	assert(!fatBuffer.empty()); // FAT must already be cached
	byte* p = &fatBuffer[(clnr * 3) / 2];
	if (clnr & 1) {
		p[0] = (p[0] & 0x0F) + (val << 4);
		p[1] = val >> 4;
	} else {
		p[0] = val;
		p[1] = (p[1] & 0xF0) + ((val >> 8) & 0x0F);
	}
}

// Find the next clusternumber marked as free in the FAT
// @throws When no more free clusters
word MSXtar::findFirstFreeCluster()
{
	for (int cluster = 2; cluster < maxCluster; ++cluster) {
		if (readFAT(cluster) == 0) {
			return cluster;
		}
	}
	throw MSXException("Disk full.");
}

// Get the next sector from a file or (root/sub)directory
// If no next sector then 0 is returned
int MSXtar::getNextSector(int sector)
{
	//if this sector is part of the root directory...
	if (sector == rootDirEnd) return 0;
	if (sector <  rootDirEnd) return sector + 1;

	if (sectorToCluster(sector) == sectorToCluster(sector + 1)) {
		return sector + 1;
	} else {
		int nextcl = readFAT(sectorToCluster(sector));
		if (nextcl == EOF_FAT) {
			return 0;
		} else {
			return clusterToSector(nextcl);
		}
	}
}

// If there are no more free entries in a subdirectory, the subdir is
// expanded with an extra cluster. This function gets the free cluster,
// clears it and updates the fat for the subdir
// returns: the first sector in the newly appended cluster
// @throws When disk is full
int MSXtar::appendClusterToSubdir(int sector)
{
	word nextcl = findFirstFreeCluster();
	int logicalSector = clusterToSector(nextcl);

	// clear this cluster
	byte buf[SECTOR_SIZE];
	memset(buf, 0, SECTOR_SIZE);
	for (int i = 0; i < sectorsPerCluster; ++i) {
		disk.writeLogicalSector(partitionOffset + i + logicalSector, buf);
	}

	word curcl = sectorToCluster(sector);
	assert(readFAT(curcl) == EOF_FAT);
	writeFAT(curcl, nextcl);
	writeFAT(nextcl, EOF_FAT);
	return logicalSector;
}


// Returns the index of a free (or with deleted file) entry
//  In:  The current dir sector
//  Out: index number, if no index is found then -1 is returned
int MSXtar::findUsableIndexInSector(int sector)
{
	byte buf[SECTOR_SIZE];
	disk.readLogicalSector(partitionOffset + sector, buf);
	MSXDirEntry* direntry = (MSXDirEntry*)buf;

	// find a not used (0x00) or delete entry (0xE5)
	for (int i = 0; i < 16; ++i) {
		byte tmp = direntry[i].filename[0];
		if ((tmp == 0x00) || (tmp == 0xE5)) {
			return i;
		}
	}
	return -1;
}

// This function returns the sector and dirindex for a new directory entry
// if needed the involved subdirectroy is expanded by an extra cluster
// returns: a DirEntry containing sector and index
// @throws When either root dir is full or disk is full
MSXtar::DirEntry MSXtar::addEntryToDir(int sector)
{
	// this routin adds the msxname to a directory sector, if needed (and
	// possible) the directory is extened with an extra cluster
	DirEntry result;
	result.sector = sector;

	if (sector <= rootDirEnd) {
		// add to the root directory
		for (/* */ ; result.sector <= rootDirEnd; result.sector++) {
			result.index = findUsableIndexInSector(result.sector);
			if (result.index != -1) {
				return result;
			}
		}
		throw MSXException("Root directory full.");
	
	} else {
		// add to a subdir
		while (true) {
			result.index = findUsableIndexInSector(result.sector);
			if (result.index != -1) {
				return result;
			}
			result.sector = getNextSector(result.sector);
			if (result.sector == 0) {
				result.sector = appendClusterToSubdir(result.sector);
			}
		}
	}
}

// create an MSX filename 8.3 format, if needed in vfat like abreviation
static char toMSXChr(char a)
{
	a = toupper(a);
	if (a == ' ' || a == '.') {
		a = '_';
	}
	return a;
}

// Transform a long hostname in a 8.3 uppercase filename as used in the
// direntries on an MSX
string MSXtar::makeSimpleMSXFileName(const string& fullfilename)
{
	string dir, fullfile;
	StringOp::splitOnLast(fullfilename, "/", dir, fullfile);

	// handle speciale case '.' and '..' first
	if ((fullfile == ".") || (fullfile == "..")) {
		fullfile.resize(11, ' ');
		return fullfile;
	}

	string file, ext;
	StringOp::splitOnLast(fullfile, ".", file, ext);
	if (file.empty()) swap(file, ext);

	StringOp::trimRight(file, " ");
	StringOp::trimRight(ext,  " ");

	// put in major case and create '_' if needed
	std::transform(file.begin(), file.end(), file.begin(), toMSXChr);
	std::transform(ext.begin(),  ext.end(),  ext.begin(),  toMSXChr);

	// add correct number of spaces
	file.resize(8, ' ');
	ext .resize(3, ' ');

	return file + ext;
}

// This function creates a new MSX subdir with given date 'd' and time 't'
// in the subdir pointed at by 'sector'. In the newly
// created subdir the entries for '.' and '..' are created
// returns: the first sector of the new subdir
// @throws in case no directory could be created
int MSXtar::addMSXSubdir(const string& msxName, int t, int d, int sector)
{
	// returns the sector for the first cluster of this subdir
	DirEntry result = addEntryToDir(sector);

	// load the sector
	byte buf[SECTOR_SIZE];
	disk.readLogicalSector(partitionOffset + result.sector, buf);
	MSXDirEntry* direntries = (MSXDirEntry*)buf;

	MSXDirEntry& direntry = direntries[result.index];
	direntry.attrib = T_MSX_DIR;
	setsh(direntry.time, t);
	setsh(direntry.date, d);
	memcpy(&direntry, makeSimpleMSXFileName(msxName).data(), 11);

	//direntry.filesize = fsize;
	word curcl = findFirstFreeCluster();
	setsh(direntry.startcluster, curcl);
	writeFAT(curcl, EOF_FAT);

	//save the sector again
	disk.writeLogicalSector(partitionOffset + result.sector, buf);

	//clear this cluster
	int logicalSector = clusterToSector(curcl);
	memset(buf, 0, SECTOR_SIZE);
	for (int i = 0; i < sectorsPerCluster; ++i) {
		disk.writeLogicalSector(partitionOffset + i + logicalSector, buf);
	}

	// now add the '.' and '..' entries!!
	memset(&direntries[0], 0, sizeof(MSXDirEntry));
	memset(&direntries[0], ' ', 11);
	memset(&direntries[0], '.', 1);
	direntries[0].attrib = T_MSX_DIR;
	setsh(direntries[0].time, t);
	setsh(direntries[0].date, d);
	setsh(direntries[0].startcluster, curcl);

	memset(&direntries[1], 0, sizeof(MSXDirEntry));
	memset(&direntries[1], ' ', 11);
	memset(&direntries[1], '.', 2);
	direntries[1].attrib = T_MSX_DIR;
	setsh(direntries[1].time, t);
	setsh(direntries[1].date, d);
	setsh(direntries[1].startcluster, sectorToCluster(sector));

	//and save this in the first sector of the new subdir
	disk.writeLogicalSector(partitionOffset + logicalSector, buf);

	return logicalSector;
}

static void getTimeDate(time_t& totalSeconds, int& time, int& date)
{
	tm* mtim = localtime(&totalSeconds);
	if (!mtim) {
		time = 0;
		date = 0;
	} else {
		time = (mtim->tm_sec >> 1) + (mtim->tm_min << 5) +
		       (mtim->tm_hour << 11);
		date = mtim->tm_mday + ((mtim->tm_mon + 1) << 5) +
		       ((mtim->tm_year + 1900 - 1980) << 9);
	}
}

// Get the time/date from a host file in MSX format
static void getTimeDate(const string& filename, int& time, int& date)
{
	struct stat st;
	if (stat(filename.c_str(), &st)) {
		// stat failed
		time = 0;
		date = 0;
	} else {
		// some info indicates that st.st_mtime could be useless on win32 with vfat.
		getTimeDate(st.st_mtime, time, date);
	}
}

// Add an MSXsubdir with the time properties from the HOST-OS subdir
// @throws when subdir could not be created
int MSXtar::addSubdirtoDSK(const string& hostName, const string& msxName,
                           int sector)
{
	int time, date;
	getTimeDate(hostName, time, date);
	return addMSXSubdir(msxName, time, date, sector);
}

// This file alters the filecontent of a given file
// It only changes the file content (and the filesize in the msxdirentry)
// It doesn't changes timestamps nor filename, filetype etc.
// @throws when something goes wrong
void MSXtar::alterFileInDSK(MSXDirEntry& msxdirentry, const string& hostName)
{
	// get host file size
	struct stat st;
	if (stat(hostName.c_str(), &st)) {
		throw MSXException("Error reading host file: " + hostName);
	}
	int hostSize = st.st_size;
	int remaining = hostSize;

	// open host file for reading
	FILE* file = fopen(hostName.c_str(), "rb");
	if (!file) {
		throw MSXException("Error reading host file: " + hostName);
	}

	// copy host file to image
	word prevcl = 0;
	word curcl = rdsh(msxdirentry.startcluster);
	while (remaining) {
		// allocate new cluster if needed
		try {
			if ((curcl == 0) || (curcl == EOF_FAT)) {
				int newcl = findFirstFreeCluster();
				if (prevcl == 0) {
					setsh(msxdirentry.startcluster, newcl);
				} else {
					writeFAT(prevcl, newcl);
				}
				writeFAT(newcl, EOF_FAT);
				curcl = newcl;
			}
		} catch (MSXException& e) {
			// no more free clusters
			break;
		}

		// fill cluster
		byte buf[SECTOR_SIZE];
		int logicalSector = clusterToSector(curcl);
		disk.readLogicalSector(partitionOffset + logicalSector, buf);
		for (int j = 0; (j < sectorsPerCluster) && remaining; ++j) {
			int chunkSize = std::min(SECTOR_SIZE, remaining);
			fread(buf, 1, chunkSize, file);
			disk.writeLogicalSector(
				partitionOffset + logicalSector + j, buf);
			remaining -= chunkSize;
		}

		// advance to next cluster
		prevcl = curcl;
		curcl = readFAT(curcl);
	}
	fclose(file);

	// terminate FAT chain
	if (prevcl == 0) {
		setsh(msxdirentry.startcluster, 0);
	} else {
		writeFAT(prevcl, EOF_FAT);
	}

	// free rest of FAT chain
	while ((curcl != EOF_FAT) && (curcl != 0)) {
		int nextcl = readFAT(curcl);
		writeFAT(curcl, 0);
		curcl = nextcl;
	}

	// write (possibly truncated) file size
	setlg(msxdirentry.size, hostSize - remaining);

	if (remaining) {
		throw MSXException("Disk full, " + hostName + " truncated.");
	}
}

// Find the dir entry for 'name' in subdir starting at the given 'sector'
// with given 'index'
// returns: a DirEntry with sector and index filled in
//          sector is 0 if no match was found
MSXtar::DirEntry MSXtar::findEntryInDir(const string& name, int sector, byte* buf)
{
	DirEntry result;
	result.sector = sector;

	while (true) {
		// read sector and scan 16 entries
		disk.readLogicalSector(partitionOffset + result.sector, buf);
		MSXDirEntry* direntries = (MSXDirEntry*)buf;
		for (result.index = 0; result.index < 16; ++result.index) {
			if (string((char*)direntries[result.index].filename, 11)
			     == name) {
				return result;
			}
		}
		// try next sector
		result.sector = getNextSector(result.sector);
		if (result.sector == 0) {
			return result;
		}
	}
}

// Add file to the MSX disk in the subdir pointed to by 'sector'
// @throws when file could not be added
string MSXtar::addFiletoDSK(const string& hostName, const string& msxName,
                          int sector)
{
	// first find out if the filename already exists in current dir
	byte buf[SECTOR_SIZE];
	DirEntry fullmsxdirentry = findEntryInDir(
		makeSimpleMSXFileName(msxName), sector, buf);
	if (fullmsxdirentry.sector != 0) {
		// TODO implement overwrite option
		return "Warning: preserving entry " + msxName + '\n';
	}

	DirEntry result = addEntryToDir(sector);
	disk.readLogicalSector(partitionOffset + result.sector, buf);
	MSXDirEntry* direntries = (MSXDirEntry*)buf;
	MSXDirEntry& direntry = direntries[result.index];
	memset(&direntry, 0, sizeof(MSXDirEntry));
	memcpy(&direntry, makeSimpleMSXFileName(msxName).c_str(), 11);
	direntry.attrib = T_MSX_REG;

	// compute time/date stamps
	int time, date;
	getTimeDate(hostName, time, date);
	setsh(direntry.time, time);
	setsh(direntry.date, date);

	alterFileInDSK(direntry, hostName);
	disk.writeLogicalSector(partitionOffset + result.sector, buf);

	return "";
}

// Transfer directory and all its subdirectories to the MSX diskimage
// @throws when an error occurs
string MSXtar::recurseDirFill(const string& dirName, int sector)
{
	string messages;
	ReadDir dir(dirName);
	while (dirent* d = dir.getEntry()) {
		string name(d->d_name);
		string fullName = dirName + '/' + name;

		if (FileOperations::isRegularFile(fullName)) {
			// add new file
			messages += addFiletoDSK(fullName, name, sector);

		} else if (FileOperations::isDirectory(fullName) &&
			   (name != ".") && (name != "..")) {
			string msxFileName = makeSimpleMSXFileName(name);
			byte buf[SECTOR_SIZE];
			DirEntry direntry = findEntryInDir(msxFileName, sector, buf);
			if (direntry.sector != 0) {
				// entry already exists .. 
				MSXDirEntry* direntries = (MSXDirEntry*)buf;
				MSXDirEntry& msxdirentry = direntries[direntry.index];
				if (msxdirentry.attrib & T_MSX_DIR) {
					// .. and is a directory
					int nextSector = clusterToSector(
						rdsh(msxdirentry.startcluster));
					messages += recurseDirFill(fullName, nextSector);
				} else {
					// .. but is NOT a directory
					messages += "MSX file " + msxFileName +
					            " is not a directory.\n";
				}
			} else {
				// add new directory
				int nextSector = addSubdirtoDSK(fullName, name, sector);
				messages += recurseDirFill(fullName, nextSector);
			}
		}
	}
	return messages;
}


string MSXtar::condensName(MSXDirEntry& direntry)
{
	string result;
	for (int i = 0; (i < 8) && (direntry.filename[i] != ' '); ++i) {
		result += tolower(direntry.filename[i]);
	}
	if (direntry.ext[0] != ' ') {
		result += '.';
		for (int i = 0; (i < 3) && (direntry.ext[i] != ' '); ++i) {
			result += tolower(direntry.ext[i]);
		}
	}
	return result;
}


// Set the entries from direntry to the timestamp of resultFile
void MSXtar::changeTime(string resultFile, MSXDirEntry& direntry)
{
	int t = rdsh(direntry.time);
	int d = rdsh(direntry.date);
	struct tm mtim;
	struct utimbuf utim;
	mtim.tm_sec  =  (t & 0x001f) << 1;
	mtim.tm_min  =  (t & 0x03e0) >> 5;
	mtim.tm_hour =  (t & 0xf800) >> 11;
	mtim.tm_mday =  (d & 0x001f);
	mtim.tm_mon  =  (d & 0x01e0) >> 5;
	mtim.tm_year = ((d & 0xfe00) >> 9) + 80;
	utim.actime  = mktime(&mtim);
	utim.modtime = mktime(&mtim);
	utime(resultFile.c_str(), &utim);
}

string MSXtar::dir()
{
	string result;
	for (int sector = MSXchrootSector; sector != 0; sector = getNextSector(sector)) {
		byte buf[SECTOR_SIZE];
		disk.readLogicalSector(partitionOffset + sector, buf);
		MSXDirEntry* direntry = (MSXDirEntry*)buf;
		for (int i = 0; i < 16; ++i) {
			if ((direntry[i].filename[0] != 0xe5) &&
			    (direntry[i].filename[0] != 0x00)) {
				// filename first (in condensed form for human readablitly)
				string tmp = condensName(direntry[i]);
				tmp.resize(13, ' ');
				result += tmp;
				//attributes
				result += (direntry[i].attrib & T_MSX_DIR  ? "d" : "-");
				result += (direntry[i].attrib & T_MSX_READ ? "r" : "-");
				result += (direntry[i].attrib & T_MSX_HID  ? "h" : "-");
				result += (direntry[i].attrib & T_MSX_VOL  ? "v" : "-"); //todo check if this is the output of files,l
				result += (direntry[i].attrib & T_MSX_ARC  ? "a" : "-"); //todo check if this is the output of files,l
				result += "  ";
				//filesize
				result += StringOp::toString(rdlg(direntry[i].size)) + "\n";
			}
		}
	}
	return result;
}

// routines to update the global vars: MSXchrootSector
void MSXtar::chdir(const string& newRootDir)
{
	chroot(newRootDir, false);
}

void MSXtar::mkdir(const string& newRootDir)
{
	int tmpMSXchrootSector = MSXchrootSector;
	chroot(newRootDir, true);
	MSXchrootSector = tmpMSXchrootSector;
}

void MSXtar::chroot(const string& newRootDir, bool createDir)
{
	string work = newRootDir;
	if (work.find_first_of("/\\") == 0) {
		// absolute path, reset MSXchrootSector
		MSXchrootSector = rootDirStart;
		StringOp::trimLeft(work, "/\\");
	}

	while (!work.empty()) {
		string firstpart;
		StringOp::splitOnFirst(work, "/\\", firstpart, work);
		StringOp::trimLeft(work, "/\\");

		// find 'firstpart' directory or create it if requested
		byte buf[SECTOR_SIZE];
		string simple = makeSimpleMSXFileName(firstpart);
		DirEntry entry = findEntryInDir(simple, MSXchrootSector, buf);
		if (entry.sector == 0) {
			if (!createDir) {
				throw MSXException("Subdirectory " + firstpart +
				                   " not found.");
			}
			// creat new subdir
			time_t now;
			time(&now);
			int t, d;
			getTimeDate(now, t, d);
			MSXchrootSector = addMSXSubdir(simple, t, d, MSXchrootSector);
		} else {
			MSXDirEntry* direntries = (MSXDirEntry*)buf;
			MSXDirEntry& direntry = direntries[entry.index];
			if (!(direntry.attrib & T_MSX_DIR)) {
				throw MSXException(firstpart + " is not a directory.");
			}
			MSXchrootSector = clusterToSector(rdsh(direntry.startcluster));
		}
	}
}

void MSXtar::fileExtract(string resultFile, MSXDirEntry& direntry)
{
	int size = rdlg(direntry.size);
	int sector = clusterToSector(rdsh(direntry.startcluster));

	FILE* file = fopen(resultFile.c_str(), "wb");
	if (!file) {
		throw MSXException("Couldn't open file for writing!");
	}
	while (size && sector) {
		byte buf[SECTOR_SIZE];
		disk.readLogicalSector(partitionOffset + sector, buf);
		int savesize = std::min(size, SECTOR_SIZE);
		fwrite(buf, 1, savesize, file);
		size -= savesize;
		sector = getNextSector(sector);
	}
	fclose(file);
	// now change the access time
	changeTime(resultFile, direntry);
}

void MSXtar::recurseDirExtract(const string& dirName, int sector)
{
	for (/* */ ; sector != 0; sector = getNextSector(sector)) {
		byte buf[SECTOR_SIZE];
		disk.readLogicalSector(partitionOffset + sector, buf);
		MSXDirEntry* direntry = (MSXDirEntry*)buf;
		for (int i = 0; i < 16; ++i) {
			if ((direntry[i].filename[0] != 0xe5) &&
			    (direntry[i].filename[0] != 0x00) &&
			    (direntry[i].filename[0] != '.')) {
				string filename = condensName(direntry[i]);
				string fullname = filename;
				if (!dirName.empty()) {
					fullname = dirName + '/' + filename;
				}
				if (direntry[i].attrib != T_MSX_DIR) { // TODO 
					fileExtract(fullname, direntry[i]);
				}
				if (direntry[i].attrib == T_MSX_DIR) {
					FileOperations::mkdirp(fullname);
					// now change the access time
					changeTime(fullname, direntry[i]);
					recurseDirExtract(
					    fullname,
					    clusterToSector(rdsh(direntry[i].startcluster)));
				}
			}
		}
	}
}

static const char* const PARTAB_HEADER= "\353\376\220MSX_IDE ";

/** returns: true if succesfull, false if partition isn't valid
  */
bool MSXtar::isPartitionTableSector(byte* buf)
{
	return strncmp((char*)buf, PARTAB_HEADER, 11) == 0;
}

bool MSXtar::hasPartitionTable()
{
	byte buf[SECTOR_SIZE];
	disk.readLogicalSector(0, buf);
	return isPartitionTableSector(buf);
}

bool MSXtar::hasPartition(int partition)
{
	byte buf[SECTOR_SIZE];
	disk.readLogicalSector(0, buf);
	if (!isPartitionTableSector(buf)) {
		return false;
	}
	Partition* p = (Partition*)(buf + 14 + (30 - partition) * 16);
	if (rdlg(p->start4) == 0) {
		return false;
	}
	return true;
}

bool MSXtar::usePartition(int partition)
{
	byte buf[SECTOR_SIZE];
	disk.readLogicalSector(0, buf);
	if (!isPartitionTableSector(buf)) {
		partitionOffset = 0;
		partitionNbSectors = disk.getNbSectors();
		disk.readLogicalSector(partitionOffset, buf);
		parseBootSectorFAT(buf);
		return false;
	}

	Partition* p = (Partition*)(buf + 14 + (30 - partition) * 16);
	if (rdlg(p->start4) == 0) {
		return false;
	}
	partitionOffset = rdlg(p->start4);
	partitionNbSectors = rdlg(p->size4);
	disk.readLogicalSector(partitionOffset, buf);
	parseBootSectorFAT(buf);
	return true;
}

void MSXtar::createDiskFile(std::vector<unsigned> sizes)
{
	byte buf[SECTOR_SIZE];

	// create the partition table if needed
	if (sizes.size() > 1) {
		memset(buf, 0, SECTOR_SIZE);
		strncpy((char*)buf, PARTAB_HEADER, 11);

		unsigned startPartition = 1;
		for (unsigned i = 0; (i < sizes.size()) && (i < 30); ++i) {
			Partition* p = (Partition*)(buf + (14 + (30 - i) * 16));
			setlg(p->start4, startPartition);
			setlg(p->size4, sizes[i]);
			partitionOffset = startPartition;
			format(sizes[i]);
			startPartition += sizes[i];
		}
		disk.writeLogicalSector(0, buf);
	} else {
		setBootSector(buf, sizes[0]);
		disk.writeLogicalSector(0, buf); //allow usePartition to read the bootsector!
		usePartition(0);
		format(sizes[0]);
	}
}

// temporary way to test import MSXtar functionality
string MSXtar::addDir(const string& rootDirName)
{
	return recurseDirFill(rootDirName, MSXchrootSector);
}

// add file into fake dsk
string MSXtar::addFile(const string& filename)
{
	string dir, file;
	StringOp::splitOnLast(filename, "/\\", dir, file);
	return addFiletoDSK(filename, file, MSXchrootSector);
}

//temporary way to test export MSXtar functionality
void MSXtar::getDir(const string& rootDirName)
{
	recurseDirExtract(rootDirName, MSXchrootSector);
}

} // namespace openmsx
