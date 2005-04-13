// $Id$

#include "MSXtar.hh"
#include "SectorBasedDisk.hh"
#include "CliComm.hh"

#include <sys/types.h>
#include <dirent.h>
#include <sys/stat.h>
#include <utime.h>
#include <unistd.h>

using std::string;

namespace openmsx {

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

MSXtar::MSXtar(SectorBasedDisk& sectordisk)
	: disk(sectordisk)
{
	nbSectorsPerCluster = 2;
	MSXchrootStartIndex = 0;
	MSXpartition = 0;
	do_extract = false;
	do_subdirs = true;
	do_singlesided = false;
	touch_option = false;
	keep_option = false;
	msxdir_option = false;
	msxpart_option = false;
	msx_allpart = false;
	do_fat16 = false;
	partitionOffset=0;
	defaultBootBlock = dos2BootBlock;

	//byte sectorbuf[SECTOR_SIZE];
	//disk.readLogicalSector(0,sectorbuf);
	//readBootSector(sectorbuf); //set object variables to correct values
}

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
void MSXtar::readBootSector(byte* buf)
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

	rootDirEnd = rootDirStart + nbRootDirSectors - 1;
	maxCluster = sectorToCluster(nbSectors);
}

// Create a correct bootsector depending on the required size of the filesystem
void MSXtar::setBootSector(byte* buf, word nbSectors)
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
		nbSides = 2;		// unknow yet
		nbFats = 2;		// unknow yet
		nbSectorsPerFat = 12;	// copied from a partition from an IDE HD
		nbSectorsPerCluster = 16;
		nbDirEntry = 256;
		nbSides = 32;		// copied from a partition from an IDE HD
		nbHiddenSectors = 16;
		descriptor = 0xF0;
	} else if (nbSectors >= 16389) {
		nbSides = 2;		// unknow yet
		nbFats = 2;		// unknow yet
		nbSectorsPerFat = 3;	// unknow yet
		nbSectorsPerCluster = 8;
		nbDirEntry = 256;
		descriptor = 0XF0;
	} else if (nbSectors >= 8213) {
		nbSides = 2;		// unknow yet
		nbFats = 2;		// unknow yet
		nbSectorsPerFat = 3;	// unknow yet
		nbSectorsPerCluster = 4;
		nbDirEntry = 256;
		descriptor = 0xF0;
	} else if (nbSectors >= 4127) {
		nbSides = 2;		// unknow yet
		nbFats = 2;		// unknow yet
		nbSectorsPerFat = 3;	// unknow yet
		nbSectorsPerCluster = 2;
		nbDirEntry = 256;
		descriptor = 0xF0;
	} else if (nbSectors >= 2880) {
		nbSides = 2;		// unknow yet
		nbFats = 2;		// unknow yet
		nbSectorsPerFat = 3;	// unknow yet
		nbSectorsPerCluster = 1;
		nbDirEntry = 224;
		descriptor = 0xF0;
	} else if (nbSectors >= 1441) {
		nbSides = 2;		// unknow yet
		nbFats = 2;		// unknow yet
		nbSectorsPerFat = 3;	// unknow yet
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
  format( disk.getNbSectors() );
}
void MSXtar::format(int partitionsectorsize)
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
	// for some reason the first 3bytes are used to indicate the end of a
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
	for (unsigned i = 1 + rootDirEnd; i < disk.getNbSectors(); ++i) {
		disk.writeLogicalSector(partitionOffset + i, sectorbuf);
	}
}

/** Get the next clusternumber from the FAT chain
  */
word MSXtar::ReadFAT(word clnr)
{
	PRT_DEBUG("ReadFAT(cnlr= "<<clnr<<");");
	word result;
	//quick-nd-dirty read entire fat in a buffer to facilitate offset calculations
	byte* sectorbuf=new byte[SECTOR_SIZE*sectorsPerFat];
	for (int i=0 ; i < sectorsPerFat ; ++i){
		disk.readLogicalSector(partitionOffset + 1 + i , &sectorbuf[i * SECTOR_SIZE] );
	};

	if (!do_fat16){
		byte *P = sectorbuf + (clnr * 3) / 2;
		PRT_DEBUG("returning "<< ( (clnr & 1) ?  (P[0] >> 4) + (P[1] << 4) : P[0] + ((P[1] & 0x0F) << 8)) );
		result= (clnr & 1) ?  (P[0] >> 4) + (P[1] << 4) : P[0] + ((P[1] & 0x0F) << 8);
	} else {
		PRT_DEBUG("FAT size 16 not yet tested!!");
		//byte *P = FSImage+SECTOR_SIZE + (clnr * 2) ;
		//return P[0] + (P[1] << 8);
		result=0;
	};
	delete[] sectorbuf;
	return result;
}

