// $Id$

#include "MSXtar.hh"
#include "CliComm.hh"
#include "ReadDir.hh"
#include "SectorAccessibleDisk.hh"
#include "FileDriveCombo.hh"
#include "FileOperations.hh"
#include "StringOp.hh"
#include <sys/stat.h>
#include <utime.h>
#include <unistd.h>
#include <cassert>

using std::string;
using std::vector;

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
void MSXtar::readBootSector(const byte* buf)
{
	MSXBootSector* boot = (MSXBootSector*)buf;
	nbSectors = rdsh(boot->nrsectors); // assume a DS disk is used
	sectorsPerTrack = rdsh(boot->nrsectors);
	nbSides = rdsh(boot->nrsides);
	nbFats = boot->nrfats[0];
	sectorsPerFat = rdsh(boot->sectorsfat);
	nbRootDirSectors = rdsh(boot->direntries) / 16;
	sectorsPerCluster = (int)(byte)boot->spcluster[0];
	rootDirStart = 1 + nbFats * sectorsPerFat;
	MSXchrootSector = rootDirStart;
	MSXchrootStartIndex = 0;
	rootDirEnd = rootDirStart + nbRootDirSectors - 1;
	maxCluster = sectorToCluster(nbSectors);
	
	PRT_DEBUG("ReadBootsector info:");
	PRT_DEBUG("nbSectors         :" << nbSectors);
	PRT_DEBUG("sectorsPerTrack   :" << sectorsPerTrack);
	PRT_DEBUG("nbSides           :" << nbSides);
	PRT_DEBUG("nbFats            :" << nbFats);
	PRT_DEBUG("sectorsPerFat     :" << sectorsPerFat);
	PRT_DEBUG("nbRootDirSectors  :" << nbRootDirSectors);
	PRT_DEBUG("sectorsPerCluster :" << sectorsPerCluster);
	PRT_DEBUG("rootDirStart      :" << rootDirStart);
	PRT_DEBUG("rootDirEnd        :" << rootDirEnd);
	PRT_DEBUG("maxCluster        :" << maxCluster);
}

