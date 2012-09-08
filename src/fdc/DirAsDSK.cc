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

	// use selected bootsector
	const byte* bootSector =
		  bootSectorType == BOOTSECTOR_DOS1
		? BootBlocks::dos1BootBlock
		: BootBlocks::dos2BootBlock;
	memcpy(&bootBlock, bootSector, sizeof(bootBlock));

	// make a clean initial disk
	cleandisk();
	// Import the host filesystem.
	syncWithHost();
}

void DirAsDSK::cleandisk()
{
	// assign empty directory entries
	for (unsigned i = 0; i < NUM_DIR_ENTRIES; ++i) {
		memset(&mapDir[i].msxInfo, 0, sizeof(MSXDirEntry));
		mapDir[i].hostName.clear();
		mapDir[i].mtime = 0;
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

	for (unsigned i = 0; i < NUM_SECTORS; ++i) {
		sectorMap[i].dirIndex = unsigned(-1);
		sectorMap[i].fileOffset = 0; // dummy value

		// 0xE5 is the value used on the Philips NMS8250
		memset(sectors[i], 0xE5, SECTOR_SIZE);
	}
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

	if (sector == 0) {
		// copy our fake bootsector into the buffer
		memcpy(buf, &bootBlock, SECTOR_SIZE);

	} else if (sector < FIRST_DIR_SECTOR) {
		// copy correct sector from FAT
		// Even if the MSX reads FAT2, we always return the content of FAT1.
		unsigned fatSector = (sector - FIRST_FAT_SECTOR) % SECTORS_PER_FAT;
		memcpy(buf, &fat[fatSector * SECTOR_SIZE], SECTOR_SIZE);

	} else if (sector < FIRST_DATA_SECTOR) {
		// create correct DIR sector
		sector -= FIRST_DIR_SECTOR;
		unsigned dirCount = sector * DIR_ENTRIES_PER_SECTOR;
		for (unsigned i = 0; i < DIR_ENTRIES_PER_SECTOR; ++i, ++dirCount) {
			memcpy(&buf[sizeof(MSXDirEntry) * i],
			       &(mapDir[dirCount].msxInfo),
			       sizeof(MSXDirEntry));
		}

	} else {
		// Read data sector.
		// Note that we DONT try to read the most current data from the
		// host file. Instead we always return the data from the last
		// host sync.
		memcpy(buf, sectors[sector], SECTOR_SIZE);
	}
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
	mapDir[dirIndex].msxInfo.filename[0] = char(0xE5);
	mapDir[dirIndex].hostName.clear(); // no longer mapped to host file

	// Clear the FAT chain to free up space in the virtual disk.
	freeFATChain(mapDir[dirIndex].msxInfo.startCluster);

}

void DirAsDSK::freeFATChain(unsigned curCl)
{
	// Follow a FAT chain and mark all clusters on this chain as free
	while ((FIRST_CLUSTER <= curCl) && (curCl < MAX_CLUSTER)) {
		unsigned nextCl = readFAT(curCl);
		writeFAT12(curCl, FREE_FAT);
		unsigned logicalSector = clusterToSector(curCl);
		for (unsigned i = 0; i < SECTORS_PER_CLUSTER; ++i) {
			unsigned sector = logicalSector + i;
			assert(sector < NUM_SECTORS);
			sectorMap[sector].dirIndex = unsigned(-1);
		}
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
	mapDir[dirIndex].msxInfo.time = t1;
	int t2 = mtim ? mtim->tm_mday + ((mtim->tm_mon + 1) << 5) +
	                ((mtim->tm_year + 1900 - 1980) << 9)
	              : 0;
	mapDir[dirIndex].msxInfo.date = t2;

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
				sectorMap[sector].dirIndex = dirIndex;
				sectorMap[sector].fileOffset = hostSize - remainingSize;
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
		freeFATChain(curCl);
	}

	// write (possibly truncated) file size
	mapDir[dirIndex].msxInfo.size = hostSize - remainingSize;

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
	memcpy(&(mapDir[dirIndex].msxInfo.filename), msxFilename.data(), 8 + 3);
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
	// Observation:
	//  A real MSX always first seems to write the sectors of the first FAT
	//  shortly followed by writing an (identical) copy to the second FAT.
	// Heuristic:
	//  We prefer to not start updating host files based on a partially
	//  updated FAT (e.g. only one of the 3 FAT sectors changed yet). So
	//  what we do is the following:
	//  - Only buffer writes to FAT1, don't make any host changes yet.
	//  - On (any) write to FAT2 we will sync the changes made by the
	//    previous FAT1 writes (the actual sector data that is written to
	//    FAT2 is ignored).
	static const unsigned FIRST_SECTOR_2ND_FAT =
	       FIRST_FAT_SECTOR + SECTORS_PER_FAT;
	if (sector < FIRST_SECTOR_2ND_FAT) {
		// write to FAT1, only buffer the data, don't update host files
		unsigned fatSector = sector - FIRST_FAT_SECTOR;
		memcpy(&fat[fatSector * SECTOR_SIZE], buf, SECTOR_SIZE);
	} else {
		// Write to FAT2 (any FAT2 sector, actual written data is
		// ignored). Sync host files based on changes written earlier
		// to FAT1.
		syncFATChanges();
	}
}

void DirAsDSK::syncFATChanges()
{
	for (unsigned i = FIRST_CLUSTER; i < MAX_CLUSTER; ++i) {
		if (readFAT(i) != readFAT2(i)) {
			exportFileFromFATChange(i);
		}
	}
	// After a write to FAT2 (any sector, any data), our (full) fat1 and
	// fat2 buffers should be identical. Note: we can't use
	//   assert(memcmp(fat, fat2, sizeof(fat)) == 0);
	// because exportFileFromFATChange() only updates the part of the FAT
	// that actually contains FAT info. E.g. not the media ID at the
	// beginning nor the unsused part at the end. And for example the 'CALL
	// FORMAT' routine also writes these parts of the FAT.
	for (unsigned i = FIRST_CLUSTER; i < MAX_CLUSTER; ++i) {
		assert(readFAT(i) == readFAT2(i));
	}
}

void DirAsDSK::exportFileFromFATChange(unsigned cluster)
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

	// Copy this whole chain from FAT1 to FAT2 (so that the loop in
	// syncFATChanges() sees this part is already handled).
	unsigned tmp = startCluster;
	while ((FIRST_CLUSTER <= tmp) && (tmp < MAX_CLUSTER)) {
		unsigned next = readFAT(tmp);
		writeFAT2(tmp, next);
		tmp = next;
	}

	// Find the corresponding direntry and (if found) export file based on
	// new cluster chain.
	for (unsigned i = 0; i < NUM_DIR_ENTRIES; ++i) {
		if (startCluster == mapDir[i].msxInfo.startCluster) {
			exportToHostFile(i);
			break;
		}
	}
}