/** Write an entry to the FAT
  */
void MSXtar::WriteFAT(word clnr, word val)
{
	byte sectorbuf[SECTOR_SIZE];
	if (!do_fat16){
		PRT_DEBUG("WriteFAT(cnlr= "<<clnr<<",val= "<<val<<");");
		int sectornr= ((clnr * 3) / 2)/SECTOR_SIZE;
		disk.readLogicalSector(partitionOffset + 1 + sectornr , sectorbuf );
		int offset= (clnr * 3) / 2 - SECTOR_SIZE * sectornr;

		byte* P=&sectorbuf[offset];
		if (offset< ( SECTOR_SIZE-1)){
		  	// all actions in same sector
			if (clnr & 1) { 
				P[0] = (P[0] & 0x0F) + (val << 4);
				P[1] = val >> 4;
			} else {
				P[0] = val;
				P[1] = (P[1] & 0xF0) + ((val >> 8) & 0x0F);
			}
			disk.writeLogicalSector(partitionOffset + 1 + sectornr , sectorbuf );
		} else {
		  	// first byte in one sector next byte in following sector
			if (clnr & 1) { 
				P[0] = (P[0] & 0x0F) + (val << 4);
			} else {
				P[0] = val;
			}
			disk.writeLogicalSector(partitionOffset + 1 + sectornr , sectorbuf );
			disk.readLogicalSector(partitionOffset + 2 + sectornr , sectorbuf );
			if (clnr & 1) { 
				sectorbuf[0] = val >> 4;
			} else {
				sectorbuf[0] = (sectorbuf[0] & 0xF0) + ((val >> 8) & 0x0F);
			}
			disk.writeLogicalSector(partitionOffset + 2 + sectornr , sectorbuf );
		}
	} else {
		PRT_DEBUG("NO FAT16 YET!");
		//cout << "FAT size 16 not yet tested!!"<<endl;
		//byte *P = FSImage+SECTOR_SIZE + (clnr * 2) ;
		//P[0] = val & 0xFF;
		//P[1] = (val >> 8) & 0xFF;
	}
}

/** Find the next clusternumber marked as free in the FAT
  */
word MSXtar::findFirstFreeCluster(void)
{
	int cluster=2;
	while ((cluster <= maxCluster) && ReadFAT(cluster)) {
		cluster++;
	};
	return cluster;
}

/** Returns the index of a free (or with deleted file) entry
  * in the current dir sector
  * Out: index (is max 15)m if no index is found then 16 is returned
  */
byte MSXtar::findUsableIndexInSector(int sector)
{
	byte buf[SECTOR_SIZE];
	disk.readLogicalSector(partitionOffset+sector , buf);
	return findUsableIndexInSector(buf);
}

byte MSXtar::findUsableIndexInSector(byte* buf)
{
	// find a not used (0x00) or delete entry (0xe5)
	byte* P=buf;
	byte i=0;
	for (; i<16 && P[0]!=0x00 && P[0]!=0xe5; i++,P+=32);
	return i;
}

/** Get the next sector from a file or (root/sub)directory
  * If no next sector then 0 is returned
  */
int MSXtar::getNextSector(int sector)
{
	//if this sector is part of the root directory...
	if  (sector == rootDirEnd){ return 0 ;}
	if  (sector < rootDirEnd){ return sector+1 ;}

	if ((nbSectorsPerCluster > 1) && ( sectorToCluster(sector) == sectorToCluster(sector+1)) ) {
		return sector+1;
	} else {
		int nextcl=ReadFAT(sectorToCluster(sector));
		if (nextcl == EOF_FAT){
			return 0;
		} else {
			return clusterToSector(nextcl);
		}
	}
}

/** if there are no more free entries in a subdirectory, the subdir is
  * expanded with an extra cluster, This function gets the free cluster,
  * clears it and updates the fat for the subdir
  * returns: the first sector in the newly appended cluster, or 0 in case of error
  */