MSXtar::MSXtar(SectorAccessibleDisk& sectordisk)
	: disk(sectordisk)
{
	nbSectorsPerCluster = 2;
	MSXchrootStartIndex = 0;
	partitionNbSectors = 0;
	partitionOffset = 0;
	defaultBootBlock = dos2BootBlock;

	//TODO : see if we can make usePartition always read the bootsector even 
	// if no partition table is given.
	// Since Filemanipulator will always call usePartition before doing any
	// real manipulationm this would eliminate the followin code.
	/*
	byte sectorbuf[SECTOR_SIZE];
	disk.readLogicalSector(0, sectorbuf);
	if (!isPartitionTableSector(sectorbuf)) {
		MSXBootSector* boot = (MSXBootSector*)sectorbuf;
		if (rdsh(boot->nrsectors)) readBootSector(sectorbuf); 
		// assuming a regular disk is used, set object variables
		// to correct values assuming this is a "normal dsk"
		// check for 0 value because otherwise we would perform
		// a division by zero....
	}
	//readBootSector(defaultBootBlock); // set object variables to correct values
	*/
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
		PRT_DEBUG("format found hasPartitionTable so running "
		          "format(partitionNbSectors)" << partitionNbSectors);
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
	readBootSector(sectorbuf); //set object variables to correct values
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
word MSXtar::readFAT(word clnr)
{
	// read entire fat in a buffer, can be optimized if needed
	vector<byte> buf(SECTOR_SIZE * sectorsPerFat);
	byte* sectorbuf = &buf[0];
	for (int i = 0; i < sectorsPerFat; ++i) {
		disk.readLogicalSector(partitionOffset + 1 + i,
		                        &sectorbuf[i * SECTOR_SIZE]);
	}

	byte* p = sectorbuf + (clnr * 3) / 2;
	return (clnr & 1)
	       ? (p[0] >> 4) + (p[1] << 4)
	       : p[0] + ((p[1] & 0x0F) << 8);
}

// Write an entry to the FAT
void MSXtar::writeFAT(word clnr, word val)
{
	byte sectorbuf[SECTOR_SIZE];
	int sectornr = ((clnr * 3) / 2) / SECTOR_SIZE;
	disk.readLogicalSector(partitionOffset + 1 + sectornr, sectorbuf);
	int offset = (clnr * 3) / 2 - SECTOR_SIZE * sectornr;

	byte* p = &sectorbuf[offset];
	if (offset < (SECTOR_SIZE - 1)) {
		// all actions in same sector
		if (clnr & 1) { 
			p[0] = (p[0] & 0x0F) + (val << 4);
			p[1] = val >> 4;
		} else {
			p[0] = val;
			p[1] = (p[1] & 0xF0) + ((val >> 8) & 0x0F);
		}
		disk.writeLogicalSector(partitionOffset + 1 + sectornr,
		                         sectorbuf);
	} else {
		// first byte in one sector next byte in following sector
		if (clnr & 1) { 
			p[0] = (p[0] & 0x0F) + (val << 4);
		} else {
			p[0] = val;
		}
		disk.writeLogicalSector(partitionOffset + 1 + sectornr,
		                         sectorbuf);
		
		disk.readLogicalSector(partitionOffset + 2 + sectornr,
		                        sectorbuf);
		if (clnr & 1) { 
			sectorbuf[0] = val >> 4;
		} else {
			sectorbuf[0] = (sectorbuf[0] & 0xF0) +
			               ((val >> 8) & 0x0F);
		}
		disk.writeLogicalSector(partitionOffset + 2 + sectornr,
		                         sectorbuf);
	}
}

// Find the next clusternumber marked as free in the FAT
word MSXtar::findFirstFreeCluster()
{
	int cluster = 2;
	while ((cluster <= maxCluster) && readFAT(cluster)) {
		++cluster;
	}
	return cluster;
}

// Returns the index of a free (or with deleted file) entry
//  In:  The current dir sector
//  Out: index (is max 15), if no index is found then 16 is returned
byte MSXtar::findUsableIndexInSector(int sector)
{
	byte buf[SECTOR_SIZE];
	disk.readLogicalSector(partitionOffset + sector, buf);
	return findUsableIndexInSector(buf);
}

byte MSXtar::findUsableIndexInSector(byte* buf)
{
	// find a not used (0x00) or delete entry (0xe5)
	byte* p = buf;
	byte i = 0;
	for (/* */; (i < 16) && (p[0] != 0x00) && (p[0] != 0xe5); ++i, p += 32);
	return i;
}

// Get the next sector from a file or (root/sub)directory
// If no next sector then 0 is returned
int MSXtar::getNextSector(int sector)
{
	//if this sector is part of the root directory...
	if (sector == rootDirEnd) return 0;
	if (sector <  rootDirEnd) return sector + 1;

	if ((nbSectorsPerCluster > 1) &&
	    (sectorToCluster(sector) == sectorToCluster(sector + 1))) {
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
// returns: the first sector in the newly appended cluster, or 0 in case of error
int MSXtar::appendClusterToSubdir(int sector)
{
	word curcl = sectorToCluster(sector);
	assert(readFAT(curcl) == EOF_FAT);
	word nextcl = findFirstFreeCluster();
	if (nextcl > maxCluster) {
		return 0;
	}
	int logicalSector = clusterToSector(nextcl);
	//clear this cluster
	byte buf[SECTOR_SIZE];
	memset(buf, 0, SECTOR_SIZE);
	for (int i = 0; i < sectorsPerCluster; ++i) {
		disk.writeLogicalSector(partitionOffset + i + logicalSector, buf);
	}
	writeFAT(curcl, nextcl);
	writeFAT(nextcl, EOF_FAT);
	return logicalSector;
}


// This function returns the sector and dirindex for a new directory entry
// if needed the involved subdirectroy is expanded by an extra cluster
// returns: a PhysDirEntry containing sector and index
//          if failed then the sectornumber is 0 and index is 16
MSXtar::PhysDirEntry MSXtar::addEntryToDir(int sector, byte /*direntryindex*/)
{
	// this routin adds the msxname to a directory sector, if needed (and
	// possible) the directory is extened with an extra cluster
	PhysDirEntry newEntry;
	byte newindex = findUsableIndexInSector(sector);
	if (sector <= rootDirEnd) {
		// we are adding this to the root sector
		while ((newindex > 15) && (sector <= rootDirEnd)) {
			newindex = findUsableIndexInSector(++sector);
		}
		newEntry.sector = sector;
		newEntry.index  = newindex;
	} else {
		// we are adding this to a subdir
		if (newindex < 16){
			newEntry.sector = sector;
			newEntry.index  = newindex;
		} else {
			while ((newindex > 15) && (sector != 0)) {
				int nextsector = getNextSector(sector);
				if (nextsector == 0) {
					nextsector = appendClusterToSubdir(sector);
					PRT_DEBUG("appendClusterToSubdir(" << sector << ") returns" << nextsector);
					if (nextsector == 0) {
						CliComm::instance().printWarning(
							"Disk is full!");
					}
				}
				sector = nextsector;
				newindex = findUsableIndexInSector(sector);
			}
			newEntry.sector = sector;
			newEntry.index  = newindex;
		}
	}
	return newEntry;
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

static string removeTrailingSpaces(string str)
{
	while (!str.empty() && (str[str.size() - 1] == ' ')) {
		str = str.substr(0, str.size() - 1);
	}
	return str;
}

// Transform a long hostname in a 8.3 uppercase filename as used in the
// direntries on an MSX
string MSXtar::makeSimpleMSXFileName(const string& fullfilename)
{
	string::size_type pos = fullfilename.find_last_of('/');
	string tmp = (pos != string::npos)
	           ? fullfilename.substr(pos + 1)
	           : fullfilename;

	// handle speciale case '.' and '..' first
	if ((tmp == ".") || (tmp == "..")) {
		tmp.resize(11, ' ');
		return tmp;
	}

	string file, ext;
	pos = tmp.find_last_of('.');
	if (pos != string::npos) {
		file = tmp.substr(0, pos);
		ext  = tmp.substr(pos + 1);
	} else {
		file = tmp;
		ext = "";
	}
	file = removeTrailingSpaces(file);
	ext  = removeTrailingSpaces(ext);
	
	// put in major case and create '_' if needed
	transform(file.begin(), file.end(), file.begin(), toMSXChr);
	transform(ext.begin(),  ext.end(),  ext.begin(),  toMSXChr);

	// add correct number of spaces
	file.resize(8, ' ');
	ext .resize(3, ' ');

	return file + ext;
}

// This function creates a new MSX subdir with given date 'd' and time 't'
// in the subdir pointed at by 'sector' and 'direntryindex' in the newly
// created subdir the entries for '.' and '..' are created
// returns: the first sector of the new subdir
//          0 in case no directory could be created
int MSXtar::addMSXSubdir(const string& msxName, int t, int d, int sector,
                         byte direntryindex)
{
	// returns the sector for the first cluster of this subdir
	PhysDirEntry result = addEntryToDir(sector, direntryindex);
	if (result.index > 15) {
		CliComm::instance().printWarning("couldn't add entry" + msxName);
		return 0;
	}
	// load the sector
	byte buf[SECTOR_SIZE];
	disk.readLogicalSector(partitionOffset + result.sector, buf);

	MSXDirEntry* direntry = (MSXDirEntry*)(buf + 32 * result.index);
	direntry->attrib = T_MSX_DIR;
	setsh(direntry->time, t);
	setsh(direntry->date, d);
	memcpy(direntry, makeSimpleMSXFileName(msxName).c_str(), 11);

	//direntry->filesize = fsize;
	word curcl = findFirstFreeCluster();
	PRT_DEBUG("New subdir starting at cluster " << curcl);
	setsh(direntry->startcluster, curcl);
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
	direntry = (MSXDirEntry*)(buf);
	memset(direntry, 0, sizeof(MSXDirEntry));
	memset(direntry, ' ', 11);
	memset(direntry, '.', 1); 
	direntry->attrib = T_MSX_DIR;
	setsh(direntry->time, t);
	setsh(direntry->date, d);
	setsh(direntry->startcluster, curcl);

	++direntry;
	memset(direntry, 0, sizeof(MSXDirEntry));
	memset(direntry, ' ', 11);
	memset(direntry, '.', 2); 
	direntry->attrib = T_MSX_DIR;
	setsh(direntry->time, t);
	setsh(direntry->date, d);
	//int parentcluster = (sector - 10) / 2;
	//setsh(direntry->startcluster, parentcluster);
	setsh(direntry->startcluster, sectorToCluster(sector));
	//and save this in the first sector of the new subdir
	disk.writeLogicalSector(partitionOffset + logicalSector, buf);

	return logicalSector;
}

static void getTimeDate(time_t* totalSeconds, int& time, int& date)
{
	tm* mtim = localtime(totalSeconds);
	time = (mtim->tm_sec >> 1) + (mtim->tm_min << 5) + 
	       (mtim->tm_hour << 11);
	date = mtim->tm_mday + ((mtim->tm_mon + 1) << 5) +
	       ((mtim->tm_year + 1900 - 1980) << 9);
}

// Get the time/date from a host file in MSX format
static void getTimeDate(const string& filename, int& time, int& date)
{
	struct stat fst;
	memset(&fst, 0, sizeof(struct stat));
	PRT_DEBUG("getTimeDate on " << filename);
	if (stat(filename.c_str(), &fst) != 0) {
		PRT_DEBUG("couldn't stat " << filename);
		time = 0;
		date = 0;
	} else {
		// some info indicates that fst.st_mtime could be useless on win32 with vfat.
		getTimeDate(&(fst.st_mtime), time, date);
	}
}

// Add an MSXsubdir with the time properties from the HOST-OS subdir
int MSXtar::addSubdirtoDSK(const string& hostName, const string& msxName,
                           int sector, byte direntryindex)
{
	//MSXDirEntry* direntry = (MSXDirEntry*)
	//   (FSImage + SECTOR_SIZE * result.sector + 32 * result.index);

	int time, date;
	getTimeDate(hostName, time, date);

	return addMSXSubdir(msxName, time, date, sector, direntryindex);
}

/** This file alters the filecontent of agiven file
  * It only changes the file content (and the filesize in the msxdirentry)
  * It doesn't changes timestamps nor filename, filetype etc.
  * Output: nothing usefull yet
  */
int MSXtar::alterFileInDSK(MSXDirEntry* msxdirentry, const string& hostName)
{
	bool needsNew = false;
	int fsize;
	struct stat fst;
	memset(&fst, 0, sizeof(struct stat));
	stat(hostName.c_str(), &fst);
	fsize = fst.st_size;
	PRT_DEBUG("alterFileInDSK: Filesize " << fsize);

	word curcl = rdsh(msxdirentry->startcluster) ;
	if (curcl == 0){
		// if entry first used then no cluster assigned yet
		curcl = findFirstFreeCluster();
		setsh(msxdirentry->startcluster, curcl);
		writeFAT(curcl, EOF_FAT); 
		needsNew = true;
	}
	PRT_DEBUG("alterFileInDSK: Starting at cluster " << curcl );

	int size = fsize;
	int prevcl = 0;
	//open file for reading
	FILE* file = fopen(hostName.c_str(), "rb");
	byte buf[SECTOR_SIZE];

	while (size && (curcl <= maxCluster)) {
		int logicalSector = clusterToSector(curcl);
		disk.readLogicalSector(partitionOffset + logicalSector, buf);
		for (int j = 0; j < sectorsPerCluster; ++j) {
			if (size) {
				PRT_DEBUG("alterFileInDSK: relative sector " << j << " in cluster " << curcl);
				// more poitical correct:
				// We should read 'size'-bytes instead of 'SECTOR_SIZE' if it is the end of the file
				fread(buf, 1, SECTOR_SIZE, file);
				disk.writeLogicalSector(
					partitionOffset + logicalSector + j, buf);
			}
			size -= (size > SECTOR_SIZE ? SECTOR_SIZE : size);
		}

		if (prevcl) {
			writeFAT(prevcl, curcl);
		}
		prevcl = curcl;
		// now we check if we continue in the current clusterstring or
		// need to allocate extra unused blocks
		//do {
		if (needsNew) {
			//need extra space for this file
			writeFAT(curcl, EOF_FAT); 
			curcl = findFirstFreeCluster();
		} else {
			// follow cluster-Fat-stream
			curcl = readFAT(curcl);
			if (curcl == EOF_FAT) {
				curcl = findFirstFreeCluster();
				needsNew = true;
			}
		}
		//} while((curcl <= maxCluster) && readFAT(curcl));
		PRT_DEBUG("alterFileInDSK: Continuing at cluster " << curcl);
	}
	fclose(file);

	if ((size == 0) && (curcl <= maxCluster)) {
		// TODO: check what an MSX does with filesize zero and fat allocation;
		if(prevcl == 0) {
			prevcl = curcl;
			curcl = readFAT(curcl);
		}
		writeFAT(prevcl, EOF_FAT); 
		PRT_DEBUG("alterFileInDSK: Ending at cluster " << prevcl);
		//cleaning rest of FAT chain if needed
		while (!needsNew) { 
			prevcl = curcl;
			if (curcl != EOF_FAT) {
				curcl = readFAT(curcl);
			} else {
				needsNew = true;
			}
			PRT_DEBUG("alterFileInDSK: Cleaning cluster " << prevcl << " from FAT");
			writeFAT(prevcl, 0);
		}
	} else {
		//TODO: don't we need a EOF_FAT in this case as well ?
		// find out and adjust code here
		CliComm::instance().printWarning(
			"Fake Diskimage full: " + hostName + " truncated.");
	}
	// write (possibly truncated) file size
	setlg(msxdirentry->size, fsize - size);
	return 0;
}

// Find the dir entry for 'name' in subdir starting at the given 'sector'
// with given 'index'
// returns: a FullMSXDirEntry if name was found
//          a direntryindex is 16 if no match was found
MSXtar::FullMSXDirEntry MSXtar::findEntryInDir(const string& name,
	int sector, byte direntryindex, byte* sectorbuf)
{
	FullMSXDirEntry result;
	result.sectorbuf = sectorbuf;

	disk.readLogicalSector(partitionOffset + sector, sectorbuf);
	byte* p = sectorbuf + 32 * direntryindex;
	byte i = direntryindex;
	bool entryfound = false;
	do {
		for (/* */;
		     (i < 16) && (strncmp(name.c_str(), (char*)p, 11) != 0);
		     ++i, p += 32);
		if (i == 16) {
			sector = getNextSector(sector);
			disk.readLogicalSector(partitionOffset + sector, sectorbuf);
			p = sectorbuf;
			if (sector) i = 0;
		} else entryfound=true;
	} while ((i < 16) && sector && !entryfound);

	result.sector = sector;
	result.direntryindex = i;
	return result;
}

// Add file to the MSX disk in the subdir pointed to by 'sector'
// returns: nothing usefull yet :-)
int MSXtar::addFiletoDSK(const string& hostName, const string& msxName,
                         int sector, byte direntryindex)
{
	// first find out if the filename already exists in current dir
	byte buf[SECTOR_SIZE];
	FullMSXDirEntry fullmsxdirentry = findEntryInDir(
		makeSimpleMSXFileName(msxName), sector, direntryindex, buf);
	if (fullmsxdirentry.direntryindex != 16) {
		CliComm::instance().printWarning("Preserving entry " + msxName);
		return 0;
	}

	PhysDirEntry result = addEntryToDir(sector, direntryindex);
	if (result.index > 15) {
		CliComm::instance().printWarning("Couldn't add entry " + hostName);
		return 255;
	}

	disk.readLogicalSector(partitionOffset + result.sector, buf);
	MSXDirEntry* direntry = (MSXDirEntry*)(buf + 32 * result.index);
	direntry->attrib = T_MSX_REG;
	//PRT_VERBOSE(hostName << " \t-> \"" << makeSimpleMSXFileName(msxName) << '"');
	memcpy(direntry, makeSimpleMSXFileName(msxName).c_str(), 11);

	// compute time/date stamps
	int time, date;
	getTimeDate(hostName, time, date);
	setsh(direntry->time, time);
	setsh(direntry->date, date);

	alterFileInDSK(direntry, hostName);
	disk.writeLogicalSector(partitionOffset + result.sector, buf);
	return 0;
}

// Transfer directory and all its subdirectories to the MSX diskimage
void MSXtar::recurseDirFill(const string& dirName, int sector, int direntryindex)
{
	PRT_DEBUG("Trying to read directory " << dirName);
	
	//read directory and fill the fake disk
	ReadDir dir(dirName);
	while (dirent* d = dir.getEntry()) {
		string name(d->d_name);
		string fullName = dirName + '/' + name;

		if (FileOperations::isRegularFile(fullName)) {
			// add file into fake dsk
			addFiletoDSK(fullName, name, sector, direntryindex);

		} else if (FileOperations::isDirectory(fullName) &&
			   (name != ".") && (name != "..")) {
			string msxFileName = makeSimpleMSXFileName(name);
			PRT_DEBUG(fullName << " \t-> \"" << msxFileName << '"');
			byte buf[SECTOR_SIZE];
			FullMSXDirEntry fullmsxdirentry = findEntryInDir(msxFileName, sector, direntryindex, buf);
			int result;
			if (fullmsxdirentry.direntryindex != 16) {
				CliComm::instance().printWarning("Dir entry " + name + " exists already");
				MSXDirEntry* msxdirentry = (MSXDirEntry*)(buf + 32 * fullmsxdirentry.direntryindex);
				result = clusterToSector(rdsh(msxdirentry->startcluster));
			} else {
				PRT_DEBUG("Adding dir entry " << name);
				// add file into fake dsk
				result = addSubdirtoDSK(fullName, name, sector, direntryindex);
			}

			recurseDirFill(fullName, result, 0);
		}
	}
}


string MSXtar::condensName(MSXDirEntry* direntry)
{
	char condensedname[13];
	char* p = condensedname;
	for (int i = 0; i < 8; ++i) {
		if (direntry->filename[i] == ' ') break;
		*p++ = tolower(direntry->filename[i]);
	}
	if (direntry->ext[0] != ' ') {
		*p++ = '.';
		for (int i = 0; i < 3; ++i) {
			if (direntry->ext[i] == ' ') break;
			*p++ = tolower(direntry->ext[i]);
		}
	}
	*p = (char)0;
	return condensedname;
}


/** Set the entries from direntry to the timestamp of resultFile
  */
void MSXtar::changeTime(string resultFile, MSXDirEntry* direntry)
{
	int t = rdsh(direntry->time);
	int d = rdsh(direntry->date);
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
	//int i=MSXchrootStartIndex;
	int i = 0; // show '.' and '..'
	int sector = MSXchrootSector;

	byte buf[SECTOR_SIZE];
	do {
		disk.readLogicalSector(partitionOffset + sector, buf);
		do {
			MSXDirEntry* direntry =(MSXDirEntry*)(buf + 32 * i);
			if (direntry->filename[0] != 0xe5 && direntry->filename[0] != 0x00 ) {
				// filename first (in condensed form for human readablitly)
				string tmp = condensName(direntry);
				tmp.resize(13, ' ');
				result += tmp;
				//CliComm::instance().printWarning("MSXtar::dir " + tmpstr);
				//attributes
				result += (direntry->attrib & T_MSX_DIR  ? "d" : "-");
				result += (direntry->attrib & T_MSX_READ ? "r" : "-");
				result += (direntry->attrib & T_MSX_HID  ? "h" : "-");
				result += (direntry->attrib & T_MSX_VOL  ? "v" : "-"); //todo check if this is the output of files,l
				result += (direntry->attrib & T_MSX_ARC  ? "a" : "-"); //todo check if this is the output of files,l
				result += "  ";
				//filesize
				result += StringOp::toString(rdlg(direntry->size)) + "\n";
			}
			i++;
		} while (i <16);
		sector = getNextSector(sector);
		i = 0;
	} while (sector != 0);

	return result;
}

//routines to update the global vars: MSXchrootSector,MSXchrootStartIndex
bool MSXtar::chdir(const string& newRootDir)
{
	return chroot(newRootDir, false);
}

bool MSXtar::mkdir(const string& newRootDir)
{
	int tmpMSXchrootSector = MSXchrootSector;
	int tmpMSXchrootStartIndex = MSXchrootStartIndex;

	bool succes = chroot(newRootDir, true);

	MSXchrootSector = tmpMSXchrootSector;
	MSXchrootStartIndex = tmpMSXchrootStartIndex;

	return succes;
}

bool MSXtar::chroot(const string& newRootDir, bool createDir)
{
	string work = newRootDir;
	// if this is not a relative directory then reset MSXchrootSector,MSXchrootStartIndex
	while (work.find_first_of("/\\") == 0) {
		MSXchrootSector = rootDirStart;
		MSXchrootStartIndex = 0;
		work.erase(0, 1);
	}

	while (!work.empty()) {
		PRT_DEBUG("chroot 0: work=" << work);
		//remove (multiple)leading '/'
		while (work.find_first_of("/\\") == 0) {
			work.erase(0, 1);
			PRT_DEBUG("chroot 1: work=" << work);
		}
		string firstpart;
		string::size_type pos = work.find_first_of("/\\");
		if (pos != string::npos) {
			firstpart = work.substr(0, pos);
			work = work.substr(pos + 1);
		} else {
			firstpart = work;
			work.clear();
		}
		// find firstpart directory or create it if requested
		string simple = makeSimpleMSXFileName(firstpart);
		byte buf[SECTOR_SIZE];
		FullMSXDirEntry fullmsxdirentry = findEntryInDir(simple, MSXchrootSector, 0, buf); // we use 0 instead of MSXchrootStartIndex since we might need to access '.' and '..' here
		if (fullmsxdirentry.direntryindex == 16) {
			if (!createDir) return false;
			//creat new subdir
			time_t now;
			int t, d;
			time(&now);
			getTimeDate(&now, t, d);

			PRT_DEBUG("Creating subdir " << simple) ;
			MSXchrootSector = addMSXSubdir(simple, t, d, MSXchrootSector, MSXchrootStartIndex);
			MSXchrootStartIndex = 2;
			if (MSXchrootSector == 0) {
				//failed to create subdir
				return false;
			}
		} else {
			MSXDirEntry* direntry = (MSXDirEntry*)(buf + 32 * fullmsxdirentry.direntryindex);
			MSXchrootSector = clusterToSector(rdsh(direntry->startcluster));
			MSXchrootStartIndex = 2;
		}
	}
	return true;
}

void MSXtar::fileExtract(string resultFile, MSXDirEntry* direntry)
{
	long size = rdlg(direntry->size);
	long savesize;
	int sector = clusterToSector(rdsh(direntry->startcluster));
	byte buf[SECTOR_SIZE];

	FILE* file = fopen(resultFile.c_str(), "wb");
	if (file == NULL) {
		CliComm::instance().printWarning("Couldn't open file for writing!");
		return;
	}
	while (size && sector) {
		disk.readLogicalSector(partitionOffset + sector, buf);
		savesize = (size > SECTOR_SIZE ? SECTOR_SIZE : size);
		fwrite(buf, 1, savesize, file);
		size -= savesize;
		sector = getNextSector(sector);
	}
	if ((sector == 0) && (size != 0)) {
		CliComm::instance().printWarning("no more sectors for file but file not ended ???");
	}
	fclose(file);
	// now change the access time
	changeTime(resultFile, direntry);
}

void MSXtar::recurseDirExtract(const string& dirName, int sector, int direntryindex)
{
	int i = direntryindex;
	byte buf[SECTOR_SIZE];
	do {
		disk.readLogicalSector(partitionOffset + sector, buf);
		do {
			MSXDirEntry* direntry = (MSXDirEntry*)(buf + 32 * i);
			if (direntry->filename[0] != 0xe5 && direntry->filename[0] != 0x00) {
				string filename = condensName(direntry);
				string fullname = filename;
				if (!dirName.empty()){
					fullname = dirName + '/' + filename;
				}
				CliComm::instance().printWarning("extracting " + fullname);
				//PRT_VERBOSE(fullname);
				if (direntry->attrib != T_MSX_DIR) {
					fileExtract(fullname, direntry);
				}
				if (direntry->attrib == T_MSX_DIR) {
					FileOperations::mkdirp(fullname);
					// now change the access time
					changeTime(fullname, direntry);
					recurseDirExtract(
						fullname ,
						clusterToSector(rdsh(direntry->startcluster)) ,
						2); //read subdir and skip entries for '.' and '..'
				}
			}
			i++;
		} while (i <16);
		sector = getNextSector(sector);
		i = 0;
	} while (sector != 0);
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

bool MSXtar::usePartition(int partition)
{
	byte buf[SECTOR_SIZE];
	disk.readLogicalSector(0, buf);
	if (!isPartitionTableSector(buf)) {
		partitionOffset = 0;
		partitionNbSectors = disk.getNbSectors();
		disk.readLogicalSector(partitionOffset, buf);
		readBootSector(buf);
		return false;
	}

	Partition* p = (Partition*)(buf + 14 + (30 - partition) * 16);
	if (rdlg(p->start4) == 0) {
		return false;
	}
	partitionOffset = rdlg(p->start4);
	partitionNbSectors = rdlg(p->size4);
	disk.readLogicalSector(partitionOffset, buf);
	readBootSector(buf);
	return true;
}

void MSXtar::createDiskFile(vector<unsigned> sizes)
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
			format(sizes[i]); //does it's own setBootSector/readBootSector
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

//temporary way to test import MSXtar functionality
void MSXtar::addDir(const string& rootDirName)
{
	//recurseDirFill(rootDirName, rootDirStart, 0);
	recurseDirFill(rootDirName, MSXchrootSector, MSXchrootStartIndex);
}

//temporary way to test export MSXtar functionality
void MSXtar::getDir(const string& rootDirName)
{
	//recurseDirExtract(rootDirName, rootDirStart, 0);
	recurseDirExtract(rootDirName, MSXchrootSector, MSXchrootStartIndex);
}

} // namespace openmsx
