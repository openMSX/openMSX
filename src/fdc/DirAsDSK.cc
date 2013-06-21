#include "DirAsDSK.hh"
#include "DiskChanger.hh"
#include "Scheduler.hh"
#include "CliComm.hh"
#include "BootBlocks.hh"
#include "File.hh"
#include "FileException.hh"
#include "ReadDir.hh"
#include "StringOp.hh"
#include <algorithm>
#include <cassert>
#include <cstring>
#include <vector>

using std::map;
using std::string;
using std::vector;

namespace openmsx {

static const unsigned SECTORS_PER_FAT = 3;
static const unsigned SECTORS_PER_DIR = 7;
static const unsigned NUM_FATS = 2;
static const unsigned SECTORS_PER_CLUSTER = 2;
static const unsigned FIRST_FAT_SECTOR = 1;
static const unsigned FIRST_SECTOR_2ND_FAT =
	FIRST_FAT_SECTOR + SECTORS_PER_FAT;
static const unsigned FIRST_DIR_SECTOR =
	FIRST_FAT_SECTOR + NUM_FATS * SECTORS_PER_FAT;
static const unsigned FIRST_DATA_SECTOR =
	FIRST_DIR_SECTOR + SECTORS_PER_DIR;
static const unsigned DIR_ENTRIES_PER_SECTOR =
       DirAsDSK::SECTOR_SIZE / sizeof(MSXDirEntry);

// First valid regular cluster number.
static const unsigned FIRST_CLUSTER = 2;
// First cluster number that can NOT be used anymore.
static const unsigned MAX_CLUSTER =
	(DirAsDSK::NUM_SECTORS - FIRST_DATA_SECTOR) / SECTORS_PER_CLUSTER + FIRST_CLUSTER;

static const unsigned FREE_FAT = 0x000;
static const unsigned BAD_FAT  = 0xFF7;
static const unsigned EOF_FAT  = 0xFFF; // actually 0xFF8-0xFFF


// Transform BAD_FAT (0xFF7) and EOF_FAT-range (0xFF8-0xFFF)
// to a single value: EOF_FAT (0xFFF).
static unsigned normalizeFAT(unsigned cluster)
{
	return (cluster < BAD_FAT) ? cluster : EOF_FAT;
}

static unsigned readFATHelper(const byte* buf, unsigned cluster)
{
	assert(FIRST_CLUSTER <= cluster);
	assert(cluster < MAX_CLUSTER);
	auto* p = &buf[(cluster * 3) / 2];
	unsigned result = (cluster & 1)
	                ? (p[0] >> 4) + (p[1] << 4)
	                : p[0] + ((p[1] & 0x0F) << 8);
	return normalizeFAT(result);
}

static void writeFATHelper(byte* buf, unsigned cluster, unsigned val)
{
	assert(FIRST_CLUSTER <= cluster);
	assert(cluster < MAX_CLUSTER);
	auto* p = &buf[(cluster * 3) / 2];
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

// Read entry from FAT.
unsigned DirAsDSK::readFAT(unsigned cluster)
{
	return readFATHelper(fat(), cluster);
}

// Write an entry to both FAT1 and FAT2.
void DirAsDSK::writeFAT12(unsigned cluster, unsigned val)
{
	writeFATHelper(fat (), cluster, val);
	writeFATHelper(fat2(), cluster, val);
	// An alternative is to copy FAT1 to FAT2 after changes have been made
	// to FAT1. This is probably more like what the real disk rom does.
}

// Returns MAX_CLUSTER in case of no more free clusters
unsigned DirAsDSK::findNextFreeCluster(unsigned cluster)
{
	assert(cluster < MAX_CLUSTER);
	do {
		++cluster;
		assert(cluster >= FIRST_CLUSTER);
	} while ((cluster < MAX_CLUSTER) && (readFAT(cluster) != FREE_FAT));
	return cluster;
}
unsigned DirAsDSK::findFirstFreeCluster()
{
	return findNextFreeCluster(FIRST_CLUSTER - 1);
}

// Throws when there are no more free clusters.
unsigned DirAsDSK::getFreeCluster()
{
	unsigned cluster = findFirstFreeCluster();
	if (cluster == MAX_CLUSTER) {
		throw MSXException("disk full");
	}
	return cluster;
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
static unsigned sectorToCluster(unsigned sector)
{
	unsigned cluster, offset;
	sectorToCluster(sector, cluster, offset);
	return cluster;
}

MSXDirEntry& DirAsDSK::msxDir(DirIndex dirIndex)
{
	assert(dirIndex.sector < NUM_SECTORS);
	assert(dirIndex.idx    < DIR_ENTRIES_PER_SECTOR);
	auto dirs = reinterpret_cast<MSXDirEntry*>(sectors[dirIndex.sector]);
	return dirs[dirIndex.idx];
}

// Returns -1 when there are no more sectors for this directory.
unsigned DirAsDSK::nextMsxDirSector(unsigned sector)
{
	if (sector < FIRST_DATA_SECTOR) {
		// Root directory.
		assert(FIRST_DIR_SECTOR <= sector);
		++sector;
		if (sector == FIRST_DATA_SECTOR) {
			// Root directory has a fixed number of sectors.
			return unsigned(-1);
		}
		return sector;
	} else {
		// Subdirectory.
		unsigned cluster, offset;
		sectorToCluster(sector, cluster, offset);
		if (offset < ((SECTORS_PER_CLUSTER - 1) * DirAsDSK::SECTOR_SIZE)) {
			// Next sector still in same cluster.
			return sector + 1;
		}
		unsigned nextCl = readFAT(cluster);
		if ((nextCl < FIRST_CLUSTER) || (MAX_CLUSTER <= nextCl)) {
			// No next cluster, end of directory reached.
			return unsigned(-1);
		}
		return clusterToSector(nextCl);
	}
}

// Check if a msx filename is used in a specific msx (sub)directory.
bool DirAsDSK::checkMSXFileExists(
	const string& msxFilename, unsigned msxDirSector)
{
	do {
		for (unsigned idx = 0; idx < DIR_ENTRIES_PER_SECTOR; ++idx) {
			DirIndex dirIndex(msxDirSector, idx);
			if (memcmp(msxDir(dirIndex).filename,
			           msxFilename.data(), 8 + 3) == 0) {
				return true;
			}
		}
		msxDirSector = nextMsxDirSector(msxDirSector);
	} while (msxDirSector != unsigned(-1));

	// Searched through all sectors of this (sub)directory.
	return false;
}

// Returns msx directory entry for the given host file. Or -1 if the host file
// is not mapped in the virtual disk.
DirAsDSK::DirIndex DirAsDSK::findHostFileInDSK(const string& hostName)
{
	for (auto& p : mapDirs) {
		if (p.second.hostName == hostName) {
			return p.first;
		}
	}
	return DirIndex(unsigned(-1), unsigned(-1));
}

// Check if a host file is already mapped in the virtual disk.
bool DirAsDSK::checkFileUsedInDSK(const string& hostName)
{
	DirIndex dirIndex = findHostFileInDSK(hostName);
	return dirIndex.sector != unsigned(-1);
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
	memcpy(&*result.begin() + 0, file.data(), std::min<size_t>(8, file.size()));
	memcpy(&*result.begin() + 8, ext .data(), std::min<size_t>(3, ext .size()));
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

	// First create structure for the virtual disk.
	setNbSectors(NUM_SECTORS); // asume a DS disk is used
	setSectorsPerTrack(9);
	setNbSides(2);

	// Initially the whole disk is filled with 0xE5 (at least on Philips
	// NMS8250).
	memset(sectors, 0xE5, sizeof(sectors));

	// Use selected bootsector.
	auto* bootSector = bootSectorType == BOOTSECTOR_DOS1
	                 ? BootBlocks::dos1BootBlock
	                 : BootBlocks::dos2BootBlock;
	memcpy(sectors[0], bootSector, SECTOR_SIZE);

	// Clear FAT1 + FAT2.
	memset(fat(), 0, SECTOR_SIZE * SECTORS_PER_FAT * NUM_FATS);
	// First 3 bytes are initialized specially:
	//  'cluster 0' contains the media descriptor (0xF9 for us)
	//  'cluster 1' is marked as EOF_FAT
	//  So cluster 2 is the first usable cluster number
	fat ()[0] = 0xF9; fat ()[1] = 0xFF; fat ()[2] = 0xFF;
	fat2()[0] = 0xF9; fat2()[1] = 0xFF; fat2()[2] = 0xFF;

	// Assign empty directory entries.
	memset(sectors[FIRST_DIR_SECTOR], 0, SECTOR_SIZE * SECTORS_PER_DIR);

	// No host files are mapped to this disk yet.
	assert(mapDirs.empty());

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
	if (auto* scheduler = diskChanger.getScheduler()) {
		auto now = scheduler->getCurrentTime();
		auto delta = now - lastAccess;
		needSync = delta > EmuDuration::sec(1);
		// Do not update lastAccess because we don't actually call
		// syncWithHost().
	} else {
		// Happens when dirasdisk is used in virtual_drive.
		needSync = true;
	}
	if (needSync) {
		flushCaches();
	}
}

void DirAsDSK::readSectorImpl(size_t sector, byte* buf)
{
	assert(sector < NUM_SECTORS);

	// 'Peek-mode' is used to periodically calculate a sha1sum for the
	// whole disk (used by reverse). We don't want this calculation to
	// interfer with the access time we use for normal read/writes. So in
	// peek-mode we skip the whole sync-step.
	if (!isPeekMode()) {
		bool needSync;
		if (auto* scheduler = diskChanger.getScheduler()) {
			auto now = scheduler->getCurrentTime();
			auto delta = now - lastAccess;
			lastAccess = now;
			needSync = delta > EmuDuration::sec(1);
		} else {
			// Happens when dirasdisk is used in virtual_drive.
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
	checkModifiedHostFiles();

	// Last add new host files (this can only consume virtual disk space).
	addNewHostFiles("", FIRST_DIR_SECTOR);
}

void DirAsDSK::checkDeletedHostFiles()
{
	// This handles both host files and directories.
	auto copy = mapDirs;
	for (auto& p : copy) {
		if (mapDirs.find(p.first) == mapDirs.end()) {
			// While iterating over (the copy of) mapDirs we delete
			// entries of mapDirs (when we delete files only the
			// current entry is deleted, when we delete
			// subdirectories possibly many entries are deleted).
			// At this point in the code we've reached such an
			// entry that's still in the original (copy of) mapDirs
			// but has already been deleted from the current
			// mapDirs. Ignore it.
			continue;
		}
		const DirIndex& dirIndex = p.first;
		MapDir& mapDir = p.second;
		string fullHostName = hostDir + mapDir.hostName;
		bool isMSXDirectory = (msxDir(dirIndex).attrib &
		                       MSXDirEntry::ATT_DIRECTORY) != 0;
		FileOperations::Stat fst;
		if ((!FileOperations::getStat(fullHostName, fst) != 0) ||
		    (FileOperations::isDirectory(fst) != isMSXDirectory)) {
			// TODO also check access permission
			// Error stat-ing file, or directory/file type is not
			// the same on the msx and host side (e.g. a host file
			// has been removed and a host directory with the same
			// name has been created). In both cases delete the msx
			// entry (if needed it will be recreated soon).
			deleteMSXFile(dirIndex);
		}
	}
}

void DirAsDSK::deleteMSXFile(DirIndex dirIndex)
{
	// Remove mapping between host and msx file (if any).
	mapDirs.erase(dirIndex);

	char c = msxDir(dirIndex).filename[0];
	if (c == 0 || c == char(0xE5)) {
		// Directory entry not in use, don't need to do anything.
		return;
	}

	if (msxDir(dirIndex).attrib & MSXDirEntry::ATT_DIRECTORY) {
		// If we're deleting a directory then also (recursively)
		// delete the files/directories in this directory.
		const char* msxName = msxDir(dirIndex).filename;
		if ((memcmp(msxName, ".          ", 11) == 0) ||
		    (memcmp(msxName, "..         ", 11) == 0)) {
			// But skip the "." and ".." entries.
			return;
		}
		// Sanity check on cluster range.
		unsigned cluster = msxDir(dirIndex).startCluster;
		if ((FIRST_CLUSTER <= cluster) && (cluster < MAX_CLUSTER)) {
			// Recursively delete all files in this subdir.
			deleteMSXFilesInDir(clusterToSector(cluster));
		}
	}

	// At this point we have a regular file or an empty subdirectory.
	// Delete it by marking the first filename char as 0xE5.
	msxDir(dirIndex).filename[0] = char(0xE5);

	// Clear the FAT chain to free up space in the virtual disk.
	freeFATChain(msxDir(dirIndex).startCluster);
}

void DirAsDSK::deleteMSXFilesInDir(unsigned msxDirSector)
{
	do {
		for (unsigned idx = 0; idx < DIR_ENTRIES_PER_SECTOR; ++idx) {
			deleteMSXFile(DirIndex(msxDirSector, idx));
		}
		msxDirSector = nextMsxDirSector(msxDirSector);
	} while (msxDirSector != unsigned(-1));
}

void DirAsDSK::freeFATChain(unsigned cluster)
{
	// Follow a FAT chain and mark all clusters on this chain as free.
	while ((FIRST_CLUSTER <= cluster) && (cluster < MAX_CLUSTER)) {
		unsigned nextCl = readFAT(cluster);
		writeFAT12(cluster, FREE_FAT);
		cluster = nextCl;
	}
}

void DirAsDSK::checkModifiedHostFiles()
{
	auto copy = mapDirs;
	for (auto& p : copy) {
		if (mapDirs.find(p.first) == mapDirs.end()) {
			// See comment in checkDeletedHostFiles().
			continue;
		}
		const DirIndex& dirIndex = p.first;
		MapDir& mapDir = p.second;
		string fullHostName = hostDir + mapDir.hostName;
		bool isMSXDirectory = (msxDir(dirIndex).attrib &
		                       MSXDirEntry::ATT_DIRECTORY) != 0;
		FileOperations::Stat fst;
		if ((!FileOperations::getStat(fullHostName, fst) == 0) &&
		    (FileOperations::isDirectory(fst) == isMSXDirectory)) {
			// Detect changes in host file.
			// Heuristic: we use filesize and modification time to detect
			// changes in file content.
			//  TODO do we need both filesize and mtime or is mtime alone
			//       enough?
			// We ignore time/size changes in directories,
			// typically such a change indicates one of the files
			// in that directory is changed/added/removed. But such
			// changes are handled elsewhere.
			if (!isMSXDirectory &&
			    ((mapDir.mtime    != fst.st_mtime) ||
			     (mapDir.filesize != size_t(fst.st_size)))) {
				importHostFile(dirIndex, fst);
			}
		} else {
			// Only very rarely happens (because checkDeletedHostFiles()
			// checked this just recently).
			deleteMSXFile(dirIndex);
		}
	}
}

void DirAsDSK::importHostFile(DirIndex dirIndex, FileOperations::Stat& fst)
{
	assert(!(msxDir(dirIndex).attrib & MSXDirEntry::ATT_DIRECTORY));
	assert(mapDirs.find(dirIndex) != mapDirs.end());

	// Set _msx_ modification time.
	setMSXTimeStamp(dirIndex, fst);

	// Set _host_ modification time (and filesize)
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
	size_t hostSize = fst.st_size;
	auto& mapDir = mapDirs[dirIndex];
	mapDir.filesize = hostSize;
	mapDir.mtime = fst.st_mtime;

	bool moreClustersInChain = true;
	unsigned curCl = msxDir(dirIndex).startCluster;
	// If there is no cluster assigned yet to this file (curCl == 0), then
	// find a free cluster. Treat invalid cases in the same way (curCl == 1
	// or curCl >= MAX_CLUSTER).
	if ((curCl < FIRST_CLUSTER) || (curCl >= MAX_CLUSTER)) {
		moreClustersInChain = false;
		curCl = findFirstFreeCluster(); // MAX_CLUSTER in case of disk-full
	}

	auto remainingSize = hostSize;
	unsigned prevCl = 0;
	try {
		string fullHostName = hostDir + mapDir.hostName;
		File file(fullHostName, "rb"); // don't uncompress

		while (remainingSize && (curCl < MAX_CLUSTER)) {
			unsigned logicalSector = clusterToSector(curCl);
			for (unsigned i = 0; i < SECTORS_PER_CLUSTER; ++i) {
				unsigned sector = logicalSector + i;
				assert(sector < NUM_SECTORS);
				byte* buf = sectors[sector];
				memset(buf, 0, SECTOR_SIZE); // in case (end of) file only fills partial sector
				auto sz = std::min(remainingSize, SECTOR_SIZE);
				file.read(buf, sz);
				remainingSize -= sz;
				if (remainingSize == 0) {
					// Don't fill next sectors in this cluster
					// if there is no data left.
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
			                     mapDir.hostName + " truncated.");
		}
	} catch (FileException& e) {
		// Error opening or reading host file.
		cliComm.printWarning("Error reading host file: " +
				mapDir.hostName + ": " + e.getMessage() +
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

	// Clear remains of FAT if needed.
	if (moreClustersInChain) {
		freeFATChain(curCl);
	}

	// Write (possibly truncated) file size.
	msxDir(dirIndex).size = uint32_t(hostSize - remainingSize);

	// TODO in case of an error (disk image full, or host file read error),
	// wouldn't it be better to remove the (half imported) msx file again?
	// Sometimes when I'm using DirAsDSK I have one file that is too big
	// (e.g. a core file, or a vim .swp file) and that one prevents
	// DirAsDSK from importing the other (small) files in my directory.
}

void DirAsDSK::setMSXTimeStamp(DirIndex dirIndex, FileOperations::Stat& fst)
{
	// Use intermediate param to prevent compilation error for Android
	auto mtime = fst.st_mtime;
	auto* mtim = localtime(&mtime);
	int t1 = mtim ? (mtim->tm_sec >> 1) + (mtim->tm_min << 5) +
	                (mtim->tm_hour << 11)
	              : 0;
	msxDir(dirIndex).time = t1;
	int t2 = mtim ? mtim->tm_mday + ((mtim->tm_mon + 1) << 5) +
	                ((mtim->tm_year + 1900 - 1980) << 9)
	              : 0;
	msxDir(dirIndex).date = t2;
}

void DirAsDSK::addNewHostFiles(const string& hostSubDir, unsigned msxDirSector)
{
	assert(!StringOp::startsWith(hostSubDir, '/'));
	assert(hostSubDir.empty() || StringOp::endsWith(hostSubDir, '/'));

	ReadDir dir(hostDir + hostSubDir);
	while (auto* d = dir.getEntry()) {
		try {
			string hostName = d->d_name;
			string fullHostName = hostDir + hostSubDir + hostName;
			FileOperations::Stat fst;
			if (!FileOperations::getStat(fullHostName, fst)) {
				throw MSXException("Error accessing " + fullHostName);
			}
			if (FileOperations::isDirectory(fst)) {
				if ((hostName == "..") || (hostName == ".")) {
					continue;
				}
				addNewDirectory(hostSubDir, hostName, msxDirSector, fst);
			} else if (FileOperations::isRegularFile(fst)) {
				addNewHostFile(hostSubDir, hostName, msxDirSector, fst);
			} else {
				throw MSXException("Not a regular file: " +
				                   fullHostName);
			}
		} catch (MSXException& e) {
			cliComm.printWarning(e.getMessage());
		}
	}
}

void DirAsDSK::addNewDirectory(const string& hostSubDir, const string& hostName,
                               unsigned msxDirSector, FileOperations::Stat& fst)
{
	string hostPath = hostSubDir + hostName;
	DirIndex dirIndex = findHostFileInDSK(hostPath);
	unsigned newMsxDirSector;
	if (dirIndex.sector == unsigned(-1)) {
		// MSX directory doesn't exist yet, create it.
		// Allocate a cluster to hold the subdirectory entries.
		unsigned cluster = getFreeCluster();
		writeFAT12(cluster, EOF_FAT);

		// Allocate and fill in directory entry.
		try {
			dirIndex = fillMSXDirEntry(hostSubDir, hostName, msxDirSector);
		} catch (...) {
			// Rollback allocation of directory cluster.
			writeFAT12(cluster, FREE_FAT);
			throw;
		}
		setMSXTimeStamp(dirIndex, fst);
		msxDir(dirIndex).attrib = MSXDirEntry::ATT_DIRECTORY;
		msxDir(dirIndex).startCluster = cluster;

		// Initialize the new directory.
		newMsxDirSector = clusterToSector(cluster);
		for (unsigned i = 0; i < SECTORS_PER_CLUSTER; ++i) {
			memset(sectors[newMsxDirSector + i], 0, SECTOR_SIZE);
		}
		DirIndex idx0(newMsxDirSector, 0); // entry for "."
		DirIndex idx1(newMsxDirSector, 1); //           ".."
		memset(msxDir(idx0).filename, ' ', 11);
		memset(msxDir(idx1).filename, ' ', 11);
		memset(msxDir(idx0).filename, '.', 1);
		memset(msxDir(idx1).filename, '.', 2);
		msxDir(idx0).attrib = MSXDirEntry::ATT_DIRECTORY;
		msxDir(idx1).attrib = MSXDirEntry::ATT_DIRECTORY;
		setMSXTimeStamp(idx0, fst);
		setMSXTimeStamp(idx1, fst);
		msxDir(idx0).startCluster = cluster;
		msxDir(idx1).startCluster = msxDirSector == FIRST_DIR_SECTOR
		                          ? 0 : sectorToCluster(msxDirSector);
	} else {
		if (!(msxDir(dirIndex).attrib & MSXDirEntry::ATT_DIRECTORY)) {
			// Should rarely happen because checkDeletedHostFiles()
			// recently checked this. (It could happen when a host
			// directory is *just*recently* created with the same
			// name as an existing msx file). Ignore, it will be
			// corrected in the next sync.
			return;
		}
		unsigned cluster = msxDir(dirIndex).startCluster;
		if ((cluster < FIRST_CLUSTER) || (cluster >= MAX_CLUSTER)) {
			// Sanity check on cluster range.
			return;
		}
		newMsxDirSector = clusterToSector(cluster);
	}

	// Recursively process this directory.
	addNewHostFiles(hostSubDir + hostName + '/', newMsxDirSector);
}

void DirAsDSK::addNewHostFile(const string& hostSubDir, const string& hostName,
                              unsigned msxDirSector, FileOperations::Stat& fst)
{
	if (checkFileUsedInDSK(hostSubDir + hostName)) {
		// File is already present in the virtual disk, do nothing.
		return;
	}
	string hostPath = hostSubDir + hostName;
	string fullHostName = hostDir + hostPath;

	// TODO check for available free space on disk instead of max free space
	static const int DISK_SPACE = (NUM_SECTORS - FIRST_DATA_SECTOR) * SECTOR_SIZE;
	if (fst.st_size > DISK_SPACE) {
		cliComm.printWarning("File too large: " + fullHostName);
		return;
	}

	DirIndex dirIndex = fillMSXDirEntry(hostSubDir, hostName, msxDirSector);
	importHostFile(dirIndex, fst);
}

DirAsDSK::DirIndex DirAsDSK::fillMSXDirEntry(
	const string& hostSubDir, const string& hostName, unsigned msxDirSector)
{
	string hostPath = hostSubDir + hostName;
	try {
		// Get empty dir entry (possibly extends subdirectory).
		DirIndex dirIndex = getFreeDirEntry(msxDirSector);

		// Create correct MSX filename.
		string msxFilename = hostToMsxName(hostName);
		if (checkMSXFileExists(msxFilename, msxDirSector)) {
			// TODO: actually should increase vfat abrev if possible!!
			throw MSXException(
				"MSX name " + msxFilename + " exists already");
		}

		// Fill in hostName / msx filename.
		assert(!StringOp::endsWith(hostPath, '/'));
		mapDirs[dirIndex].hostName = hostPath;
		memset(&msxDir(dirIndex), 0, sizeof(MSXDirEntry)); // clear entry
		memcpy(msxDir(dirIndex).filename, msxFilename.data(), 8 + 3);
		return dirIndex;
	} catch (MSXException& e) {
		throw MSXException("Couldn't add " + hostPath + ": " +
		                   e.getMessage());
	}
}

DirAsDSK::DirIndex DirAsDSK::getFreeDirEntry(unsigned msxDirSector)
{
	while (true) {
		for (unsigned idx = 0; idx < DIR_ENTRIES_PER_SECTOR; ++idx) {
			DirIndex dirIndex(msxDirSector, idx);
			const char* msxName = msxDir(dirIndex).filename;
			if ((msxName[0] == char(0x00)) ||
			    (msxName[0] == char(0xE5))) {
				// Found an unused msx entry. There shouldn't
				// be any hostfile mapped to this entry.
				assert(mapDirs.find(dirIndex) == mapDirs.end());
				return dirIndex;
			}
		}
		unsigned sector = nextMsxDirSector(msxDirSector);
		if (sector == unsigned(-1)) break;
		msxDirSector = sector;
	}

	// No free space in existing directory.
	if (msxDirSector == (FIRST_DATA_SECTOR - 1)) {
		// Can't extend root directory.
		throw MSXException("root directory full");
	}

	// Extend sub-directory: allocate and clear a new cluster, add this
	// cluster in the existing FAT chain.
	unsigned cluster = sectorToCluster(msxDirSector);
	unsigned newCluster = getFreeCluster();
	unsigned sector = clusterToSector(newCluster);
	memset(sectors[sector], 0, SECTORS_PER_CLUSTER * SECTOR_SIZE);
	writeFAT12(cluster, newCluster);
	writeFAT12(newCluster, EOF_FAT);

	// First entry in this newly allocated cluster is free. Return it.
	return DirIndex(sector, 0);
}

void DirAsDSK::writeSectorImpl(size_t sector_, const byte* buf)
{
	assert(sector_ < NUM_SECTORS);
	assert(syncMode != SYNC_READONLY);
	auto sector = unsigned(sector_);

	// Update last access time.
	if (auto* scheduler = diskChanger.getScheduler()) {
		lastAccess = scheduler->getCurrentTime();
	}

	DirIndex dirDirIndex;
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
	} else if (isDirSector(sector, dirDirIndex)) {
		// Either root- or sub-directory.
		writeDIRSector(sector, dirDirIndex, buf);
	} else {
		writeDataSector(sector, buf);
	}
}

void DirAsDSK::writeFATSector(unsigned sector, const byte* buf)
{
	// Create copy of old FAT (to be able to detect changes).
	byte oldFAT[SECTORS_PER_FAT * SECTOR_SIZE];
	memcpy(oldFAT, fat(), sizeof(oldFAT));

	// Update current FAT with new data.
	memcpy(sectors[sector], buf, SECTOR_SIZE);

	// Look for changes.
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
	// Get first cluster in the FAT chain that contains 'cluster'.
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
	DirIndex dirIndex, dirDirIndex;
	if (getDirEntryForCluster(startCluster, dirIndex, dirDirIndex)) {
		exportToHost(dirIndex, dirDirIndex);
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
			// Found a predecessor.
			cluster = i;
			++chainLength;
			i = FIRST_CLUSTER - 1; // restart search
		}
	}
	return cluster;
}

// Generic helper function that walks over the whole MSX directory tree
// (possibly it stops early so it doesn't always walk over the whole tree).
// The action that is performed while walking depends on the functor parameter.
template<typename FUNC> bool DirAsDSK::scanMsxDirs(FUNC func, unsigned sector)
{
	size_t rdIdx = 0;
	vector<unsigned> dirs;  // TODO make vector of struct instead of
	vector<DirIndex> dirs2; //      2 parallel vectors.
	while (true) {
		do {
			// About to process a new directory sector.
			if (func.onDirSector(sector)) return true;

			for (unsigned idx = 0; idx < DIR_ENTRIES_PER_SECTOR; ++idx) {
				// About to process a new directory entry.
				DirIndex dirIndex(sector, idx);
				const MSXDirEntry& entry = msxDir(dirIndex);
				if (func.onDirEntry(dirIndex, entry)) return true;

				if ((entry.filename[0] == char(0x00)) ||
				    (entry.filename[0] == char(0xE5)) ||
				    !(entry.attrib & MSXDirEntry::ATT_DIRECTORY)) {
					// Not a directory.
					continue;
				}
				unsigned cluster = msxDir(dirIndex).startCluster;
				if ((cluster < FIRST_CLUSTER) ||
				    (cluster >= MAX_CLUSTER)) {
					// Cluster=0 happens for ".." entries to
					// the root directory, also be robust for
					// bogus data.
					continue;
				}
				unsigned dir = clusterToSector(cluster);
				if (find(dirs.begin(), dirs.end(), dir) !=
				    dirs.end()) {
					// Already found this sector. Except
					// for the special "." and ".."
					// entries, loops should not occur in
					// valid disk images, but don't crash
					// on (intentionally?) invalid images.
					continue;
				}
				// Found a new directory, insert in the set of
				// yet-to-be-processed directories.
				dirs.push_back(dir);
				dirs2.push_back(dirIndex);
			}
			sector = nextMsxDirSector(sector);
		} while (sector != unsigned(-1));

		// Scan next subdirectory (if any).
		if (rdIdx == dirs.size()) {
			// Visited all directories.
			return false;
		}
		// About to process a new subdirectory.
		func.onVisitSubDir(dirs2[rdIdx]);
		sector = dirs[rdIdx++];
	}
}

// Base class for functor objects to be used in scanMsxDirs().
// This implements all required methods with empty implementations.
struct NullScanner {
	// Called right before we enter a new subdirectory.
	void onVisitSubDir(DirAsDSK::DirIndex /*subdir*/) {}

	// Called when a new sector of a (sub)directory is being scanned.
	inline bool onDirSector(unsigned /*dirSector*/) {
		return false;
	}

	// Called for each directory entry (in a sector).
	inline bool onDirEntry(DirAsDSK::DirIndex /*dirIndex*/,
	                       const MSXDirEntry& /*entry*/) {
		return false;
	}
};

// Base class for the IsDirSector and DirEntryForCluster scanner algorithms
// below. This class remembers the directory entry of the last visited subdir.
struct DirScanner : NullScanner {
	DirScanner(DirAsDSK::DirIndex& dirDirIndex_)
		: dirDirIndex(dirDirIndex_)
	{
		dirDirIndex = DirAsDSK::DirIndex(0, 0); // represents entry for root dir
	}

	// Called right before we enter a new subdirectory.
	void onVisitSubDir(DirAsDSK::DirIndex subdir) {
		dirDirIndex = subdir;
	}

	DirAsDSK::DirIndex& dirDirIndex;
};

// Figure out whether a given sector is part of the msx directory structure.
struct IsDirSector : DirScanner {
	IsDirSector(unsigned sector_, DirAsDSK::DirIndex& dirDirIndex)
		: DirScanner(dirDirIndex)
		, sector(sector_) {}
	bool onDirSector(unsigned dirSector) {
		return sector == dirSector;
	}
	const unsigned sector;
};
bool DirAsDSK::isDirSector(unsigned sector, DirIndex& dirDirIndex)
{
	return scanMsxDirs(IsDirSector(sector, dirDirIndex), FIRST_DIR_SECTOR);
}

// Search for the directory entry that has the given startCluster.
struct DirEntryForCluster : DirScanner {
	DirEntryForCluster(unsigned cluster_,
	                   DirAsDSK::DirIndex& dirIndex_,
	                   DirAsDSK::DirIndex& dirDirIndex)
		: DirScanner(dirDirIndex)
		, cluster(cluster_)
		, result(dirIndex_) {}
	bool onDirEntry(DirAsDSK::DirIndex dirIndex, const MSXDirEntry& entry) {
		if (entry.startCluster == cluster) {
			result = dirIndex;
			return true;
		}
		return false;
	}
	const unsigned cluster;
	DirAsDSK::DirIndex& result;
};
bool DirAsDSK::getDirEntryForCluster(unsigned cluster,
                                     DirIndex& dirIndex, DirIndex& dirDirIndex)
{
	return scanMsxDirs(DirEntryForCluster(cluster, dirIndex, dirDirIndex),
	                   FIRST_DIR_SECTOR);
}
DirAsDSK::DirIndex DirAsDSK::getDirEntryForCluster(unsigned cluster)
{
	DirIndex dirIndex, dirDirIndex;
	if (getDirEntryForCluster(cluster, dirIndex, dirDirIndex)) {
		return dirIndex;
	} else {
		return DirIndex(unsigned(-1), unsigned(-1)); // not found
	}
}

// Remove the mapping between the msx and host for all the files/dirs in the
// given msx directory (+ subdirectories).
struct UnmapHostFiles : NullScanner {
	UnmapHostFiles(DirAsDSK::MapDirs& mapDirs_)
		: mapDirs(mapDirs_) {}
	bool onDirEntry(DirAsDSK::DirIndex dirIndex,
	                const MSXDirEntry& /*entry*/) {
		mapDirs.erase(dirIndex);
		return false;
	}
	DirAsDSK::MapDirs& mapDirs;
};
void DirAsDSK::unmapHostFiles(unsigned msxDirSector)
{
	scanMsxDirs(UnmapHostFiles(mapDirs), msxDirSector);
}

void DirAsDSK::exportToHost(DirIndex dirIndex, DirIndex dirDirIndex)
{
	// Handle both files and subdirectories.
	if (msxDir(dirIndex).attrib & MSXDirEntry::ATT_VOLUME) {
		// But ignore volume ID.
		return;
	}
	const char* msxName = msxDir(dirIndex).filename;
	string hostName;
	auto it = mapDirs.find(dirIndex);
	if (it == mapDirs.end()) {
		// Host file/dir does not yet exist, create hostname from
		// msx name.
		if ((msxName[0] == char(0x00)) || (msxName[0] == char(0xE5))) {
			// Invalid MSX name, don't do anything.
			return;
		}
		string hostSubDir;
		if (dirDirIndex.sector != 0) {
			// Not the msx root directory.
			auto it2 = mapDirs.find(dirDirIndex);
			assert(it2 != mapDirs.end());
			hostSubDir = it2->second.hostName;
			assert(!StringOp::endsWith(hostSubDir, '/'));
			hostSubDir += '/';
		}
		hostName = hostSubDir + msxToHostName(msxName);
		mapDirs[dirIndex].hostName = hostName;
	} else {
		// Hostname is already known.
		hostName = it->second.hostName;
	}
	if (msxDir(dirIndex).attrib & MSXDirEntry::ATT_DIRECTORY) {
		if ((memcmp(msxName, ".          ", 11) == 0) ||
		    (memcmp(msxName, "..         ", 11) == 0)) {
			// Don't export "." or "..".
			return;
		}
		exportToHostDir(dirIndex, hostName);
	} else {
		exportToHostFile(dirIndex, hostName);
	}
}

void DirAsDSK::exportToHostDir(DirIndex dirIndex, const string& hostName)
{
	try {
		unsigned cluster = msxDir(dirIndex).startCluster;
		if ((cluster < FIRST_CLUSTER) || (cluster >= MAX_CLUSTER)) {
			// Sanity check on cluster range.
			return;
		}
		unsigned msxDirSector = clusterToSector(cluster);

		// Create the host directory.
		string fullHostName = hostDir + hostName;
		FileOperations::mkdirp(fullHostName);

		// Export all the components in this directory.
		do {
			if (readFAT(sectorToCluster(msxDirSector)) == FREE_FAT) {
				// This happens e.g. on a TurboR when a directory
				// is removed: first the FAT is cleared before
				// the directory entry is updated.
				return;
			}
			for (unsigned idx = 0; idx < DIR_ENTRIES_PER_SECTOR; ++idx) {
				exportToHost(DirIndex(msxDirSector, idx), dirIndex);
			}
			msxDirSector = nextMsxDirSector(msxDirSector);
		} while (msxDirSector != unsigned(-1));
	} catch (FileException& e) {
		cliComm.printWarning("Error while syncing host directory: " +
			hostName + ": " + e.getMessage());
	}
}

void DirAsDSK::exportToHostFile(DirIndex dirIndex, const string& hostName)
{
	// We write a host file with length that is the minimum of:
	//  - Length indicated in msx directory entry.
	//  - Length of FAT-chain * cluster-size, this chain can have length=0
	//    if startCluster is not (yet) filled in.
	try {
		unsigned curCl = msxDir(dirIndex).startCluster;
		unsigned msxSize = msxDir(dirIndex).size;

		string fullHostName = hostDir + hostName;
		File file(fullHostName, File::TRUNCATE);
		unsigned offset = 0;
		while ((FIRST_CLUSTER <= curCl) && (curCl < MAX_CLUSTER)) {
			unsigned logicalSector = clusterToSector(curCl);
			for (unsigned i = 0; i < SECTORS_PER_CLUSTER; ++i) {
				if (offset >= msxSize) break;
				unsigned sector = logicalSector + i;
				assert(sector < NUM_SECTORS);
				auto writeSize = std::min<size_t>(msxSize - offset, SECTOR_SIZE);
				file.write(sectors[sector], writeSize);
				offset += SECTOR_SIZE;
			}
			if (offset >= msxSize) break;
			curCl = readFAT(curCl);
		}
	} catch (FileException& e) {
		cliComm.printWarning("Error while syncing host file: " +
			hostName + ": " + e.getMessage());
	}
}

void DirAsDSK::writeDIRSector(unsigned sector, DirIndex dirDirIndex,
                              const byte* buf)
{
	// Look for changed directory entries.
	for (unsigned idx = 0; idx < DIR_ENTRIES_PER_SECTOR; ++idx) {
		auto& newEntry = *reinterpret_cast<const MSXDirEntry*>(
			&buf[sizeof(MSXDirEntry) * idx]);
		DirIndex dirIndex(sector, idx);
		if (memcmp(&msxDir(dirIndex), &newEntry, sizeof(newEntry)) != 0) {
			writeDIREntry(dirIndex, dirDirIndex, newEntry);
		}
	}
	// At this point sector should be updated.
	assert(memcmp(sectors[sector], buf, SECTOR_SIZE) == 0);
}

void DirAsDSK::writeDIREntry(DirIndex dirIndex, DirIndex dirDirIndex,
                             const MSXDirEntry& newEntry)
{
	if (memcmp(msxDir(dirIndex).filename, newEntry.filename, 8 + 3) ||
	    ((msxDir(dirIndex).attrib & MSXDirEntry::ATT_DIRECTORY) !=
	     (        newEntry.attrib & MSXDirEntry::ATT_DIRECTORY))) {
		// Name or file-type in the direntry was changed.
		auto it = mapDirs.find(dirIndex);
		if (it != mapDirs.end()) {
			// If there is an associated hostfile, then delete it
			// (in case of a rename, the file will be recreated
			// below).
			string fullHostName = hostDir + it->second.hostName;
			FileOperations::deleteRecursive(fullHostName); // ignore return value
			// Remove mapping between msx and host file/dir.
			mapDirs.erase(it);
			if (msxDir(dirIndex).attrib & MSXDirEntry::ATT_DIRECTORY) {
				// In case of a directory also unmap all
				// sub-components.
				unsigned cluster = msxDir(dirIndex).startCluster;
				if ((FIRST_CLUSTER <= cluster) &&
				    (cluster < MAX_CLUSTER)) {
					unmapHostFiles(clusterToSector(cluster));
				}
			}
		}
	}

	// Copy the new msx directory entry.
	memcpy(&msxDir(dirIndex), &newEntry, sizeof(newEntry));

	// (Re-)export the full file/directory.
	exportToHost(dirIndex, dirDirIndex);
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
	DirIndex dirIndex = getDirEntryForCluster(startCluster);
	// no need to check for 'dirIndex.sector == unsigned(-1)'
	auto it = mapDirs.find(dirIndex);
	if (it == mapDirs.end()) {
		// This sector was not mapped to a file, nothing more to do.
		return;
	}

	// Actually write data to host file.
	string fullHostName = hostDir + it->second.hostName;
	try {
		File file(fullHostName, "rb+"); // don't uncompress
		file.seek(offset);
		unsigned msxSize = msxDir(dirIndex).size;
		if (msxSize > offset) {
			auto writeSize = std::min<size_t>(msxSize - offset, SECTOR_SIZE);
			file.write(buf, writeSize);
		}
	} catch (FileException& e) {
		cliComm.printWarning("Couldn't write to file " + fullHostName +
		                     ": " + e.getMessage());
	}
}

} // namespace openmsx