int MSXtar::appendClusterToSubdir(int sector)
{
		word curcl = sectorToCluster(sector);
		if ( ReadFAT(curcl) != EOF_FAT){
			CliComm::instance().printWarning("appendClusterToSubdir called with sector in a not EOF_FAT cluster ");
		}
		word nextcl=findFirstFreeCluster();
		if (nextcl > maxCluster){
			CliComm::instance().printWarning("Disk full no more free clusters");
			return 0;
		}
		int logicalSector=clusterToSector(nextcl);
		//clear this cluster
		byte buf[SECTOR_SIZE];
		memset(buf,0,SECTOR_SIZE);
		for (int i=0 ; i < sectorsPerCluster ; ++i) {
			disk.writeLogicalSector(partitionOffset + i + logicalSector, buf);
		};
		WriteFAT(curcl, nextcl);
		WriteFAT(nextcl, EOF_FAT);
		return logicalSector;
}


/** This function returns the sector and dirindex for a new directory entry
  * if needed the involved subdirectroy is expanded by an extra cluster
  * returns: a struct physDirEntry containing sector and index
  *          if failed then the sectornumber is 0
  */
struct MSXtar::physDirEntry MSXtar::addEntryToDir(int sector,byte direntryindex)
{
	//this routin adds the msxname to a directory sector, if needed (and possible) the directory is extened with an extra cluster
	struct physDirEntry newEntry;
	byte newindex=findUsableIndexInSector(sector);
	if (sector <= rootDirEnd){
		// we are adding this to the root sector
		while (newindex>15 && sector <= rootDirEnd){
			newindex=findUsableIndexInSector(++sector);
		}
		newEntry.sector=sector;
		newEntry.index=newindex;
	} else {
		// we are adding this to a subdir
		if  (newindex<16){
			newEntry.sector=sector;
			newEntry.index=newindex;
		} else {
			int nextsector;
			while (newindex>15 && sector!=0 ){
				nextsector=getNextSector(sector);
				if (nextsector==0){
					nextsector=appendClusterToSubdir(sector);
					PRT_DEBUG("appendClusterToSubdir("<<sector<<") returns"<<nextsector);
					if (nextsector==0){
						CliComm::instance().printWarning("disk is full!");
					}
				}
				sector=nextsector;
				newindex=findUsableIndexInSector(sector);
			};
			newEntry.sector=sector;
			newEntry.index=newindex;
		}
	};
	return newEntry;
}

// create an MSX filename 8.3 format, if needed in vfat like abreviation
//static char MSXtar::toMSXChr(char a)
char toMSXChr(char a)
{
	a = toupper(a);
	if (a == ' ' || a == '.') {
		a = '_';
	}
	return a;
}

/** Transform a long hostname in a 8.3 uppercase filename as used in the
 * direntries on an MSX
 */
std::string MSXtar::makeSimpleMSXFileName(const std::string& fullfilename)
{
	unsigned pos = fullfilename.find_last_of('/');
	string tmp;
	if (pos != string::npos) {
		tmp = fullfilename.substr(pos + 1);
	} else {
		tmp = fullfilename;
	}
	//PRT_DEBUG("filename before transform " << tmp);
	//PRT_DEBUG("filename after transform " << tmp);

	string file, ext;
	pos = tmp.find_last_of('.');
	if (pos != string::npos) {
		file = tmp.substr(0, pos);
		ext  = tmp.substr(pos + 1);
	} else {
		file = tmp;
		ext = "";
	}
	//remove trailing spaces
	while (file.find_last_of(' ')==(file.size()-1) &&
		file.find_last_of(' ') != string::npos){
		file=file.substr(0,file.size() -1 );
		CliComm::instance().printWarning("shrinking file *"+file+"*");
	};
	while (ext.find_last_of(' ')==(ext.size()-1) &&
		ext.find_last_of(' ') != string::npos ){
		ext=ext.substr(0,ext.size() -1 );
		CliComm::instance().printWarning("shrinking ext *"+ext+"*");
	};
	// put in major case and create '_' if needed
	transform(file.begin(), file.end(), file.begin(), toMSXChr);
	transform(ext.begin(), ext.end(), ext.begin(), toMSXChr);

	PRT_DEBUG("adding correct amount of spaces");
	file += "        ";
	ext  += "   ";
	file = file.substr(0, 8);
	ext  = ext.substr(0, 3);

	return file + ext;
}

