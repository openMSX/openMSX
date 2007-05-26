// $Id$

#include "MSXtar.hh"
#include "ReadDir.hh"
#include "SectorAccessibleDisk.hh"
#include "FileOperations.hh"
#include "MSXException.hh"
#include "StringOp.hh"
#include "BootBlocks.hh"
#include <sys/stat.h>
#include <time.h>
#include <utime.h>
#include <unistd.h>
#include <cstdlib>
#include <cstring>
#include <algorithm>
#include <cassert>

using std::string;

namespace openmsx {

static const unsigned EOF_FAT = 0x0FFF; // signals EOF in FAT12
static const unsigned SECTOR_SIZE = 512;

static const byte T_MSX_REG  = 0x00; // Normal file
static const byte T_MSX_READ = 0x01; // Read-Only file
static const byte T_MSX_HID  = 0x02; // Hidden file
static const byte T_MSX_SYS  = 0x04; // System file
static const byte T_MSX_VOL  = 0x08; // filename is Volume Label
static const byte T_MSX_DIR  = 0x10; // entry is a subdir
static const byte T_MSX_ARC  = 0x20; // Archive bit
// This particular combination of flags indicates that this dir entry is used
// to store a long Unicode file name.
// For details, read http://home.teleport.com/~brainy/lfn.htm
static const byte T_MSX_LFN  = 0x0F; // LFN entry (long files names)

// functions to change DirEntries
static inline void setsh(byte* x, unsigned y)
{
	x[0] = (y >> 0) & 0xFF;
	x[1] = (y >> 8) & 0xFF;
}
static inline void setlg(byte* x, unsigned y)
{
	x[0] = (y >>  0) & 0xFF;
	x[1] = (y >>  8) & 0xFF;
	x[2] = (y >> 16) & 0xFF;
	x[3] = (y >> 24) & 0xFF;
}

// functions to read DirEntries
static inline unsigned rdsh(const byte* x)
{
	return (x[0] << 0) + (x[1] << 8);
}
static inline unsigned rdlg(const byte* x)
{
	return (x[0] << 0) + (x[1] << 8) + (x[2] << 16) + (x[3] << 24);
}

/** Transforms a clusternumber towards the first sector of this cluster
  * The calculation uses info read fom the bootsector
  */
unsigned MSXtar::clusterToSector(unsigned cluster)
{
	return 1 + rootDirLast + sectorsPerCluster * (cluster - 2);
}

/** Transforms a sectornumber towards it containing cluster
  * The calculation uses info read fom the bootsector
  */
unsigned MSXtar::sectorToCluster(unsigned sector)
{
	return 2 + ((sector - (1 + rootDirLast)) / sectorsPerCluster);
}


/** Initialize object variables by reading info from the bootsector
  */
void MSXtar::parseBootSector(const byte* buf)
{
	const MSXBootSector* boot = (const MSXBootSector*)buf;
	unsigned nbSectors = rdsh(boot->nrsectors);
	if (nbSectors == 0) { // TODO: check limits more accurately
		throw MSXException("Illegal number of sectors: " +
			StringOp::toString(nbSectors));
	}
	unsigned nbSides = rdsh(boot->nrsides);
	if (nbSides == 0) { // TODO: check limits more accurately
		throw MSXException("Illegal number of sides: " +
			StringOp::toString(nbSides));
	}
	unsigned nbFats = boot->nrfats[0];
	if (nbFats == 0) { // TODO: check limits more accurately
		throw MSXException("Illegal number of FATs: " +
			StringOp::toString(nbFats));
	}
	sectorsPerFat = rdsh(boot->sectorsfat);
	if (sectorsPerFat == 0) { // TODO: check limits more accurately
		throw MSXException("Illegal number sectors per FAT: " +
			StringOp::toString(sectorsPerFat));
	}
	unsigned nbRootDirSectors = rdsh(boot->direntries) / 16;
	if (nbRootDirSectors == 0) { // TODO: check limits more accurately
		throw MSXException("Illegal number of root dir sectors: " +
			StringOp::toString(nbRootDirSectors));
	}
	sectorsPerCluster = boot->spcluster[0];
	if (sectorsPerCluster == 0) { // TODO: check limits more accurately
		throw MSXException("Illegal number of sectors per cluster: " +
			StringOp::toString(sectorsPerCluster));
	}
	rootDirStart = 1 + nbFats * sectorsPerFat;
	chrootSector = rootDirStart;
	rootDirLast = rootDirStart + nbRootDirSectors - 1;
	maxCluster = sectorToCluster(nbSectors);
}

void MSXtar::parseBootSectorFAT(const byte* buf)
{
	// empty FAT buffer before filling it again (or for the first time)
	writeCachedFAT();

	try {
		parseBootSector(buf);
	} catch (MSXException& e) {
		throw MSXException("Bad disk image: " + e.getMessage());
	}

	// cache complete FAT
	assert(fatBuffer.empty()); // cache must be empty at this point
	fatCacheDirty = false;
	fatBuffer.resize(SECTOR_SIZE * sectorsPerFat);
	for (unsigned i = 0; i < sectorsPerFat; ++i) {
		disk.readLogicalSector(i + 1 + partitionOffset,
		                       &fatBuffer[SECTOR_SIZE * i]);
	}
}

void MSXtar::writeLogicalSector(unsigned sector, const byte* buf)
{
	assert(sector < partitionNbSectors);
	unsigned fatSector = sector - 1;
	if ((fatSector < (unsigned)sectorsPerFat) && !fatBuffer.empty()) {
		// we have a cache and this is a sector of the 1st FAT
		//   --> update cache
		memcpy(&fatBuffer[SECTOR_SIZE * fatSector], buf, SECTOR_SIZE);
		fatCacheDirty = true;
	} else {
		disk.writeLogicalSector(sector + partitionOffset, buf);
	}
}

void MSXtar::readLogicalSector(unsigned sector, byte* buf)
{
	assert(sector < partitionNbSectors);
	unsigned fatSector = sector - 1;
	if ((fatSector < (unsigned)sectorsPerFat) && !fatBuffer.empty()) {
		// we have a cache and this is a sector of the 1st FAT
		//   --> read from cache
		memcpy(buf, &fatBuffer[SECTOR_SIZE * fatSector], SECTOR_SIZE);
	} else {
		disk.readLogicalSector(sector + partitionOffset, buf);
	}
}

MSXtar::MSXtar(SectorAccessibleDisk& sectordisk)
	: disk(sectordisk)
{
	static bool init = false;
	if (!init) {
		init = true;
		srand(time(0));
	}

	fatCacheDirty = false;
	partitionOffset = 0;
	partitionNbSectors = disk.getNbSectors();
	if (partitionNbSectors == 0) {
		throw MSXException("No disk inserted.");
	}
}

MSXtar::~MSXtar()
{
	try {
		writeCachedFAT();
	} catch (MSXException& e) {
		// nothing
	}
}

// write cached FAT back to disk image
void MSXtar::writeCachedFAT()
{
	if (fatCacheDirty) {
		for (unsigned i = 0; i < fatBuffer.size() / SECTOR_SIZE; ++i) {
			disk.writeLogicalSector(i + 1 + partitionOffset,
			                        &fatBuffer[SECTOR_SIZE * i]);
		}
		fatCacheDirty = false;
	}
	fatBuffer.clear();
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

	// set random volume id
	for (int i = 0x27; i < 0x2B; ++i) {
		buf[i] = rand() & 0x7F;
	}
}

// Format a diskimage with correct bootsector, FAT etc.
void MSXtar::format()
{
	// first create a bootsector and start from the default bootblock
	byte sectorbuf[512];
	const byte* defaultBootBlock = BootBlocks::dos2BootBlock;
	memcpy(sectorbuf, defaultBootBlock, SECTOR_SIZE);
	setBootSector(sectorbuf, partitionNbSectors);
	try {
		parseBootSector(sectorbuf); //set object variables to correct values
	} catch (MSXException& e) {
		throw MSXException("Internal error, invalid defaultBootBlock: " +
		                   e.getMessage());
	}
	writeLogicalSector(0, sectorbuf);

	MSXBootSector* boot = (MSXBootSector*)sectorbuf;
	byte descriptor = boot->descriptor[0];

	// Assign default empty values to disk
	memset(sectorbuf, 0x00, SECTOR_SIZE);
	for (unsigned i = 2; i <= rootDirLast; ++i) {
		writeLogicalSector(i, sectorbuf);
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

	writeLogicalSector(1, sectorbuf);

	memset(sectorbuf, 0xE5, SECTOR_SIZE);
	for (unsigned i = 1 + rootDirLast; i < partitionNbSectors; ++i) {
		writeLogicalSector(i, sectorbuf);
	}
}

// Get the next clusternumber from the FAT chain
unsigned MSXtar::readFAT(unsigned clnr) const
{
	assert(!fatBuffer.empty()); // FAT must already be cached
	const byte* p = &fatBuffer[(clnr * 3) / 2];
	return (clnr & 1)
	       ? (p[0] >> 4) + (p[1] << 4)
	       : p[0] + ((p[1] & 0x0F) << 8);
}

// Write an entry to the FAT
void MSXtar::writeFAT(unsigned clnr, unsigned val)
{
	assert(!fatBuffer.empty()); // FAT must already be cached
	assert(val < 4096); // FAT12
	byte* p = &fatBuffer[(clnr * 3) / 2];
	if (clnr & 1) {
		p[0] = (p[0] & 0x0F) + (val << 4);
		p[1] = val >> 4;
	} else {
		p[0] = val;
		p[1] = (p[1] & 0xF0) + ((val >> 8) & 0x0F);
	}
	fatCacheDirty = true;
}

// Find the next clusternumber marked as free in the FAT
// @throws When no more free clusters
unsigned MSXtar::findFirstFreeCluster()
{
	for (unsigned cluster = 2; cluster < maxCluster; ++cluster) {
		if (readFAT(cluster) == 0) {
			return cluster;
		}
	}
	throw MSXException("Disk full.");
}

// Get the next sector from a file or (root/sub)directory
// If no next sector then 0 is returned
unsigned MSXtar::getNextSector(unsigned sector)
{
	if (sector <= rootDirLast) {
		// sector is part of the root directory
		return (sector == rootDirLast) ? 0 : sector + 1;
	}
	unsigned currCluster = sectorToCluster(sector);
	if (currCluster == sectorToCluster(sector + 1)) {
		// next sector of cluster
		return sector + 1;
	} else {
		// first sector in next cluster
		unsigned nextcl = readFAT(currCluster);
		return (nextcl == EOF_FAT) ? 0 : clusterToSector(nextcl);
	}
}

// If there are no more free entries in a subdirectory, the subdir is
// expanded with an extra cluster. This function gets the free cluster,
// clears it and updates the fat for the subdir
// returns: the first sector in the newly appended cluster
// @throws When disk is full
unsigned MSXtar::appendClusterToSubdir(unsigned sector)
{
	unsigned nextcl = findFirstFreeCluster();
	unsigned logicalSector = clusterToSector(nextcl);

	// clear this cluster
	byte buf[SECTOR_SIZE];
	memset(buf, 0, SECTOR_SIZE);
	for (unsigned i = 0; i < sectorsPerCluster; ++i) {
		writeLogicalSector(i + logicalSector, buf);
	}

	unsigned curcl = sectorToCluster(sector);
	assert(readFAT(curcl) == EOF_FAT);
	writeFAT(curcl, nextcl);
	writeFAT(nextcl, EOF_FAT);
	return logicalSector;
}


// Returns the index of a free (or with deleted file) entry
//  In:  The current dir sector
//  Out: index number, if no index is found then -1 is returned
unsigned MSXtar::findUsableIndexInSector(unsigned sector)
{
	byte buf[SECTOR_SIZE];
	readLogicalSector(sector, buf);
	MSXDirEntry* direntry = (MSXDirEntry*)buf;

	// find a not used (0x00) or delete entry (0xE5)
	for (unsigned i = 0; i < 16; ++i) {
		byte tmp = direntry[i].filename[0];
		if ((tmp == 0x00) || (tmp == 0xE5)) {
			return i;
		}
	}
	return (unsigned)-1;
}

// This function returns the sector and dirindex for a new directory entry
// if needed the involved subdirectroy is expanded by an extra cluster
// returns: a DirEntry containing sector and index
// @throws When either root dir is full or disk is full
MSXtar::DirEntry MSXtar::addEntryToDir(unsigned sector)
{
	// this routine adds the msxname to a directory sector, if needed (and
	// possible) the directory is extened with an extra cluster
	DirEntry result;
	result.sector = sector;

	if (sector <= rootDirLast) {
		// add to the root directory
		for (/* */ ; result.sector <= rootDirLast; result.sector++) {
			result.index = findUsableIndexInSector(result.sector);
			if (result.index != (unsigned)-1) {
				return result;
			}
		}
		throw MSXException("Root directory full.");

	} else {
		// add to a subdir
		while (true) {
			result.index = findUsableIndexInSector(result.sector);
			if (result.index != (unsigned)-1) {
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
unsigned MSXtar::addSubdir(
	const string& msxName, unsigned t, unsigned d, unsigned sector)
{
	// returns the sector for the first cluster of this subdir
	DirEntry result = addEntryToDir(sector);

	// load the sector
	byte buf[SECTOR_SIZE];
	readLogicalSector(result.sector, buf);
	MSXDirEntry* direntries = (MSXDirEntry*)buf;

	MSXDirEntry& direntry = direntries[result.index];
	direntry.attrib = T_MSX_DIR;
	setsh(direntry.time, t);
	setsh(direntry.date, d);
	memcpy(&direntry, makeSimpleMSXFileName(msxName).data(), 11);

	// direntry.filesize = fsize;
	unsigned curcl = findFirstFreeCluster();
	setsh(direntry.startcluster, curcl);
	writeFAT(curcl, EOF_FAT);

	// save the sector again
	writeLogicalSector(result.sector, buf);

	//clear this cluster
	unsigned logicalSector = clusterToSector(curcl);
	memset(buf, 0, SECTOR_SIZE);
	for (unsigned i = 0; i < sectorsPerCluster; ++i) {
		writeLogicalSector(i + logicalSector, buf);
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

	// and save this in the first sector of the new subdir
	writeLogicalSector(logicalSector, buf);

	return logicalSector;
}

static void getTimeDate(time_t& totalSeconds, unsigned& time, unsigned& date)
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
static void getTimeDate(const string& filename, unsigned& time, unsigned& date)
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
unsigned MSXtar::addSubdirToDSK(const string& hostName, const string& msxName,
                                unsigned sector)
{
	unsigned time, date;
	getTimeDate(hostName, time, date);
	return addSubdir(msxName, time, date, sector);
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
	unsigned hostSize = st.st_size;
	unsigned remaining = hostSize;

	// open host file for reading
	FILE* file = fopen(hostName.c_str(), "rb");
	if (!file) {
		throw MSXException("Error reading host file: " + hostName);
	}

	// copy host file to image
	unsigned prevcl = 0;
	unsigned curcl = rdsh(msxdirentry.startcluster);
	while (remaining) {
		// allocate new cluster if needed
		try {
			if ((curcl == 0) || (curcl == EOF_FAT)) {
				unsigned newcl = findFirstFreeCluster();
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
		unsigned logicalSector = clusterToSector(curcl);
		for (unsigned j = 0; (j < sectorsPerCluster) && remaining; ++j) {
			byte buf[SECTOR_SIZE];
			memset(buf, 0, SECTOR_SIZE);
			unsigned chunkSize = std::min(SECTOR_SIZE, remaining);
			fread(buf, 1, chunkSize, file);
			writeLogicalSector(logicalSector + j, buf);
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
		unsigned nextcl = readFAT(curcl);
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
MSXtar::DirEntry MSXtar::findEntryInDir(
	const string& name, unsigned sector, byte* buf)
{
	DirEntry result;
	result.sector = sector;
	result.index = 0; // avoid warning (only some gcc versions complain) 
	while (result.sector) {
		// read sector and scan 16 entries
		readLogicalSector(result.sector, buf);
		MSXDirEntry* direntries = (MSXDirEntry*)buf;
		for (result.index = 0; result.index < 16; ++result.index) {
			if (string((char*)direntries[result.index].filename, 11)
			     == name) {
				return result;
			}
		}
		// try next sector
		result.sector = getNextSector(result.sector);
	}
	return result;
}

// Add file to the MSX disk in the subdir pointed to by 'sector'
// @throws when file could not be added
string MSXtar::addFileToDSK(const string& fullname, unsigned rootSector)
{
	string dir, hostName;
	StringOp::splitOnLast(fullname, "/\\", dir, hostName);
	string msxName = makeSimpleMSXFileName(hostName);

	// first find out if the filename already exists in current dir
	byte dummy[SECTOR_SIZE];
	DirEntry fullmsxdirentry = findEntryInDir(msxName, rootSector, dummy);
	if (fullmsxdirentry.sector != 0) {
		// TODO implement overwrite option
		return "Warning: preserving entry " + hostName + '\n';
	}

	byte buf[SECTOR_SIZE];
	DirEntry dirEntry = addEntryToDir(rootSector);
	readLogicalSector(dirEntry.sector, buf);
	MSXDirEntry* direntries = (MSXDirEntry*)buf;
	MSXDirEntry& direntry = direntries[dirEntry.index];
	memset(&direntry, 0, sizeof(MSXDirEntry));
	memcpy(&direntry, msxName.c_str(), 11);
	direntry.attrib = T_MSX_REG;

	// compute time/date stamps
	unsigned time, date;
	getTimeDate(fullname, time, date);
	setsh(direntry.time, time);
	setsh(direntry.date, date);

	try {
		alterFileInDSK(direntry, fullname);
	} catch (MSXException& e) {
		// still write directory entry
		writeLogicalSector(dirEntry.sector, buf);
		throw;
	}
	writeLogicalSector(dirEntry.sector, buf);
	return "";
}

// Transfer directory and all its subdirectories to the MSX diskimage
// @throws when an error occurs
string MSXtar::recurseDirFill(const string& dirName, unsigned sector)
{
	string messages;
	ReadDir dir(dirName);
	while (dirent* d = dir.getEntry()) {
		string name(d->d_name);
		string fullName = dirName + '/' + name;

		if (FileOperations::isRegularFile(fullName)) {
			// add new file
			messages += addFileToDSK(fullName, sector);

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
					unsigned nextSector = clusterToSector(
						rdsh(msxdirentry.startcluster));
					messages += recurseDirFill(fullName, nextSector);
				} else {
					// .. but is NOT a directory
					messages += "MSX file " + msxFileName +
					            " is not a directory.\n";
				}
			} else {
				// add new directory
				unsigned nextSector = addSubdirToDSK(fullName, name, sector);
				messages += recurseDirFill(fullName, nextSector);
			}
		}
	}
	return messages;
}


string MSXtar::condensName(MSXDirEntry& direntry)
{
	string result;
	for (unsigned i = 0; (i < 8) && (direntry.filename[i] != ' '); ++i) {
		result += tolower(direntry.filename[i]);
	}
	if (direntry.ext[0] != ' ') {
		result += '.';
		for (unsigned i = 0; (i < 3) && (direntry.ext[i] != ' '); ++i) {
			result += tolower(direntry.ext[i]);
		}
	}
	return result;
}


// Set the entries from direntry to the timestamp of resultFile
void MSXtar::changeTime(string resultFile, MSXDirEntry& direntry)
{
	unsigned t = rdsh(direntry.time);
	unsigned d = rdsh(direntry.date);
	struct tm mtim;
	struct utimbuf utim;
	mtim.tm_sec   =  (t & 0x001f) << 1;
	mtim.tm_min   =  (t & 0x03e0) >> 5;
	mtim.tm_hour  =  (t & 0xf800) >> 11;
	mtim.tm_mday  =  (d & 0x001f);
	mtim.tm_mon   =  (d & 0x01e0) >> 5;
	mtim.tm_year  = ((d & 0xfe00) >> 9) + 80;
	mtim.tm_isdst = -1;
	utim.actime  = mktime(&mtim);
	utim.modtime = mktime(&mtim);
	utime(resultFile.c_str(), &utim);
}

string MSXtar::dir()
{
	string result;
	for (unsigned sector = chrootSector; sector != 0; sector = getNextSector(sector)) {
		byte buf[SECTOR_SIZE];
		readLogicalSector(sector, buf);
		MSXDirEntry* direntry = (MSXDirEntry*)buf;
		for (unsigned i = 0; i < 16; ++i) {
			if ((direntry[i].filename[0] != 0xe5) &&
			    (direntry[i].filename[0] != 0x00) &&
			    (direntry[i].attrib != T_MSX_LFN)) {
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

// routines to update the global vars: chrootSector
void MSXtar::chdir(const string& newRootDir)
{
	chroot(newRootDir, false);
}

void MSXtar::mkdir(const string& newRootDir)
{
	unsigned tmpMSXchrootSector = chrootSector;
	chroot(newRootDir, true);
	chrootSector = tmpMSXchrootSector;
}

void MSXtar::chroot(const string& newRootDir, bool createDir)
{
	string work = newRootDir;
	if (work.find_first_of("/\\") == 0) {
		// absolute path, reset chrootSector
		chrootSector = rootDirStart;
		StringOp::trimLeft(work, "/\\");
	}

	while (!work.empty()) {
		string firstpart;
		StringOp::splitOnFirst(work, "/\\", firstpart, work);
		StringOp::trimLeft(work, "/\\");

		// find 'firstpart' directory or create it if requested
		byte buf[SECTOR_SIZE];
		string simple = makeSimpleMSXFileName(firstpart);
		DirEntry entry = findEntryInDir(simple, chrootSector, buf);
		if (entry.sector == 0) {
			if (!createDir) {
				throw MSXException("Subdirectory " + firstpart +
				                   " not found.");
			}
			// creat new subdir
			time_t now;
			time(&now);
			unsigned t, d;
			getTimeDate(now, t, d);
			chrootSector = addSubdir(simple, t, d, chrootSector);
		} else {
			MSXDirEntry* direntries = (MSXDirEntry*)buf;
			MSXDirEntry& direntry = direntries[entry.index];
			if (!(direntry.attrib & T_MSX_DIR)) {
				throw MSXException(firstpart + " is not a directory.");
			}
			chrootSector = clusterToSector(rdsh(direntry.startcluster));
		}
	}
}

void MSXtar::fileExtract(string resultFile, MSXDirEntry& direntry)
{
	unsigned size = rdlg(direntry.size);
	unsigned sector = clusterToSector(rdsh(direntry.startcluster));

	FILE* file = fopen(resultFile.c_str(), "wb");
	if (!file) {
		throw MSXException("Couldn't open file for writing!");
	}
	while (size && sector) {
		byte buf[SECTOR_SIZE];
		readLogicalSector(sector, buf);
		unsigned savesize = std::min(size, SECTOR_SIZE);
		fwrite(buf, 1, savesize, file);
		size -= savesize;
		sector = getNextSector(sector);
	}
	fclose(file);
	// now change the access time
	changeTime(resultFile, direntry);
}

void MSXtar::recurseDirExtract(const string& dirName, unsigned sector)
{
	for (/* */ ; sector != 0; sector = getNextSector(sector)) {
		byte buf[SECTOR_SIZE];
		readLogicalSector(sector, buf);
		MSXDirEntry* direntry = (MSXDirEntry*)buf;
		for (unsigned i = 0; i < 16; ++i) {
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
static bool isPartitionTableSector(byte* buf)
{
	return strncmp((char*)buf, PARTAB_HEADER, 11) == 0;
}

bool MSXtar::hasPartitionTable()
{
	byte buf[SECTOR_SIZE];
	disk.readLogicalSector(0, buf);
	return isPartitionTableSector(buf);
}

bool MSXtar::hasPartition(unsigned partition)
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

bool MSXtar::usePartition(unsigned partition)
{
	// TODO: separate partition selection from boot sector parsing
	// (format only needs the former, and the latter can throw exceptions)
	byte partitionTable[SECTOR_SIZE];
	partitionOffset = 0;
	partitionNbSectors = disk.getNbSectors();
	disk.readLogicalSector(0, partitionTable);
	bool hasPartitionTable = isPartitionTableSector(partitionTable);
	if (hasPartitionTable) {
		Partition* p = (Partition*)
			(partitionTable + 14 + (30 - partition) * 16);
		if (rdlg(p->start4) != 0) {
			partitionOffset = rdlg(p->start4);
			partitionNbSectors = rdlg(p->size4);
			if (p->sys_ind != 0x01) {
				throw MSXException("Not a FAT12 partition");
			}
		} else {
			hasPartitionTable = false;
		}
	}

	byte bootSector[SECTOR_SIZE];
	disk.readLogicalSector(partitionOffset, bootSector);
	parseBootSectorFAT(bootSector);
	return hasPartitionTable;
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

void MSXtar::createDiskFile(std::vector<unsigned> sizes)
{
	byte buf[SECTOR_SIZE];
	assert(fatBuffer.empty()); //when creating disks we shouldn't have any fatBuffer

	// create the partition table if needed
	if (sizes.size() > 1) {
		memset(buf, 0, SECTOR_SIZE);
		strncpy((char*)buf, PARTAB_HEADER, 11);
		buf[SECTOR_SIZE - 2] = 0x55;
		buf[SECTOR_SIZE - 1] = 0xAA;

		partitionOffset = 1;
		for (unsigned i = 0; (i < sizes.size()) && (i < 30); ++i) {
			Partition* p = (Partition*)(buf + (14 + (30 - i) * 16));
			unsigned startCylinder, startHead, startSector;
			unsigned endCylinder, endHead, endSector;
			logicalToCHS(partitionOffset,
			             startCylinder, startHead, startSector);
			logicalToCHS(partitionOffset + sizes[i] - 1,
			             endCylinder, endHead, endSector);
			p->boot_ind = (i == 0) ? 0x80 : 0x00; // bootflag
			p->head = startHead;
			p->sector = startSector;
			p->cyl = startCylinder;
			p->sys_ind = 0x01; // FAT12
			p->end_head = endHead;
			p->end_sector = endSector;
			p->end_cyl = endCylinder;
			setlg(p->start4, partitionOffset);
			setlg(p->size4, sizes[i]);
			partitionNbSectors = sizes[i];
			format();
			partitionOffset += partitionNbSectors;
		}
		partitionOffset = 0;
		partitionNbSectors = disk.getNbSectors();
		writeLogicalSector(0, buf);
	} else {
		assert(partitionOffset == 0);
		setBootSector(buf, sizes[0]);
		writeLogicalSector(0, buf); //allow usePartition to read the bootsector!
		usePartition(0);
		format();
	}
}

// temporary way to test import MSXtar functionality
string MSXtar::addDir(const string& rootDirName)
{
	return recurseDirFill(rootDirName, chrootSector);
}

// add file into fake dsk
string MSXtar::addFile(const string& filename)
{
	return addFileToDSK(filename, chrootSector);
}

//temporary way to test export MSXtar functionality
void MSXtar::getDir(const string& rootDirName)
{
	recurseDirExtract(rootDirName, chrootSector);
}

} // namespace openmsx