void DirAsDSK::exportToHostFile(unsigned dirIndex)
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
				if (offset >= msxSize) break;
				unsigned sector = logicalSector + i;
				assert(sector < NUM_SECTORS);
				unsigned writeSize = std::min(msxSize - offset, SECTOR_SIZE);
				file.write(sectors[sector], writeSize);
				sectorMap[sector].dirIndex = dirIndex;
				sectorMap[sector].fileOffset = offset;
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
	if (memcmp(mapDir[dirIndex].msxInfo.filename, newEntry.filename, 8 + 3)) {
		// name in the direntry was changed
		if (mapDir[dirIndex].inUse()) {
			// If there is an associated hostfile, then delete it
			// (in case of a rename, the file will be recreated
			// below).
			string fullHostName = hostDir + mapDir[dirIndex].hostName;
			FileOperations::unlink(fullHostName); // ignore return value
			for (unsigned i = FIRST_DATA_SECTOR; i < NUM_SECTORS; ++i) {
				if (sectorMap[i].dirIndex == dirIndex) {
					 sectorMap[i].dirIndex = unsigned(-1);
				}
			}
			mapDir[dirIndex].hostName.clear();
		}
	}

	// Copy the new msx directory entry
	memcpy(&(mapDir[dirIndex].msxInfo), &newEntry, sizeof(newEntry));

	// (Re-)export the full file
	exportToHostFile(dirIndex);
}

void DirAsDSK::writeDataSector(unsigned sector, const byte* buf)
{
	assert(sector >= FIRST_DATA_SECTOR);
	assert(sector < NUM_SECTORS);

	// Buffer the write, whether the sector is mapped to a file or not.
	memcpy(sectors[sector], buf, SECTOR_SIZE);

	unsigned dirIndex = sectorMap[sector].dirIndex;
	if (dirIndex == unsigned(-1)) {
		// This sector was not mapped to a file, nothing more to do.
		return;
	}

	// Actually write data to host file.
	unsigned offset = sectorMap[sector].fileOffset;
	string fullHostName = hostDir + mapDir[dirIndex].hostName;
	try {
		File file(fullHostName);
		file.seek(offset);
		unsigned msxSize = mapDir[dirIndex].msxInfo.size;
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
