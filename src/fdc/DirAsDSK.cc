// $Id$

#include "DirAsDSK.hh"
#include "DiskChanger.hh"
#include "Scheduler.hh"
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
#include <cstring>
#include <limits>

using std::string;

namespace openmsx {

static const unsigned NUM_FATS = 2;
static const unsigned SECTORS_PER_CLUSTER = 2;
static const unsigned FIRST_FAT_SECTOR = 1;
static const unsigned FIRST_SECTOR_2ND_FAT =
	FIRST_FAT_SECTOR + DirAsDSK::SECTORS_PER_FAT;
static const unsigned FIRST_DIR_SECTOR =
	FIRST_FAT_SECTOR + NUM_FATS * DirAsDSK::SECTORS_PER_FAT;
static const unsigned FIRST_DATA_SECTOR =
	FIRST_DIR_SECTOR + DirAsDSK::SECTORS_PER_DIR;

// first valid regular cluster number
static const unsigned FIRST_CLUSTER = 2;
// first cluster number that can NOT be used anymore
static const unsigned MAX_CLUSTER =
	(DirAsDSK::NUM_SECTORS - FIRST_DATA_SECTOR) / SECTORS_PER_CLUSTER + FIRST_CLUSTER;

static const unsigned FREE_FAT = 0x000;
static const unsigned BAD_FAT  = 0xFF7;
static const unsigned EOF_FAT  = 0xFFF; // actually 0xFF8-0xFFF


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

byte* DirAsDSK::fat()
{
	return sectors[FIRST_FAT_SECTOR];
}
byte* DirAsDSK::fat2()
{
	return sectors[FIRST_SECTOR_2ND_FAT];
}

// Read entry from FAT
unsigned DirAsDSK::readFAT(unsigned cluster)
{
	return readFATHelper(fat(), cluster);
}

// Write an entry to both FAT1 and FAT2
void DirAsDSK::writeFAT12(unsigned cluster, unsigned val)
{
	writeFATHelper(fat (), cluster, val);
	writeFATHelper(fat2(), cluster, val);
	// An alternative is to copy FAT1 to FAT2 after changes have been made
	// to FAT1. This is probably more like what the real disk rom does.
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

static void sectorToCluster(unsigned sector, unsigned& cluster, unsigned& offset)
{
	assert(sector >= FIRST_DATA_SECTOR);
	assert(sector < DirAsDSK::NUM_SECTORS);
	sector -= FIRST_DATA_SECTOR;
	cluster = (sector / SECTORS_PER_CLUSTER) + FIRST_CLUSTER;
	offset  = (sector % SECTORS_PER_CLUSTER) * DirAsDSK::SECTOR_SIZE;
}

MSXDirEntry& DirAsDSK::msxDir(unsigned dirIndex)
{
	assert(dirIndex < NUM_DIR_ENTRIES);
	MSXDirEntry* dirs = reinterpret_cast<MSXDirEntry*>(
		sectors[FIRST_DIR_SECTOR]);
	return dirs[dirIndex];
}

// Check if a msx filename is used in the emulated MSX disk
bool DirAsDSK::checkMSXFileExists(const string& msxFilename)
{
	for (unsigned i = 0; i < NUM_DIR_ENTRIES; ++i) {
		if (memcmp(msxDir(i).filename,
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


DirAsDSK::DirAsDSK(DiskChanger& diskChanger_, CliComm& cliComm_,
                   const Filename& hostDir_, SyncMode syncMode_,
                   BootSectorType bootSectorType)
	: SectorBasedDisk(hostDir_)
	, diskChanger(diskChanger_)
	, cliComm(cliComm_)
	, hostDir(hostDir_.getResolved() + '/')
	, syncMode(syncMode_)
	, lastAccess(EmuTime::zero)
{
	if (!FileOperations::isDirectory(hostDir)) {
		throw MSXException("Not a directory");
	}

	// First create structure for the virtual disk
	setNbSectors(NUM_SECTORS); // asume a DS disk is used
	setSectorsPerTrack(9);
	setNbSides(2);

	// Initially the whole disk is filled with 0xE5 (at least on Philips
	// NMS8250).
	memset(sectors, 0xE5, sizeof(sectors));

	// Use selected bootsector
	const byte* bootSector = bootSectorType == BOOTSECTOR_DOS1
	                       ? BootBlocks::dos1BootBlock
	                       : BootBlocks::dos2BootBlock;
	memcpy(sectors[0], bootSector, SECTOR_SIZE);

	// Clear FAT1 + FAT2
	memset(fat(), 0, SECTOR_SIZE * SECTORS_PER_FAT * NUM_FATS);
	// First 3 bytes are initialized specially:
	//  'cluster 0' contains the media descriptor (0xF9 for us)
	//  'cluster 1' is marked as EOF_FAT
	//  So cluster 2 is the first usable cluster number
	fat ()[0] = 0xF9; fat ()[1] = 0xFF; fat ()[2] = 0xFF;
	fat2()[0] = 0xF9; fat2()[1] = 0xFF; fat2()[2] = 0xFF;

	// Assign empty directory entries, none of these entries is currently
	// associated with a host file.
	for (unsigned i = 0; i < NUM_DIR_ENTRIES; ++i) {
		memset(&msxDir(i), 0, sizeof(MSXDirEntry));
		mapDir[i].hostName.clear();
		mapDir[i].mtime = 0;
		mapDir[i].filesize = 0;
	}

	// Import the host filesystem.
	syncWithHost();
}

bool DirAsDSK::isWriteProtectedImpl() const
{
	return syncMode == SYNC_READONLY;
}

void DirAsDSK::checkCaches()
{
	bool needSync;
	if (Scheduler* scheduler = diskChanger.getScheduler()) {
		EmuTime now = scheduler->getCurrentTime();
		EmuDuration delta = now - lastAccess;
		needSync = delta > EmuDuration::sec(1);
		// Do not update lastAccess because we don't actually call
		// syncWithHost().
	} else {
		// happens when dirasdisk is used in virtual_drive
		needSync = true;
	}
	if (needSync) {
		flushCaches();
	}
}

void DirAsDSK::readSectorImpl(unsigned sector, byte* buf)
{
	assert(sector < NUM_SECTORS);

	// 'Peek-mode' is used to periodically calculate a sha1sum for the
	// whole disk (used by reverse). We don't want this calculation to
	// interfer with the access time we use for normal read/writes. So in
	// peek-mode we skip the whole sync-step.
	if (!isPeekMode()) {
		bool needSync;
		if (Scheduler* scheduler = diskChanger.getScheduler()) {
			EmuTime now = scheduler->getCurrentTime();
			EmuDuration delta = now - lastAccess;
			lastAccess = now;
			needSync = delta > EmuDuration::sec(1);
		} else {
			// happens when dirasdisk is used in virtual_drive
			needSync = true;
		}
		if (needSync) {
			syncWithHost();
			flushCaches(); // e.g. sha1sum
			// Let the diskdrive report the disk has been ejected.
			// E.g. a turbor machine uses this to flush its
			// internal disk caches.
			diskChanger.forceDiskChange();
		}
	}

	// Simply return the sector from our virtual disk image.
	memcpy(buf, sectors[sector], SECTOR_SIZE);
}

void DirAsDSK::syncWithHost()
{
	// Check for removed host files. This frees up space in the virtual
	// disk. Do this first because otherwise later actions may fail (run
	// out of virtual disk space) for no good reason.
	checkDeletedHostFiles();

	// Next update existing files. This may enlarge or shrink virtual
	// files. In case not all host files fit on the virtual disk it's
	// better to update the existing files than to (partly) add a too big
	// new file and have no space left to enlarge the existing files.
	for (unsigned i = 0; i < NUM_DIR_ENTRIES; ++i) {
		checkModifiedHostFile(i);
	}

	// Last add new host files (this can only consume virtual disk space).
	addNewHostFiles();
}

void DirAsDSK::checkDeletedHostFiles()
{
	for (unsigned dirIndex = 0; dirIndex < NUM_DIR_ENTRIES; ++dirIndex) {
		if (!mapDir[dirIndex].inUse()) continue;

		string fullHostName = hostDir + mapDir[dirIndex].hostName;
		struct stat fst;
		if (stat(fullHostName.c_str(), &fst) != 0) {
			// TODO also check access permission
			// error stat-ing file, assume it's been deleted
			deleteMSXFile(dirIndex);
		}
	}
}

void DirAsDSK::deleteMSXFile(unsigned dirIndex)
{
	// Delete it from the MSX DIR sectors by marking the first filename
	// char as 0xE5.
	msxDir(dirIndex).filename[0] = char(0xE5);
	mapDir[dirIndex].hostName.clear(); // no longer mapped to host file

	// Clear the FAT chain to free up space in the virtual disk.
	freeFATChain(msxDir(dirIndex).startCluster);
}

void DirAsDSK::freeFATChain(unsigned curCl)
{
	// Follow a FAT chain and mark all clusters on this chain as free
	while ((FIRST_CLUSTER <= curCl) && (curCl < MAX_CLUSTER)) {
		unsigned nextCl = readFAT(curCl);
		writeFAT12(curCl, FREE_FAT);
		curCl = nextCl;
	}
}

void DirAsDSK::checkModifiedHostFile(unsigned dirIndex)
{
	if (!mapDir[dirIndex].inUse()) return;

	string fullHostName = hostDir + mapDir[dirIndex].hostName;
	struct stat fst;
	if (stat(fullHostName.c_str(), &fst) == 0) {
		// Detect changes in host file.
		// Heuristic: we use filesize and modification time to detect
		// changes in file content.
		//  TODO do we need both filesize and mtime or is mtime alone
		//       enough?
		if ((mapDir[dirIndex].mtime    != fst.st_mtime) ||
		    (mapDir[dirIndex].filesize != fst.st_size)) {
			importHostFile(dirIndex, fst);
		}
	} else {
		// Only very rarely happens (because checkDeletedHostFiles()
		// checked this just recently).
		deleteMSXFile(dirIndex);
	}
}

void DirAsDSK::importHostFile(unsigned dirIndex, struct stat& fst)
{
	// compute time/date stamps
	struct tm* mtim = localtime(&(fst.st_mtime));
	int t1 = mtim ? (mtim->tm_sec >> 1) + (mtim->tm_min << 5) +
	                (mtim->tm_hour << 11)
	              : 0;
	msxDir(dirIndex).time = t1;
	int t2 = mtim ? mtim->tm_mday + ((mtim->tm_mon + 1) << 5) +
	                ((mtim->tm_year + 1900 - 1980) << 9)
	              : 0;
	msxDir(dirIndex).date = t2;

	// Set host modification time (and filesize)
	// Note: this is the _only_ place where we update the mapdir.mtime
	// field. We do _not_ update it when the msx writes to the file. So in
	// case the msx does write a data sector, it writes to the correct
	// offset in the host file, but mtime is not updated. On the next
	// host->emu sync we will resync the full file content and only then
	// update mtime. This may seem inefficient, but it makes sure that we
	// never miss any file changes performed by the host. E.g. the host
	// changed part of the file short before the msx wrote to the same
	// file. We can never guarantee that such race-scenarios work
	// correctly, but at least with this mtime-update convention the file
	// content will be the same on the host and the msx side after every
	// sync.
	unsigned hostSize = fst.st_size;
	mapDir[dirIndex].filesize = hostSize;
	mapDir[dirIndex].mtime = fst.st_mtime;

	bool moreClustersInChain = true;
	unsigned curCl = msxDir(dirIndex).startCluster;
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
				byte* buf = sectors[sector];
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
				msxDir(dirIndex).startCluster = curCl;
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
		msxDir(dirIndex).startCluster = FREE_FAT;
	}

	// clear remains of FAT if needed
	if (moreClustersInChain) {
		freeFATChain(curCl);
	}

	// write (possibly truncated) file size
	msxDir(dirIndex).size = hostSize - remainingSize;

	// TODO in case of an error (disk image full, or host file read error),
	// wouldn't it be better to remove the (half imported) msx file again?
	// Sometimes when I'm using DirAsDSK I have one file that is too big
	// (e.g. a core file, or a vim .swp file) and that one prevents
	// DirAsDSK from importing the other (small) files in my directory.
}

void DirAsDSK::addNewHostFiles()
{
	ReadDir dir(hostDir);
	while (struct dirent* d = dir.getEntry()) {
		string hostName = d->d_name;
		if (!checkFileUsedInDSK(hostName)) {
			// only if the file was not yet in the virtual disk
			foundNewHostFile(hostName);
		}
	}
}

void DirAsDSK::foundNewHostFile(const string& hostName)
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
	memcpy(msxDir(dirIndex).filename, msxFilename.data(), 8 + 3);
	importHostFile(dirIndex, fst);
}

void DirAsDSK::writeSectorImpl(unsigned sector, const byte* buf)
{
	assert(sector < NUM_SECTORS);
	assert(syncMode != SYNC_READONLY);

	// Update last access time
	if (Scheduler* scheduler = diskChanger.getScheduler()) {
		lastAccess = scheduler->getCurrentTime();
	}

	if (sector == 0) {
		// Ignore. We don't allow writing to the bootsector. It would
		// be very bad if the MSX tried to format this disk using other
		// disk parameters than this code assumes. It's also not useful
		// to write a different bootprogram to this disk because it
		// will be lost when this virtual disk is ejected.
	} else if (sector < FIRST_SECTOR_2ND_FAT) {
		writeFATSector(sector, buf);
	} else if (sector < FIRST_DIR_SECTOR) {
		// Write to 2nd FAT, only buffer it. Don't interpret the data
		// in FAT2 in any way (nor trigger any action on this write).
		memcpy(sectors[sector], buf, SECTOR_SIZE);
	} else if (sector < FIRST_DATA_SECTOR) {
		writeDIRSector(sector, buf);
	} else {
		writeDataSector(sector, buf);
	}
}

void DirAsDSK::writeFATSector(unsigned sector, const byte* buf)
{
	// Create copy of old FAT (to be able to detect changes)
	byte oldFAT[SECTORS_PER_FAT * SECTOR_SIZE];
	memcpy(oldFAT, fat(), sizeof(oldFAT));

	// Update current FAT with new data
	memcpy(sectors[sector], buf, SECTOR_SIZE);

	// Look for changes
	for (unsigned i = FIRST_CLUSTER; i < MAX_CLUSTER; ++i) {
		if (readFAT(i) != readFATHelper(oldFAT, i)) {
			exportFileFromFATChange(i, oldFAT);
		}
	}
	// At this point there should be no more differences.
	// Note: we can't use
	//   assert(memcmp(fat(), oldFAT, sizeof(oldFAT)) == 0);
	// because exportFileFromFATChange() only updates the part of the FAT
	// that actually contains FAT info. E.g. not the media ID at the
	// beginning nor the unsused part at the end. And for example the 'CALL
	// FORMAT' routine also writes these parts of the FAT.
	for (unsigned i = FIRST_CLUSTER; i < MAX_CLUSTER; ++i) {
		assert(readFAT(i) == readFATHelper(oldFAT, i));
	}
}

void DirAsDSK::exportFileFromFATChange(unsigned cluster, byte* oldFAT)
{
	// Get first cluster in the FAT chain that contains 'cluster'
	unsigned chainLength; // not used
	unsigned startCluster = getChainStart(cluster, chainLength);

	// Copy this whole chain from FAT1 to FAT2 (so that the loop in
	// writeFATSector() sees this part is already handled).
	unsigned tmp = startCluster;
	while ((FIRST_CLUSTER <= tmp) && (tmp < MAX_CLUSTER)) {
		unsigned next = readFAT(tmp);
		writeFATHelper(oldFAT, tmp, next);
		tmp = next;
	}

	// Find the corresponding direntry and (if found) export file based on
	// new cluster chain.
	unsigned dirIndex = getDirEntryForCluster(startCluster);
	if (dirIndex != unsigned(-1)) {
		exportToHostFile(dirIndex);
	}
}

unsigned DirAsDSK::getChainStart(unsigned cluster, unsigned& chainLength)
{
	// Search for the first cluster in the chain that contains 'cluster'
	// Note: worst case (this implementation of) the search is O(N^2), but
	// because usually FAT chains are allocated in ascending order, this
	// search is fast O(N).
	chainLength = 0;
	for (unsigned i = FIRST_CLUSTER; i < MAX_CLUSTER; ++i) {
		if (readFAT(i) == cluster) {
			// found a predecessor
			cluster = i;
			++chainLength;
			i = FIRST_CLUSTER - 1; // restart search
		}
	}
	return cluster;
}

unsigned DirAsDSK::getDirEntryForCluster(unsigned cluster)
{
	// Find the direntry with given start cluster
	for (unsigned i = 0; i < NUM_DIR_ENTRIES; ++i) {
		if (msxDir(i).startCluster == cluster) {
			return i;
		}
	}
	return unsigned(-1); // not found
}

void DirAsDSK::exportToHostFile(unsigned dirIndex)
{
	if (!mapDir[dirIndex].inUse()) {
		// Host file does not yet exist, create filename from msx name
		const char* msxName = msxDir(dirIndex).filename;
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
		unsigned curCl = msxDir(dirIndex).startCluster;
		unsigned msxSize = msxDir(dirIndex).size;

		string fullHostName = hostDir + mapDir[dirIndex].hostName;
		File file(fullHostName, File::TRUNCATE);
		unsigned offset = 0;
		while ((FIRST_CLUSTER <= curCl) && (curCl < MAX_CLUSTER)) {
			unsigned logicalSector = clusterToSector(curCl);
			for (unsigned i = 0; i < SECTORS_PER_CLUSTER; ++i) {
				if (offset >= msxSize) break;
				unsigned sector = logicalSector + i;
				assert(sector < NUM_SECTORS);
				unsigned writeSize = std::min(msxSize - offset, SECTOR_SIZE);
				file.write(sectors[sector], writeSize);
				offset += SECTOR_SIZE;
			}
			if (offset >= msxSize) break;
			curCl = readFAT(curCl);
		}
	} catch (FileException& e) {
		cliComm.printWarning("Error while syncing host file: " +
			mapDir[dirIndex].hostName + ": " + e.getMessage());
	}
}

void DirAsDSK::writeDIRSector(unsigned sector, const byte* buf)
{
	// Look for changed directory entries
	unsigned dirIndex = (sector - FIRST_DIR_SECTOR) * DIR_ENTRIES_PER_SECTOR;
	for (unsigned i = 0; i < DIR_ENTRIES_PER_SECTOR; ++i, ++dirIndex) {
		const MSXDirEntry& newEntry = *reinterpret_cast<const MSXDirEntry*>(
			&buf[sizeof(MSXDirEntry) * i]);
		if (memcmp(&msxDir(dirIndex), &newEntry, sizeof(newEntry)) != 0) {
			writeDIREntry(dirIndex, newEntry);
		}
	}
	// At this point sector should be updated
	assert(memcmp(sectors[sector], buf, SECTOR_SIZE) == 0);
}

void DirAsDSK::writeDIREntry(unsigned dirIndex, const MSXDirEntry& newEntry)
{
	if (memcmp(msxDir(dirIndex).filename, newEntry.filename, 8 + 3)) {
		// name in the direntry was changed
		if (mapDir[dirIndex].inUse()) {
			// If there is an associated hostfile, then delete it
			// (in case of a rename, the file will be recreated
			// below).
			string fullHostName = hostDir + mapDir[dirIndex].hostName;
			FileOperations::unlink(fullHostName); // ignore return value
			mapDir[dirIndex].hostName.clear();
		}
	}

	// Copy the new msx directory entry
	memcpy(&msxDir(dirIndex), &newEntry, sizeof(newEntry));

	// (Re-)export the full file
	exportToHostFile(dirIndex);
}

void DirAsDSK::writeDataSector(unsigned sector, const byte* buf)
{
	assert(sector >= FIRST_DATA_SECTOR);
	assert(sector < NUM_SECTORS);

	// Buffer the write, whether the sector is mapped to a file or not.
	memcpy(sectors[sector], buf, SECTOR_SIZE);

	// Get first cluster in the FAT chain that contains this sector.
	unsigned cluster, offset, chainLength;
	sectorToCluster(sector, cluster, offset);
	unsigned startCluster = getChainStart(cluster, chainLength);
	offset += (SECTOR_SIZE * SECTORS_PER_CLUSTER) * chainLength;

	// Get corresponding directory entry.
	unsigned dirIndex = getDirEntryForCluster(startCluster);
	if ((dirIndex == unsigned(-1)) || !mapDir[dirIndex].inUse()) {
		// This sector was not mapped to a file, nothing more to do.
		return;
	}

	// Actually write data to host file.
	string fullHostName = hostDir + mapDir[dirIndex].hostName;
	try {
		File file(fullHostName);
		file.seek(offset);
		unsigned msxSize = msxDir(dirIndex).size;
		if (msxSize > offset) {
			unsigned writeSize = std::min(msxSize - offset, SECTOR_SIZE);
			file.write(buf, writeSize);
		}
	} catch (FileException& e) {
		cliComm.printWarning("Couldn't write to file " + fullHostName +
		                     ": " + e.getMessage());
	}
}

} // namespace openmsx