/** This function creates a new MSX subdir with given date 'd' and time 't'
  * in the subdir pointed at by 'sector' and 'direntryindex' in the newly
  * created subdir the entries for '.' and '..' are created
  * returns: the first sector of the new subdir
  *          0 in case no directory could be created
  */
int MSXtar::AddMSXSubdir(string msxName,int t,int d,int sector,byte direntryindex)
{
	// returns the sector for the first cluster of this subdir
	struct physDirEntry result;
	result=addEntryToDir(sector,direntryindex);
	if (result.index > 15){
		CliComm::instance().printWarning( "couldn't add entry"+msxName );
		return 0;
	}
	//load the sector
	byte buf[SECTOR_SIZE];
	disk.readLogicalSector(partitionOffset + result.sector , buf );

	struct MSXDirEntry* direntry =(MSXDirEntry*)(buf+32*result.index);
	direntry->attrib=T_MSX_DIR;
	setsh(direntry->time, t);
	setsh(direntry->date, d);
	memcpy(direntry,makeSimpleMSXFileName(msxName).c_str(),11);

	//direntry->filesize=fsize;
	word curcl = 2;
	curcl=findFirstFreeCluster();
	PRT_DEBUG("New subdir starting at cluster " << curcl );
	setsh(direntry->startcluster, curcl);
	WriteFAT(curcl, EOF_FAT);
	//save the sector again
	disk.writeLogicalSector(partitionOffset + result.sector , buf );

	//clear this cluster
	int logicalSector=clusterToSector(curcl);
	memset(buf,0,SECTOR_SIZE);
	for (int i=0 ; i < sectorsPerCluster ; ++i) {
		disk.writeLogicalSector(partitionOffset + i + logicalSector, buf);
	};
	// now add the '.' and '..' entries!!
	direntry =(MSXDirEntry*)(buf);
	memset(direntry, 0, sizeof(struct MSXDirEntry));
	memset(direntry, 0x20, 11); //all spaces
	memset(direntry, '.', 1); 
	direntry->attrib=T_MSX_DIR;
	setsh(direntry->time, t);
	setsh(direntry->date, d);
	setsh(direntry->startcluster, curcl);

	direntry++;
	memset(direntry, 0, sizeof(struct MSXDirEntry));
	memset(direntry, 0x20, 11); //all spaces
	memset(direntry, '.', 2); 
	direntry->attrib=T_MSX_DIR;
	setsh(direntry->time, t);
	setsh(direntry->date, d);
	//int parentcluster=( sector-10)/2;
	//setsh(direntry->startcluster, parentcluster);
	setsh(direntry->startcluster, sectorToCluster(sector));
	//and save this in the first sector of the new subdir
	disk.writeLogicalSector(partitionOffset + logicalSector, buf);

	return logicalSector;
}
/** Add an MSXsubdir with the time properties from the HOST-OS subdir
 */
int MSXtar::AddSubdirtoDSK(string hostName,string msxName,int sector,byte direntryindex)
{
	//struct MSXDirEntry* direntry =(MSXDirEntry*)(FSImage+SECTOR_SIZE*result.sector+32*result.index);

	// compute time/date stamps
	struct stat fst;
	memset(&fst, 0, sizeof(struct stat));
	stat(hostName.c_str(), &fst);
	struct tm mtim = *localtime(&(fst.st_mtime));
	int t = (mtim.tm_sec >> 1) + (mtim.tm_min << 5) +
		(mtim.tm_hour << 11);
	int d = mtim.tm_mday + ((mtim.tm_mon + 1) << 5) +
	    ((mtim.tm_year + 1900 - 1980) << 9);

	int logicalSector=AddMSXSubdir(msxName,t,d,sector,direntryindex);
	return logicalSector;
}

/** This file alters the filecontent of agiven file
  * It only changes the file content (and the filesize in the msxdirentry)
  * It doesn't changes timestamps nor filename, filetype etc.
  * Output: nothing usefull yet
  */
