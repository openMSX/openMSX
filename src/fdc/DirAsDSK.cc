// $Id$

#include "DirAsDSK.hh"
#include "CliComm.hh"
#include "BootBlocks.hh"
#include "File.hh"
#include "FileException.hh"
#include "FileOperations.hh"
#include "ReadDir.hh"
#include "StringOp.hh"
#include "statp.hh"
#include <algorithm>
#include <cassert>
#include <cstdio>
#include <cstring>
#include <limits>
#include <sys/types.h>

using std::string;

namespace openmsx {

static const unsigned NUM_FATS = 2;
static const unsigned SECTORS_PER_CLUSTER = 2;
static const unsigned FIRST_FAT_SECTOR = 1;
static const unsigned FIRST_DIR_SECTOR =
	FIRST_FAT_SECTOR + NUM_FATS * DirAsDSK::SECTORS_PER_FAT;
static const unsigned FIRST_DATA_SECTOR =
	FIRST_DIR_SECTOR + DirAsDSK::SECTORS_PER_DIR;

// first valid regular cluster number
static const unsigned FIRST_CLUSTER = 2;
// first cluster number that can NOT be used anymore
static const unsigned MAX_CLUSTER =
	(DirAsDSK::NUM_SECTORS - FIRST_DATA_SECTOR) / SECTORS_PER_CLUSTER + FIRST_CLUSTER;

// REDEFINE, see comment in DirAsDSK.hh
const unsigned DirAsDSK::SECTOR_SIZE;

static const unsigned FREE_FAT = 0x000;
static const unsigned BAD_FAT  = 0xFF7;
static const unsigned EOF_FAT  = 0xFFF; // actually 0xFF8-0xFFF


// Wrapper function to easily enable/disable debug prints
// Don't check-in this code with printing enabled:
//   printing stuff to stdout breaks stdio CliComm connections!
static void debug(const char* format, ...)
{
	(void)format;
#if 0
	va_list args;
	va_start(args, format);
	vprintf(format, args);
	va_end(args);
#endif
}


// transform BAD_FAT (0xFF7) and EOF_FAT-range (0xFF8-0xFFF)
// to a single value: EOF_FAT (0xFFF)
static unsigned normalizeFAT(unsigned cluster)
{
	return (cluster < BAD_FAT) ? cluster : EOF_FAT;
}

static unsigned readFATHelper(const byte* buf, unsigned cluster)
{
	assert(FIRST_CLUSTER <= cluster);
	assert(cluster < MAX_CLUSTER);
	const byte* p = buf + (cluster * 3) / 2;
	unsigned result = (cluster & 1)
	                ? (p[0] >> 4) + (p[1] << 4)
	                : p[0] + ((p[1] & 0x0F) << 8);
	return normalizeFAT(result);
}

static void writeFATHelper(byte* buf, unsigned cluster, unsigned val)
{
	assert(FIRST_CLUSTER <= cluster);
	assert(cluster < MAX_CLUSTER);
	byte* p = buf + (cluster * 3) / 2;
	if (cluster & 1) {
		p[0] = (p[0] & 0x0F) + (val << 4);
		p[1] = val >> 4;
	} else {
		p[0] = val;
		p[1] = (p[1] & 0xF0) + ((val >> 8) & 0x0F);
	}
}

// Read entry from FAT
unsigned DirAsDSK::readFAT(unsigned cluster)
{
	return readFATHelper(fat, cluster);
}

// Read entry from 2nd FAT
// 2nd FAT is _only_ used internally in DirAsDSK to detect updates in the FAT,
// if the emulated MSX reads a sector from the 2nd FAT, it actually gets a
// sector from the 1st FAT (in other words, from the MSX point of view, both
// FATs always contain identical values).
unsigned DirAsDSK::readFAT2(unsigned cluster)
{
	return readFATHelper(fat2, cluster);
}

// Write an entry to both FAT1 and FAT2
void DirAsDSK::writeFAT12(unsigned cluster, unsigned val)
{
	writeFATHelper(fat,  cluster, val);
	writeFATHelper(fat2, cluster, val);
}

// Write an entry to FAT2 (see note at readFAT2())
void DirAsDSK::writeFAT2(unsigned cluster, unsigned val)
{
	writeFATHelper(fat2, cluster, val);
}

// Returns MAX_CLUSTER in case of no more free clusters
unsigned DirAsDSK::findNextFreeCluster(unsigned curCl)
{
	assert(curCl < MAX_CLUSTER);
	do {
		++curCl;
		assert(curCl >= FIRST_CLUSTER);
	} while ((curCl < MAX_CLUSTER) && (readFAT(curCl) != FREE_FAT));
	return curCl;
}
unsigned DirAsDSK::findFirstFreeCluster()
{
	return findNextFreeCluster(FIRST_CLUSTER - 1);
}

static unsigned clusterToSector(unsigned cluster)
{
	assert(cluster >= FIRST_CLUSTER);
	assert(cluster < MAX_CLUSTER);
	return FIRST_DATA_SECTOR + SECTORS_PER_CLUSTER *
	            (cluster - FIRST_CLUSTER);
}

// Check if a msx filename is used in the emulated MSX disk
bool DirAsDSK::checkMSXFileExists(const string& msxFilename)
{
	for (unsigned i = 0; i < NUM_DIR_ENTRIES; ++i) {
		if (memcmp(mapDir[i].msxInfo.filename,
			   msxFilename.data(), 8 + 3) == 0) {
			return true;
		}
	}
	return false;
}

// Check if a host file is already mapped in the virtual disk
bool DirAsDSK::checkFileUsedInDSK(const string& hostName)
{
	for (unsigned i = 0; i < NUM_DIR_ENTRIES; ++i) {
		if (mapDir[i].hostName == hostName) {
			return true;
		}
	}
	return false;
}

// Create an MSX filename 8.3 format. TODO use vfat-like abbreviation
static char toMSXChr(char a)
{
	return (a == ' ') ? '_' : ::toupper(a);
}
static string hostToMsxName(string hostName)
{
	transform(hostName.begin(), hostName.end(), hostName.begin(), toMSXChr);

	string_ref file, ext;
	StringOp::splitOnLast(hostName, '.', file, ext);
	if (file.empty()) std::swap(file, ext);

	string result(8 + 3, ' ');
	memcpy(&*result.begin() + 0, file.data(), std::min(8u, file.size()));
	memcpy(&*result.begin() + 8, ext .data(), std::min(3u, ext .size()));
	return result;
}

static string msxToHostName(const char* msxName)
{
	string result;
	for (unsigned i = 0; (i < 8) && (msxName[i] != ' '); ++i) {
		result += tolower(msxName[i]);
	}
	if (msxName[8] != ' ') {
		result += '.';
		for (unsigned i = 8; (i < (8 + 3)) && (msxName[i] != ' '); ++i) {
			result += tolower(msxName[i]);
		}
	}
	return result;
}


void DirAsDSK::scanHostDir(bool onlyNewFiles)
{
	debug("Scanning HostDir for new files\n");
	// read directory and fill the fake disk
	ReadDir dir(hostDir);
	while (struct dirent* d = dir.getEntry()) {
		string name(d->d_name);
		// check if file is added to diskimage
		if (!onlyNewFiles || !checkFileUsedInDSK(name)) {
			debug("found new file %s\n", d->d_name);
			updateFileInDisk(name);
		}
	}
}

void DirAsDSK::cleandisk()
{
	// assign empty directory entries
	for (unsigned i = 0; i < NUM_DIR_ENTRIES; ++i) {
		memset(&mapDir[i].msxInfo, 0, sizeof(MSXDirEntry));
		mapDir[i].hostName.clear();
		mapDir[i].filesize = 0;
	}

	// Clear FAT1 + FAT2
	memset(fat,  0, sizeof(fat ));
	memset(fat2, 0, sizeof(fat2));
	// First 3 bytes are initialized specially:
	//  'cluster 0' contains the media descriptor (0xF9 for us)
	//  'cluster 1' is marked as EOF_FAT
	//  So cluster 2 is the first usable cluster number
	fat [0] = 0xF9; fat [1] = 0xFF; fat [2] = 0xFF;
	fat2[0] = 0xF9; fat2[1] = 0xFF; fat2[2] = 0xFF;

	// clear the sectorMap so that they all point to 'clean' sectors
	for (unsigned i = 0; i < NUM_SECTORS; ++i) {
		sectorMap[i].usage = CLEAN;
		sectorMap[i].dirIndex = 0;
		sectorMap[i].fileOffset = 0;
	}
}

DirAsDSK::DirAsDSK(CliComm& cliComm_, const Filename& hostDir_,
		SyncMode syncMode_, BootSectorType bootSectorType)
	: SectorBasedDisk(hostDir_)
	, cliComm(cliComm_)
	, hostDir(hostDir_.getResolved() + '/')
	, syncMode(syncMode_)
{
	if (!FileOperations::isDirectory(hostDir)) {
		throw MSXException("Not a directory");
	}

	// First create structure for the virtual disk
	setNbSectors(NUM_SECTORS); // asume a DS disk is used
	setSectorsPerTrack(9);
	setNbSides(2);

	// use selected bootsector
	const byte* bootSector =
		  bootSectorType == BOOTSECTOR_DOS1
		? BootBlocks::dos1BootBlock
		: BootBlocks::dos2BootBlock;
	memcpy(&bootBlock, bootSector, sizeof(bootBlock));

	// make a clean initial disk
	cleandisk();
	// Import the host filesystem.
	scanHostDir(false);
}

bool DirAsDSK::isWriteProtectedImpl() const
{
	return syncMode == SYNC_READONLY;
}

void DirAsDSK::readSectorImpl(unsigned sector, byte* buf)
{
	assert(sector < NUM_SECTORS);

	// This method behaves different in 'peek-mode' (see
	// SectorAccessibleDisk.hh) In peek-mode we don't sync with the host
	// filesystem. Instead all reads come directly from the in-memory
	// caches. Peek-mode is (only) used to read the (whole) disk to
	// calculate a sha1sum for the periodic reverse snapshots. This is far
	// from ideal: ideally we want each snapshot to have an up-to-date
	// sha1sum for the dir-as-disk image. But this mode was required to
	// work around a far worse problem: disk-corruption. It happens for
	// example in the following scenario:
	// - Save a file to dir-as-disk using a Philips-NMS-8250 (disk rom
	//   matters). Also the timing matters, I could easily trigger the
	//   problem while saving a screen 5 image in Paint4, but not when
	//   saving a similar file in MSX-BASIC. Now the following sectors are
	//   read/written:
	// - First the directory entry is created with the correct filename,
	//   but filesize and start-cluster are both still set to zero.
	// - Data sectors are written.
	// - Every second the writes are interrupted with a read of the whole
	//   disk (sectors 0-1439) to calculate the sha1sum. At this point this
	//   doesn't cause problems (yet).
	// - Now the directory entry is written with the correct filesize and
	//   cluster number. But at this point the FAT is not yet updated.
	// - Often at this point the whole disk is read again and this leads
	//   to disk corruption: Because the host filesize and the filesize
	//   in the directory entry don't match both files will be synced, but
	//   because the FAT is not yet in a consistent state, this sync will
	//   go wrong (in the end the filesize in the directory entry will be
	//   set to the wrong value).
	// - Last the MSX writes the FAT sectors, but the earlier host sync
	//   already screwed up the filesize in the directory entry.
	debug("DirAsDSK::readSectorImpl: %i ", sector);
	switch (sector) {
		case 0: debug("boot sector\n");
			break;
		case 1:
		case 2:
		case 3:
			debug("FAT 1 sector %i\n", (sector - 0));
			break;
		case 4:
		case 5:
		case 6:
			debug("FAT 2 sector %i\n", (sector - 3));
			break;
		case 7:
		case 8:
		case 9:
		case 10:
		case 11:
		case 12:
		case 13:
			debug("DIR sector %i\n", (sector - 6));
			break;
		default:
			debug("data sector\n");
			break;
	}

	if (sector == 0) {
		// copy our fake bootsector into the buffer
		memcpy(buf, &bootBlock, SECTOR_SIZE);

	} else if (sector < FIRST_DIR_SECTOR) {
		// copy correct sector from FAT

		// quick-and-dirty:
		// we check all files in the faked disk for altered filesize
		// remapping each fat entry to its direntry and do some bookkeeping
		// to avoid multiple checks will probably be slower than this
		if (!isPeekMode()) {
			for (unsigned i = 0; i < NUM_DIR_ENTRIES; ++i) {
				checkAlterFileInDisk(i);
			}
		}

		unsigned fatSector = (sector - FIRST_FAT_SECTOR) % SECTORS_PER_FAT;
		memcpy(buf, &fat[fatSector * SECTOR_SIZE], SECTOR_SIZE);

	} else if (sector < FIRST_DATA_SECTOR) {
		// create correct DIR sector
		sector -= FIRST_DIR_SECTOR;
		unsigned dirCount = sector * DIR_ENTRIES_PER_SECTOR;
		// check if there are new files on the host when we read the
		// first directory sector
		if (!isPeekMode() && dirCount == 0) {
			scanHostDir(true);
		}
		for (unsigned i = 0; i < DIR_ENTRIES_PER_SECTOR; ++i, ++dirCount) {
			if (!isPeekMode()) checkAlterFileInDisk(dirCount);
			memcpy(&buf[sizeof(MSXDirEntry) * i],
			       &(mapDir[dirCount].msxInfo),
			       sizeof(MSXDirEntry));
		}

	} else {
		// else get map from sector to file and read correct block
		// folowing same numbering as FAT eg. first data block is cluster 2
		if (sectorMap[sector].usage == CLEAN) {
			// return an 'empty' sector
			// 0xE5 is the value used on the Philips VG8250
			memset(buf, 0xE5, SECTOR_SIZE);
		} else if (sectorMap[sector].usage == CACHED) {
			memcpy(buf, cachedSectors[sector].data, SECTOR_SIZE);
		} else {
			// first copy cached data
			// in case (end of) file only fills partial sector
			memcpy(buf, cachedSectors[sector].data, SECTOR_SIZE);
			if (!isPeekMode()) {
				// read data from host file
				unsigned offset = sectorMap[sector].fileOffset;
				string hostName = mapDir[sectorMap[sector].dirIndex].hostName;
				checkAlterFileInDisk(hostName);
				// now try to read from file if possible
				try {
					string fullHostName = hostDir + hostName;
					File file(fullHostName);
					unsigned size = file.getSize();
					file.seek(offset);
					if (offset < size) {
						file.read(buf, std::min(size - offset, SECTOR_SIZE));
					} else {
						// Normally shouldn't happen because
						// checkAlterFileInDisk() above synced
						// host file size with MSX file size.
					}
					// and store the newly read data again in the sector cache
					// since checkAlterFileInDisk => updateFileInDisk only reads
					// altered data if filesize has been changed and not if only
					// content in file has changed
					memcpy(cachedSectors[sector].data, buf, SECTOR_SIZE);
				} catch (FileException&) {
					// couldn't open/read cached sector file
				}
			}
		}
	}
}

void DirAsDSK::checkAlterFileInDisk(const string& hostName)
{
	for (unsigned i = 0; i < NUM_DIR_ENTRIES; ++i) {
		if (mapDir[i].hostName == hostName) {
			checkAlterFileInDisk(i);
		}
	}
}

void DirAsDSK::checkAlterFileInDisk(unsigned dirIndex)
{
	if (!mapDir[dirIndex].inUse()) return;

	string fullHostName = hostDir + mapDir[dirIndex].hostName;
	struct stat fst;
	if (stat(fullHostName.c_str(), &fst) == 0) {
		if (mapDir[dirIndex].filesize != fst.st_size) {
			// changed filesize
			updateFileInDisk(dirIndex, fst);
		}
	} else {
		// file can not be stat'ed => assume it has been deleted
		// and thus delete it from the MSX DIR sectors by marking
		// the first filename char as 0xE5
		debug(" host os file deleted ? %s\n", mapDir[dirIndex].hostName.c_str());
		mapDir[dirIndex].msxInfo.filename[0] = char(0xE5);
		mapDir[dirIndex].hostName.clear();
		// Since we do not clear the FAT (a real MSX doesn't either)
		// and all data is cached in memmory you now can use MSX-DOS
		// undelete tools to restore the file on your host-OS, using
		// the 8.3 msx name of course :-)
		//
		// TODO: It might be a good idea to mark all the sectors as
		// CACHED instead of MIXED since the original file is gone...
	}
}

void DirAsDSK::updateFileInDisk(unsigned dirIndex, struct stat& fst)
{
	// compute time/date stamps
	struct tm* mtim = localtime(&(fst.st_mtime));
	int t1 = mtim ? (mtim->tm_sec >> 1) + (mtim->tm_min << 5) +
	                (mtim->tm_hour << 11)
	              : 0;
	mapDir[dirIndex].msxInfo.time = t1;
	int t2 = mtim ? mtim->tm_mday + ((mtim->tm_mon + 1) << 5) +
	                ((mtim->tm_year + 1900 - 1980) << 9)
	              : 0;
	mapDir[dirIndex].msxInfo.date = t2;

	unsigned hostSize = fst.st_size;
	mapDir[dirIndex].filesize = hostSize;

	bool moreClustersInChain = true;
	unsigned curCl = mapDir[dirIndex].msxInfo.startCluster;
	// If there is no cluster assigned yet to this file (curCl == 0), then
	// find a free cluster. Treat invalid cases in the same way (curCl == 1
	// or curCl >= MAX_CLUSTER).
	if ((curCl < FIRST_CLUSTER) || (curCl >= MAX_CLUSTER)) {
		moreClustersInChain = false;
		curCl = findFirstFreeCluster(); // MAX_CLUSTER in case of disk-full
	}

	unsigned remainingSize = hostSize;
	unsigned prevCl = 0;
	try {
		string fullHostName = hostDir + mapDir[dirIndex].hostName;
		File file(fullHostName, "rb"); // don't uncompress

		while (remainingSize && (curCl < MAX_CLUSTER)) {
			unsigned logicalSector = clusterToSector(curCl);
			for (unsigned i = 0; i < SECTORS_PER_CLUSTER; ++i) {
				unsigned sector = logicalSector + i;
				assert(sector < NUM_SECTORS);
				sectorMap[sector].usage = MIXED;
				sectorMap[sector].dirIndex = dirIndex;
				sectorMap[sector].fileOffset = hostSize - remainingSize;
				byte* buf = cachedSectors[sector].data;
				memset(buf, 0, SECTOR_SIZE); // in case (end of) file only fills partial sector
				file.read(buf, std::min(remainingSize, SECTOR_SIZE));
				remainingSize -= std::min(remainingSize, SECTOR_SIZE);
				if (remainingSize == 0) {
					// don't fill next sectors in this cluster
					// if there is no data left
					break;
				}
			}

			if (prevCl) {
				writeFAT12(prevCl, curCl);
			} else {
				mapDir[dirIndex].msxInfo.startCluster = curCl;
			}
			prevCl = curCl;

			// Check if we can follow the existing FAT chain or
			// need to allocate a free cluster.
			if (moreClustersInChain) {
				curCl = readFAT(curCl);
				if ((curCl == EOF_FAT)      || // normal end
				    (curCl < FIRST_CLUSTER) || // invalid
				    (curCl >= MAX_CLUSTER)) {  // invalid
					// Treat invalid FAT chain the same as
					// a normal EOF_FAT.
					moreClustersInChain = false;
					curCl = findFirstFreeCluster();
				}
			} else {
				curCl = findNextFreeCluster(curCl);
			}
		}
		if (remainingSize != 0) {
			cliComm.printWarning("Virtual diskimage full: " +
			                     mapDir[dirIndex].hostName + " truncated.");
		}
	} catch (FileException& e) {
		// Error opening or reading host file
		cliComm.printWarning("Error reading host file: " +
				mapDir[dirIndex].hostName +
				": " + e.getMessage() +
				" Truncated file on MSX disk.");
	}

	// In all cases (no error / image full / host read error) we need to
	// properly terminate the FAT chain.
	if (prevCl) {
		writeFAT12(prevCl, EOF_FAT);
	} else {
		// Filesize zero: don't allocate any cluster, write zero
		// cluster number (checked on a MSXTurboR, DOS2 mode).
		mapDir[dirIndex].msxInfo.startCluster = FREE_FAT;
	}

	// clear remains of FAT if needed
	if (moreClustersInChain) {
		while ((FIRST_CLUSTER <= curCl) && (curCl < MAX_CLUSTER)) {
			writeFAT12(curCl, FREE_FAT);
			unsigned logicalSector = clusterToSector(curCl);
			for (unsigned i = 0; i < SECTORS_PER_CLUSTER; ++i) {
				unsigned sector = logicalSector + i;
				assert(sector < NUM_SECTORS);
				sectorMap[sector].usage = CLEAN;
				sectorMap[sector].dirIndex = 0;
				sectorMap[sector].fileOffset = 0;
			}
			prevCl = curCl;
			curCl = readFAT(curCl);
		}
		writeFAT12(prevCl, FREE_FAT);
		unsigned logicalSector = clusterToSector(prevCl);
		for (unsigned i = 0; i < SECTORS_PER_CLUSTER; ++i) {
			unsigned sector = logicalSector + i;
			assert(sector < NUM_SECTORS);
			sectorMap[sector].usage = CLEAN;
			sectorMap[sector].dirIndex = 0;
			sectorMap[sector].fileOffset = 0;
		}
	}

	// write (possibly truncated) file size
	mapDir[dirIndex].msxInfo.size = hostSize - remainingSize;

	// TODO in case of an error (disk image full, or host file read error),
	// wouldn't it be better to remove the (half imported) msx file again?
	// Sometimes when I'm using DirAsDSK I have one file that is too big
	// (e.g. a core file, or a vim .swp file) and that one prevents
	// DirAsDSK from importing the other (small) files in my directory.
}

void DirAsDSK::truncateCorrespondingFile(unsigned dirIndex)
{
	if (!mapDir[dirIndex].inUse()) {
		// Host file does not yet exist, create filename from msx name.
		const char* msxName = mapDir[dirIndex].msxInfo.filename;
		// If the msx file is deleted, we don't need to do anything.
		if (msxName[0] == char(0xE5)) return;
		string hostName = msxToHostName(msxName);
		mapDir[dirIndex].hostName = hostName;
		debug("      truncateCorrespondingFile of new Host OS file\n");
	}
	debug("      truncateCorrespondingFile %s\n", mapDir[dirIndex].hostName.c_str());
	unsigned msxSize = mapDir[dirIndex].msxInfo.size;
	mapDir[dirIndex].filesize = msxSize;

	// stuff below can fail, so do it as the last thing in this method
	try {
		string fullHostName = hostDir + mapDir[dirIndex].hostName;
		File file(fullHostName, File::CREATE);
		file.truncate(msxSize);
	} catch (FileException& e) {
		cliComm.printWarning("Error while truncating host file: " +
			mapDir[dirIndex].hostName + ": " + e.getMessage());
	}
}

void DirAsDSK::extractCacheToFile(unsigned dirIndex)
{
	if (!mapDir[dirIndex].inUse()) {
		// Host file does not yet exist, create filename from msx name
		const char* msxName = mapDir[dirIndex].msxInfo.filename;
		// If the msx file is deleted, we don't need to do anything
		if (msxName[0] == char(0xE5)) return;
		string hostName = msxToHostName(msxName);
		mapDir[dirIndex].hostName = hostName;
	}
	// We write a host file with length that is the minimum of:
	//  - Length indicated in msx directory entry.
	//  - Length of FAT-chain * cluster-size, this chain can have length=0
	//    if startCluster is not (yet) filled in.
	try {
		unsigned curCl = mapDir[dirIndex].msxInfo.startCluster;
		unsigned msxSize = mapDir[dirIndex].msxInfo.size;

		string fullHostName = hostDir + mapDir[dirIndex].hostName;
		File file(fullHostName, File::TRUNCATE);
		unsigned offset = 0;
		while ((FIRST_CLUSTER <= curCl) && (curCl < MAX_CLUSTER)) {
			unsigned logicalSector = clusterToSector(curCl);
			for (unsigned i = 0; i < SECTORS_PER_CLUSTER; ++i) {
				unsigned sector = logicalSector + i;
				assert(sector < NUM_SECTORS);
				if ((sectorMap[sector].usage == CACHED ||
				     sectorMap[sector].usage == MIXED) &&
				    (msxSize > offset)) {
					byte* buf = cachedSectors[sector].data;
					unsigned writeSize = std::min(msxSize - offset, SECTOR_SIZE);
					file.write(buf, writeSize);

					sectorMap[sector].usage = MIXED;
					sectorMap[sector].dirIndex = dirIndex;
					sectorMap[sector].fileOffset = offset;
				}
				offset += SECTOR_SIZE;
			}
			curCl = readFAT(curCl);
		}
	} catch (FileException& e) {
		cliComm.printWarning("Error while syncing host file: " +
			mapDir[dirIndex].hostName + ": " + e.getMessage());
	}
}


void DirAsDSK::writeSectorImpl(unsigned sector, const byte* buf)
{
	assert(sector < NUM_SECTORS);
	assert(syncMode != SYNC_READONLY);

	debug("DirAsDSK::writeSectorImpl: %i ", sector);
	switch (sector) {
		case 0: debug("boot sector\n");
			break;
		case 1:
		case 2:
		case 3:
			debug("FAT 1 sector %i\n", (sector - 0));
			break;
		case 4:
		case 5:
		case 6:
			debug("FAT 2 sector %i\n", (sector - 3));
			break;
		case 7:
		case 8:
		case 9:
		case 10:
		case 11:
		case 12:
		case 13:
			debug("DIR sector %i\n", (sector - 6));
			break;
		default:
			debug("data sector\n");
			break;
	}
	if (sector >= FIRST_DATA_SECTOR) {
		assert(sector < NUM_SECTORS);
		debug("  Mode: ");
		switch (sectorMap[sector].usage) {
		case CLEAN:
			debug("CLEAN\n");
			break;
		case CACHED:
			debug("CACHED\n");
			break;
		case MIXED:
			debug("MIXED ");
			debug("  direntry : %i \n", sectorMap[sector].dirIndex);
			debug("    => %s \n", mapDir[sectorMap[sector].dirIndex].hostName.c_str());
			debug("  fileOffset : %li\n", sectorMap[sector].fileOffset);
			break;
		}
	}

	if (sector == 0) {
		memcpy(&bootBlock, buf, SECTOR_SIZE);
	} else if (sector < FIRST_DIR_SECTOR) {
		writeFATSector(sector, buf);
	} else if (sector < FIRST_DATA_SECTOR) {
		writeDIRSector(sector, buf);
	} else {
		writeDataSector(sector, buf);
	}
}

void DirAsDSK::writeFATSector(unsigned sector, const byte* buf)
{
	// During formatting sectors > 1+SECTORS_PER_FAT are empty (all
	// bytes are 0) so we would erase the 3 bytes indentifier at the
	// beginning of the FAT !!
	// Since the two FATs should be identical after "normal" usage,
	// we use writes to the second FAT (which should be an exact backup
	// of the first FAT to detect changes (files might have
	// grown/shrunk/added) so that they can be passed on to the HOST-OS
	static const unsigned FIRST_SECTOR_2ND_FAT =
	       FIRST_FAT_SECTOR + SECTORS_PER_FAT;
	if (sector < FIRST_SECTOR_2ND_FAT) {
		unsigned fatSector = sector - FIRST_FAT_SECTOR;
		memcpy(&fat[fatSector * SECTOR_SIZE], buf, SECTOR_SIZE);
		return;
	}

	// writes to the second FAT so we check for changes
	// but we fully ignore the sectors afterwards (see remark
	// about identifier bytes above)
	static const unsigned MAX_FAT_ENTRIES_PER_SECTOR =
		(SECTOR_SIZE * 2 + (3 - 1)) / 3; // rounded up
	unsigned fatSector = sector - FIRST_SECTOR_2ND_FAT;
	unsigned startCluster = std::max(FIRST_CLUSTER,
	                                 ((fatSector * SECTOR_SIZE) * 2) / 3);
	unsigned endCluster = std::min(startCluster + MAX_FAT_ENTRIES_PER_SECTOR,
	                               MAX_CLUSTER);
	for (unsigned i = startCluster; i < endCluster; ++i) {
		if (readFAT(i) != readFAT2(i)) {
			updateFileFromAlteredFatOnly(i);
		}
	}

	memcpy(&fat2[fatSector * SECTOR_SIZE], buf, SECTOR_SIZE);
}

void DirAsDSK::writeDIRSector(unsigned sector, const byte* buf)
{
	// We assume that the dir entry is updated last, so the fat and actual
	// sector data should already contain the correct data. Most MSX disk
	// roms honour this behaviour for normal fileactions. Of course some
	// diskcaching programs and disk optimizers can abandon this behaviour
	// and in such case the logic used here goes haywire!!
	unsigned dirIndex = (sector - FIRST_DIR_SECTOR) * DIR_ENTRIES_PER_SECTOR;
	for (unsigned i = 0; i < DIR_ENTRIES_PER_SECTOR; ++i, ++dirIndex) {
		const MSXDirEntry& newEntry = *reinterpret_cast<const MSXDirEntry*>(
			&buf[sizeof(MSXDirEntry) * i]);
		if (memcmp(mapDir[dirIndex].msxInfo.filename, &newEntry, sizeof(newEntry)) != 0) {
			writeDIREntry(dirIndex, newEntry);
		}
	}
}

void DirAsDSK::writeDIREntry(unsigned dirIndex, const MSXDirEntry& newEntry)
{
	unsigned oldClus = mapDir[dirIndex].msxInfo.startCluster;
	unsigned newClus = newEntry.startCluster;
	unsigned oldSize = mapDir[dirIndex].msxInfo.size;
	unsigned newSize = newEntry.size;

	// The 3 vital informations needed
	bool chgName = memcmp(mapDir[dirIndex].msxInfo.filename, newEntry.filename, 8 + 3) != 0;
	bool chgClus = oldClus != newClus;
	bool chgSize = oldSize != newSize;

	// Here are the possible combinations encountered in normal usage so far,
	// the bool order is chgName chgClus chgSize
	// 0 0 0 : nothing changed for this direntry... :-)
	// 0 0 1 : File has grown or shrunk
	// 0 1 1 : name remains but size and cluster changed
	//         => second step in new file creation (checked on NMS8250)
	// 1 0 0 : a) Only create a name and size+cluster still zero
	//          => first step in new file creation (cheked on NMS8250)
	//             if we start a new file the currentcluster can be set to zero
	//             FI: on a Philips NMS8250 in Basic try : copy"con"to:"a.txt"
	//             it will first write the first 7 sectors, then the data, then
	//             update the 7 first sectors with correct data
	//         b) name changed, others remained unchanged
	//          => file renamed
	//         c) first char of name changed to 0xE5, others remained unchanged
	//          => file deleted
	// 1 1 1 : a new file has been created (as done by a Panasonic FS A1GT)
	debug("  dirIndex %i filename: %s\n",
	      dirIndex, mapDir[dirIndex].hostName.c_str());
	debug("  chgName: %i chgClus: %i chgSize: %i\n", chgName, chgClus, chgSize);
	debug("  Old start %i   New start %i\n",   oldClus, newClus);
	debug("  Old size  %i   New size  %i\n\n", oldSize, newSize);

	if (chgName && !chgClus && !chgSize) {
		if (newEntry.filename[0] == char(0xE5) && syncMode == SYNC_FULL) {
			// dir entry has been deleted
			// delete file from host OS and 'clear' all sector
			// data pointing to this HOST OS file
			string fullHostName = hostDir + mapDir[dirIndex].hostName;
			FileOperations::unlink(fullHostName);
			for (unsigned i = FIRST_DATA_SECTOR; i < NUM_SECTORS; ++i) {
				if (sectorMap[i].dirIndex == dirIndex) {
					 sectorMap[i].usage = CACHED;
				}
			}
			mapDir[dirIndex].hostName.clear();

		} else if ((newEntry.filename[0] != char(0xE5)) &&
			   (syncMode == SYNC_FULL || syncMode == SYNC_NODELETE)) {
			string hostName = msxToHostName(newEntry.filename);
			string newFilename = hostDir + hostName;
			if (newClus == 0 && newSize == 0) {
				// creating a new file
				mapDir[dirIndex].hostName = hostName;
				// we do not need to write anything since the MSX
				// will update this later when the size is altered
				try {
					File file(newFilename, File::TRUNCATE);
				} catch (FileException& e) {
					cliComm.printWarning("Couldn't create new file: " + e.getMessage());
				}
			} else {
				// rename file on host OS
				string oldFilename = hostDir + mapDir[dirIndex].hostName;
				if (rename(oldFilename.c_str(), newFilename.c_str()) == 0) {
					// renaming on host OS succeeeded
					mapDir[dirIndex].hostName = hostName;
				}
			}
		} else {
			cliComm.printWarning(
				"File has been renamed in emulated disk. Host OS file (" +
				mapDir[dirIndex].hostName + ") remains untouched!");
		}
	}

	if (chgSize) {
		// content changed, extract the file
		// Cluster might have changed is this is a new file so chgClus
		// is ignored. Also name might have been changed (on a turbo R
		// the single shot Dir update when creating new files)
		if (oldSize < newSize) {
			// new size is bigger, file has grown
			memcpy(&(mapDir[dirIndex].msxInfo), &newEntry, sizeof(newEntry));
			extractCacheToFile(dirIndex);
		} else {
			// New size is smaller, file has been reduced. Luckily
			// the entire file is in cache, we need this since on
			// some MSX models during a copy from file to overwrite
			// an existing file the sequence is that first the
			// actual data is written and then the size is set to
			// zero before it is set to the new value. If we didn't
			// cache this, then all the 'mapped' sectors would lose
			// their value.
			memcpy(&(mapDir[dirIndex].msxInfo), &newEntry, sizeof(newEntry));
			truncateCorrespondingFile(dirIndex);
			if (newSize != 0) {
				extractCacheToFile(dirIndex); // see copy remark above
			}
		}
	}

	if (!chgName && chgClus && !chgSize) {
		cliComm.printWarning(
			"This case of writing to DIR is not yet implemented "
			"since we haven't encountered it in real life yet.");
	}

	// for now blindly take over info
	memcpy(&(mapDir[dirIndex].msxInfo), &newEntry, sizeof(newEntry));
}

void DirAsDSK::writeDataSector(unsigned sector, const byte* buf)
{
	assert(sector >= FIRST_DATA_SECTOR);
	assert(sector < NUM_SECTORS);

	// first and before all else buffer everything !!!!
	// check if cachedSectors exists, if not assign memory.
	memcpy(cachedSectors[sector].data, buf, SECTOR_SIZE);

	// if in SYNC_CACHEDWRITE then simply mark sector as cached and be done with it
	if (syncMode == SYNC_CACHEDWRITE) {
		// change to a regular cached sector
		sectorMap[sector].usage = CACHED;
		sectorMap[sector].dirIndex = 0;
		sectorMap[sector].fileOffset = 0;
		return;
	}

	if (sectorMap[sector].usage == MIXED) {
		// save data to host file
		unsigned offset = sectorMap[sector].fileOffset;
		unsigned dirent = sectorMap[sector].dirIndex;
		string fullHostName = hostDir + mapDir[dirent].hostName;
		try {
			File file(fullHostName);
			file.seek(offset);
			unsigned msxSize = mapDir[dirent].msxInfo.size;
			if (msxSize > offset) {
				unsigned writeSize = std::min(msxSize - offset, SECTOR_SIZE);
				file.write(buf, writeSize);
			}
		} catch (FileException& e) {
			cliComm.printWarning("Couldn't write to file " + fullHostName + ": " + e.getMessage());
		}
	} else {
		// indicate data is cached, it might be CACHED already or it was CLEAN
		sectorMap[sector].usage = CACHED;
	}
}

void DirAsDSK::updateFileFromAlteredFatOnly(unsigned cluster)
{
	// Search for the first cluster in the chain that contains 'cluster'
	// Note: worst case (this implementation of) the search is O(N^2), but
	// because usually FAT chains are allocated in ascending order, this
	// search is fast O(N).
	unsigned startCluster = cluster;
	for (unsigned i = FIRST_CLUSTER; i < MAX_CLUSTER; ++i) {
		if (readFAT(i) == startCluster) {
			// found a predecessor
			startCluster = i;
			i = FIRST_CLUSTER - 1; // restart search
		}
	}

	// Find the corresponding direntry and (if found) export file based on
	// new cluster chain.
	for (unsigned i = 0; i < NUM_DIR_ENTRIES; ++i) {
		if (startCluster == mapDir[i].msxInfo.startCluster) {
			extractCacheToFile(i);
			break;
		}
	}

	// from 'startCluster' and 'cluster' on, update fat2 so that the check
	// in writeFATSector() doesn't call this routine again for the same file
	unsigned curCl = startCluster;
	while ((FIRST_CLUSTER <= curCl) && (curCl < MAX_CLUSTER)) {
		unsigned next = readFAT(curCl);
		writeFAT2(curCl, next);
		curCl = next;
	}

	// since the new FAT chain can be shorter (file size shrunk)
	// we also start from 'cluster', in such case
	// the loop above doesn't take care of this, since it will
	// stop at the new EOF_FAT || curCl == 0 condition
	curCl = cluster;
	while ((FIRST_CLUSTER <= curCl) && (curCl < MAX_CLUSTER)) {
		unsigned next = readFAT(curCl);
		writeFAT2(curCl, next);
		curCl = next;
	}
}


void DirAsDSK::updateFileInDisk(const string& hostName)
{
	string fullHostName = hostDir + hostName;
	struct stat fst;
	if (stat(fullHostName.c_str(), &fst)) {
		cliComm.printWarning("Error accessing " + fullHostName);
		return;
	}
	if (!S_ISREG(fst.st_mode)) {
		// we only handle regular files for now
		if (hostName != "." && hostName != "..") {
			// don't warn for these files, as they occur in any directory
			cliComm.printWarning("Not a regular file: " + fullHostName);
		}
		return;
	}
	if (fst.st_size >= std::numeric_limits<int>::max()) {
		// File sizes are processed using int, so prevent integer
		// overflow. Files this large won't be not be supported
		// by an MSX anyway
		cliComm.printWarning("File too large: " + fullHostName);
		return;
	}
	if (!checkFileUsedInDSK(hostName)) {
		// add file to fakedisk
		addFileToDSK(hostName, fst);
	} else {
		// really update file
		checkAlterFileInDisk(hostName);
	}
}

void DirAsDSK::addFileToDSK(const string& hostName, struct stat& fst)
{
	// Get empty dir entry
	unsigned dirIndex = 0;
	while (mapDir[dirIndex].inUse()) {
		if (++dirIndex == NUM_DIR_ENTRIES) {
			cliComm.printWarning(
				"Couldn't add " + hostName + ": root dir full");
			return;
		}
	}

	// Create correct MSX filename
	string msxFilename = hostToMsxName(hostName);
	if (checkMSXFileExists(msxFilename)) {
		// TODO: actually should increase vfat abrev if possible!!
		cliComm.printWarning(
			"Couldn't add " + hostName + ": MSX name " +
			msxFilename + " existed already");
		return;
	}

	// Fill in filenames and import the file content.
	mapDir[dirIndex].hostName = hostName;
	memcpy(&(mapDir[dirIndex].msxInfo.filename), msxFilename.data(), 8 + 3);
	updateFileInDisk(dirIndex, fst);
}

} // namespace openmsx