int MSXtar::AlterFileInDSK(struct MSXDirEntry* msxdirentry, string hostName)
{
	bool needsNew=false;
	int fsize;
	struct stat fst;
	memset(&fst, 0, sizeof(struct stat));
	stat(hostName.c_str(), &fst);
	fsize = fst.st_size;
	PRT_DEBUG("AlterFileInDSK: Filesize " << fsize);

	word curcl =rdsh(msxdirentry->startcluster) ;
	// if entry first used then no cluster assigned yet
	if (curcl==0){
		curcl=findFirstFreeCluster();
		setsh(msxdirentry->startcluster, curcl);
		WriteFAT(curcl, EOF_FAT); 
		needsNew=true;
	}
	PRT_DEBUG("AlterFileInDSK: Starting at cluster " << curcl );

	int size = fsize;
	int prevcl = 0;
	//open file for reading
	FILE* file = fopen(hostName.c_str(),"rb");
	byte buf[SECTOR_SIZE];

	while (size && (curcl <= maxCluster)) {
		int logicalSector= clusterToSector(curcl);
		disk.readLogicalSector(partitionOffset + logicalSector , buf );
		for (int j=0 ; j < sectorsPerCluster ; j++){
			if (size){
				PRT_DEBUG("AlterFileInDSK: relative sector "<<j<<" in cluster "<<curcl);
				// more poitical correct:
				//We should read 'size'-bytes instead of 'SECTOR_SIZE' if it is the end of the file
				fread(buf,1,SECTOR_SIZE,file);
				disk.writeLogicalSector(partitionOffset + logicalSector + j , buf );
			}
			size -= (size > SECTOR_SIZE  ? SECTOR_SIZE  : size);
		}

		if (prevcl) {
			WriteFAT(prevcl, curcl);
		}
		prevcl = curcl;
		//now we check if we continue in the current clusterstring or need to allocate extra unused blocks
		//do {
		if (needsNew){
			//need extra space for this file
			WriteFAT(curcl, EOF_FAT); 
			curcl=findFirstFreeCluster();
		} else {
			// follow cluster-Fat-stream
			curcl=ReadFAT(curcl);
			if (curcl == EOF_FAT){
				curcl=findFirstFreeCluster();
				needsNew=true;
			}
		}
		//} while((curcl <= maxCluster) && ReadFAT(curcl));
		PRT_DEBUG("AlterFileInDSK: Continuing at cluster " << curcl);
	}
	fclose(file);

	if ((size == 0) && (curcl <= maxCluster)) {
		// TODO: check what an MSX does with filesize zero and fat allocation;
		if(prevcl==0) {
			prevcl=curcl;
			curcl=ReadFAT(curcl);
		};
		WriteFAT(prevcl, EOF_FAT); 
		PRT_DEBUG("AlterFileInDSK: Ending at cluster " << prevcl);
		//cleaning rest of FAT chain if needed
		while (!needsNew){
			prevcl=curcl;
			if (curcl != EOF_FAT){
				curcl=ReadFAT(curcl);
			} else {
				needsNew=true;
			}
			PRT_DEBUG("AlterFileInDSK: Cleaning cluster " << prevcl << " from FAT");
			WriteFAT(prevcl, 0);
		}
		
	} else {
		//TODO: don't we need a EOF_FAT in this case as well ?
		// find out and adjust code here
		CliComm::instance().printWarning( "Fake Diskimage full: " + hostName + " truncated.");
	}
	//write (possibly truncated) file size
	setlg(msxdirentry->size,fsize-size);
	return 0;
}

/** Find the dir entry for 'name' in subdir starting at the given 'sector'
  * with given 'index'
  * returns: a fullMSXDirEntry struct if name was found
  *          a direntryindex is 16 if no match was found
  */
struct MSXtar::fullMSXDirEntry MSXtar::findEntryInDir(string name,int sector,byte direntryindex,byte* sectorbuf)
{
	struct fullMSXDirEntry result;
	result.sectorbuf=sectorbuf;

	disk.readLogicalSector(partitionOffset + sector , sectorbuf );
	byte* P=sectorbuf+32*direntryindex;
	byte i=direntryindex;
	do {
		for (; i<16 && strncmp(name.c_str(),(char*)P,11)!=0; i++,P+=32);
		if (i==16){
			sector=getNextSector(sector);
			disk.readLogicalSector(partitionOffset + sector , sectorbuf );
			P=sectorbuf;
			if (sector) i=0;
		};
		
	} while (i>15 && sector);

	result.sector=sector;
	result.direntryindex=i;
	return result;
}


//TODO: transform to msxtar.cc code from here on down
//

/** Add file to the MSX disk in the subdir pointed to by 'sector'
  * returns: nothing usefull yet :-)
  */
int MSXtar::AddFiletoDSK(string hostName,string msxName,int sector,byte direntryindex)
{
	//first find out if the filename already exists current dir
	byte buf[SECTOR_SIZE];
	struct fullMSXDirEntry fullmsxdirentry=findEntryInDir(makeSimpleMSXFileName(msxName),sector,direntryindex,buf);
	if ( fullmsxdirentry.direntryindex == 16 ) {
//	if (msxdirentry!=NULL)
		CliComm::instance().printWarning("Preserving entry "+msxName );
	  return 0;
	};
	struct physDirEntry result;
	result=addEntryToDir(sector,direntryindex);
	if (result.index > 15){
		CliComm::instance().printWarning( "couldn't add entry"+hostName);
		return 255;
	}
	disk.readLogicalSector(partitionOffset + result.sector , buf );
	struct MSXDirEntry* direntry =(MSXDirEntry*)(buf+32*result.index);
	direntry->attrib=T_MSX_REG;
//	PRT_VERBOSE(hostName<<" \t-> \""<<makeSimpleMSXFileName(msxName) << '"' );
	memcpy(direntry,makeSimpleMSXFileName(msxName).c_str(),11);

	// compute time/date stamps
	struct stat fst;
	memset(&fst, 0, sizeof(struct stat));
	stat(hostName.c_str(), &fst);
	struct tm mtim = *localtime(&(fst.st_mtime));
	int t = (mtim.tm_sec >> 1) + (mtim.tm_min << 5) +
		(mtim.tm_hour << 11);
	setsh(direntry->time, t);
	t = mtim.tm_mday + ((mtim.tm_mon + 1) << 5) +
	    ((mtim.tm_year + 1900 - 1980) << 9);
	setsh(direntry->date, t);

	AlterFileInDSK(direntry,hostName);
	disk.writeLogicalSector(partitionOffset + result.sector , buf );
	return 0; //avoid warning
}

/** transfer directory and all its subdirectories to the MSX diskimage
  */
void MSXtar::recurseDirFill(const string &DirName,int sector,int direntryindex)
{
	int result;

	PRT_DEBUG("Trying to read directory"<<DirName);

	DIR* dir = opendir(DirName.c_str());

	if (dir == NULL ) {
		PRT_DEBUG("Not a directory");
		//throw MSXException("Not a directory");
		return;
	}
	//read directory and fill the fake disk
	struct dirent* d = readdir(dir);
	while (d) {
		string name(d->d_name);
		PRT_DEBUG("reading name in dir :" << name);
		if ( d->d_type ==DT_REG ) {
			result=AddFiletoDSK(DirName + '/' + name,name,sector,direntryindex); // used here to add file into fake dsk
		}
		else if (d->d_type ==DT_DIR && name != string(".") && name != string("..") ) {
			if (do_subdirs){
			CliComm::instance().printWarning(DirName + '/' + name +" \t-> \""+ makeSimpleMSXFileName(name) +  '"' );
			byte buf[SECTOR_SIZE];
			struct fullMSXDirEntry fullmsxdirentry=findEntryInDir(makeSimpleMSXFileName(name),sector,direntryindex,buf);
			if (fullmsxdirentry.direntryindex != 16){
			  CliComm::instance().printWarning("Dir entry "+name +" exists already ");
			  struct MSXDirEntry* msxdirentry=(struct MSXDirEntry*)(buf + 32 * fullmsxdirentry.direntryindex) ;
			  result= clusterToSector(rdsh(msxdirentry->startcluster));
			} else {
			  CliComm::instance().printWarning("Adding dir entry "+name);
			result=AddSubdirtoDSK(DirName + '/' + name,name,sector,direntryindex); // used here to add file into fake dsk
			}

			recurseDirFill( DirName + '/' + name ,result,0);
			} else {
			PRT_DEBUG("Skipping subdir:"<< DirName + "/" + name);
			}
		};
		d = readdir(dir);
	}
	closedir(dir);
}


//temporary way to test import MSXtar functionality
void MSXtar::addDir(const std::string &rootDirName)
{
   recurseDirFill(rootDirName,0,0);
}

} // namespace openmsx
