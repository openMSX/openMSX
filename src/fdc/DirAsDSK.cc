#include "DirAsDSK.hh"

#include "BootBlocks.hh"
#include "CliComm.hh"
#include "DiskChanger.hh"
#include "File.hh"
#include "FileException.hh"
#include "ReadDir.hh"
#include "Scheduler.hh"

#include "StringOp.hh"
#include "narrow.hh"
#include "one_of.hh"
#include "ranges.hh"
#include "stl.hh"
#include "xrange.hh"

#include <algorithm>
#include <cassert>
#include <cstring>
#include <vector>

using std::string;
using std::vector;

namespace openmsx {

static constexpr unsigned SECTOR_SIZE = sizeof(SectorBuffer);
static constexpr unsigned SECTORS_PER_DIR = 7;
static constexpr unsigned NUM_FATS = 2;
static constexpr unsigned NUM_TRACKS = 80;
static constexpr unsigned SECTORS_PER_CLUSTER = 2;
static constexpr unsigned SECTORS_PER_TRACK = 9;
static constexpr unsigned FIRST_FAT_SECTOR = 1;
static constexpr unsigned DIR_ENTRIES_PER_SECTOR =
	SECTOR_SIZE / sizeof(MSXDirEntry);

// First valid regular cluster number.
static constexpr unsigned FIRST_CLUSTER = 2;

static constexpr unsigned FREE_FAT = 0x000;
static constexpr unsigned BAD_FAT  = 0xFF7;
static constexpr unsigned EOF_FAT  = 0xFFF; // actually 0xFF8-0xFFF


// Transform BAD_FAT (0xFF7) and EOF_FAT-range (0xFF8-0xFFF)
// to a single value: EOF_FAT (0xFFF).
[[nodiscard]] static constexpr unsigned normalizeFAT(unsigned cluster)
{
	return (cluster < BAD_FAT) ? cluster : EOF_FAT;
}

unsigned DirAsDSK::readFATHelper(std::span<const SectorBuffer> fatBuf, unsigned cluster) const
{
	assert(FIRST_CLUSTER <= cluster);
	assert(cluster < maxCluster);
	std::span buf{fatBuf[0].raw.data(), fatBuf.size() * SECTOR_SIZE};
	auto p = subspan<2>(buf, (cluster * 3) / 2);
	unsigned result = (cluster & 1)
	                ? (p[0] >> 4) + (p[1] << 4)
	                : p[0] + ((p[1] & 0x0F) << 8);
	return normalizeFAT(result);
}

void DirAsDSK::writeFATHelper(std::span<SectorBuffer> fatBuf, unsigned cluster, unsigned val) const
{
	assert(FIRST_CLUSTER <= cluster);
	assert(cluster < maxCluster);
	std::span buf{fatBuf[0].raw.data(), fatBuf.size() * SECTOR_SIZE};
	auto p = subspan<2>(buf, (cluster * 3) / 2);
	if (cluster & 1) {
		p[0] = narrow_cast<uint8_t>((p[0] & 0x0F) + (val << 4));
		p[1] = narrow_cast<uint8_t>(val >> 4);
	} else {
		p[0] = narrow_cast<uint8_t>(val);
		p[1] = narrow_cast<uint8_t>((p[1] & 0xF0) + ((val >> 8) & 0x0F));
	}
}

std::span<SectorBuffer> DirAsDSK::fat()
{
	return {&sectors[FIRST_FAT_SECTOR], nofSectorsPerFat};
}
std::span<SectorBuffer> DirAsDSK::fat2()
{
	return {&sectors[firstSector2ndFAT], nofSectorsPerFat};
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

// Returns maxCluster in case of no more free clusters
unsigned DirAsDSK::findNextFreeCluster(unsigned cluster)
{
	assert(cluster < maxCluster);
	do {
		++cluster;
		assert(cluster >= FIRST_CLUSTER);
	} while ((cluster < maxCluster) && (readFAT(cluster) != FREE_FAT));
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
	if (cluster == maxCluster) {
		throw MSXException("disk full");
	}
	return cluster;
}

unsigned DirAsDSK::clusterToSector(unsigned cluster) const
{
	assert(cluster >= FIRST_CLUSTER);
	assert(cluster < maxCluster);
	return firstDataSector + SECTORS_PER_CLUSTER *
	            (cluster - FIRST_CLUSTER);
}

std::pair<unsigned, unsigned> DirAsDSK::sectorToClusterOffset(unsigned sector) const
{
	assert(sector >= firstDataSector);
	assert(sector < nofSectors);
	sector -= firstDataSector;
	unsigned cluster = (sector / SECTORS_PER_CLUSTER) + FIRST_CLUSTER;
	unsigned offset  = (sector % SECTORS_PER_CLUSTER) * SECTOR_SIZE;
	return {cluster, offset};
}
unsigned DirAsDSK::sectorToCluster(unsigned sector) const
{
	auto [cluster, offset] = sectorToClusterOffset(sector);
	return cluster;
}

MSXDirEntry& DirAsDSK::msxDir(DirIndex dirIndex)
{
	assert(dirIndex.sector < nofSectors);
	assert(dirIndex.idx    < DIR_ENTRIES_PER_SECTOR);
	return sectors[dirIndex.sector].dirEntry[dirIndex.idx];
}

// Returns -1 when there are no more sectors for this directory.
unsigned DirAsDSK::nextMsxDirSector(unsigned sector)
{
	if (sector < firstDataSector) {
		// Root directory.
		assert(firstDirSector <= sector);
		++sector;
		if (sector == firstDataSector) {
			// Root directory has a fixed number of sectors.
			return unsigned(-1);
		}
		return sector;
	} else {
		// Subdirectory.
		auto [cluster, offset] = sectorToClusterOffset(sector);
		if (offset < ((SECTORS_PER_CLUSTER - 1) * SECTOR_SIZE)) {
			// Next sector still in same cluster.
			return sector + 1;
		}
		unsigned nextCl = readFAT(cluster);
		if ((nextCl < FIRST_CLUSTER) || (maxCluster <= nextCl)) {
			// No next cluster, end of directory reached.
			return unsigned(-1);
		}
		return clusterToSector(nextCl);
	}
}

// Check if a msx filename is used in a specific msx (sub)directory.
bool DirAsDSK::checkMSXFileExists(
	std::span<const char, 11> msxFilename, unsigned msxDirSector)
{
	vector<bool> visited(nofSectors, false);
	do {
		if (visited[msxDirSector]) {
			// cycle detected, invalid disk, but don't crash on it
			return false;
		}
		visited[msxDirSector] = true;

		for (auto idx : xrange(DIR_ENTRIES_PER_SECTOR)) {
			DirIndex dirIndex(msxDirSector, idx);
			if (std::ranges::equal(msxDir(dirIndex).filename, msxFilename)) {
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
DirAsDSK::DirIndex DirAsDSK::findHostFileInDSK(std::string_view hostName) const
{
	for (const auto& [dirIdx, mapDir] : mapDirs) {
		if (mapDir.hostName == hostName) {
			return dirIdx;
		}
	}
	return {unsigned(-1), unsigned(-1)};
}

// Check if a host file is already mapped in the virtual disk.
bool DirAsDSK::checkFileUsedInDSK(std::string_view hostName) const
{
	DirIndex dirIndex = findHostFileInDSK(hostName);
	return dirIndex.sector != unsigned(-1);
}

static std::array<char, 11> hostToMsxName(string hostName)
{
	// Create an MSX filename 8.3 format. TODO use vfat-like abbreviation
	transform_in_place(hostName, [](char a) {
		return (a == ' ') ? '_' : ::toupper(a);
	});
	auto [file, ext] = StringOp::splitOnLast(hostName, '.');
	if (file.empty()) std::swap(file, ext);

	std::array<char, 8 + 3> result;
	ranges::fill(result, ' ');
	ranges::copy(subspan(file, 0, std::min<size_t>(8, file.size())), subspan<8>(result, 0));
	ranges::copy(subspan(ext,  0, std::min<size_t>(3, ext .size())), subspan<3>(result, 8));
	std::ranges::replace(result, '.', '_');
	return result;
}

static string msxToHostName(std::span<const char, 11> msxName)
{
	string result;
	for (unsigned i = 0; (i < 8) && (msxName[i] != ' '); ++i) {
		result += char(tolower(msxName[i]));
	}
	if (msxName[8] != ' ') {
		result += '.';
		for (unsigned i = 8; (i < (8 + 3)) && (msxName[i] != ' '); ++i) {
			result += char(tolower(msxName[i]));
		}
	}
	return result;
}


DirAsDSK::DirAsDSK(DiskChanger& diskChanger_, CliComm& cliComm_,
                   const Filename& hostDir_, SyncMode syncMode_,
                   BootSectorType bootSectorType)
	: SectorBasedDisk(DiskName(hostDir_))
	, diskChanger(diskChanger_)
	, cliComm(cliComm_)
	, hostDir(FileOperations::expandTilde(hostDir_.getResolved() + '/'))
	, syncMode(syncMode_)
	, nofSectors((diskChanger_.isDoubleSidedDrive() ? 2 : 1) * SECTORS_PER_TRACK * NUM_TRACKS)
	, nofSectorsPerFat(narrow<unsigned>((((3 * nofSectors) / (2 * SECTORS_PER_CLUSTER)) + SECTOR_SIZE - 1) / SECTOR_SIZE))
	, firstSector2ndFAT(FIRST_FAT_SECTOR + nofSectorsPerFat)
	, firstDirSector(FIRST_FAT_SECTOR + NUM_FATS * nofSectorsPerFat)
	, firstDataSector(firstDirSector + SECTORS_PER_DIR)
	, maxCluster((nofSectors - firstDataSector) / SECTORS_PER_CLUSTER + FIRST_CLUSTER)
	, sectors(nofSectors)
{
	if (!FileOperations::isDirectory(hostDir)) {
		throw MSXException("Not a directory");
	}

	// First create structure for the virtual disk.
	uint8_t numSides = diskChanger_.isDoubleSidedDrive() ? 2 : 1;
	setNbSectors(nofSectors);
	setSectorsPerTrack(SECTORS_PER_TRACK);
	setNbSides(numSides);

	// Initially the whole disk is filled with 0xE5 (at least on Philips
	// NMS8250).
	ranges::fill(std::span{sectors[0].raw.data(), sizeof(SectorBuffer) * nofSectors}, 0xE5);

	// Use selected boot sector, fill-in values.
	uint8_t mediaDescriptor = (numSides == 2) ? 0xF9 : 0xF8;
	const auto& protoBootSector = bootSectorType == BootSectorType::DOS1
		? BootBlocks::dos1BootBlock
		: BootBlocks::dos2BootBlock;
	sectors[0] = protoBootSector;
	auto& bootSector = sectors[0].bootSector;
	bootSector.bpSector     = SECTOR_SIZE;
	bootSector.spCluster    = SECTORS_PER_CLUSTER;
	bootSector.nrFats       = NUM_FATS;
	bootSector.dirEntries   = SECTORS_PER_DIR * (SECTOR_SIZE / sizeof(MSXDirEntry));
	bootSector.nrSectors    = narrow_cast<uint16_t>(nofSectors);
	bootSector.descriptor   = mediaDescriptor;
	bootSector.sectorsFat   = narrow_cast<uint16_t>(nofSectorsPerFat);
	bootSector.sectorsTrack = SECTORS_PER_TRACK;
	bootSector.nrSides      = numSides;

	// Clear FAT1 + FAT2.
	ranges::fill(std::span{fat()[0].raw.data(), SECTOR_SIZE * nofSectorsPerFat * NUM_FATS}, 0);
	// First 3 bytes are initialized specially:
	//  'cluster 0' contains the media descriptor
	//  'cluster 1' is marked as EOF_FAT
	//  So cluster 2 is the first usable cluster number
	auto init = [&](auto f) {
		f[0].raw[0] = mediaDescriptor;
		f[0].raw[1] = 0xFF;
		f[0].raw[2] = 0xFF;
	};
	init(fat());
	init(fat2());

	// Assign empty directory entries.
	ranges::fill(std::span{sectors[firstDirSector].raw.data(), SECTOR_SIZE * SECTORS_PER_DIR}, 0);

	// No host files are mapped to this disk yet.
	assert(mapDirs.empty());

	// Import the host filesystem.
	syncWithHost();
}

bool DirAsDSK::isWriteProtectedImpl() const
{
	return syncMode == SyncMode::READONLY;
}

bool DirAsDSK::hasChanged() const
{
	// For simplicity, for now, always report content has (possibly) changed
	// (in the future we could try to detect that no host files have
	// actually changed).
	// The effect is that the MSX always see a 'disk-changed-signal'.
	// This fixes: https://github.com/openMSX/openMSX/issues/1410
	return true;
}

void DirAsDSK::checkCaches()
{
	bool needSync = [&] {
		if (const auto* scheduler = diskChanger.getScheduler()) {
			auto now = scheduler->getCurrentTime();
			auto delta = now - lastAccess;
			return delta > EmuDuration::sec(1);
			// Do not update lastAccess because we don't actually call
			// syncWithHost().
		} else {
			// Happens when dirasdisk is used in virtual_drive.
			return true;
		}
	}();
	if (needSync) {
		flushCaches();
	}
}

void DirAsDSK::readSectorImpl(size_t sector, SectorBuffer& buf)
{
	assert(sector < nofSectors);

	// 'Peek-mode' is used to periodically calculate a sha1sum for the
	// whole disk (used by reverse). We don't want this calculation to
	// interfere with the access time we use for normal read/writes. So in
	// peek-mode we skip the whole sync-step.
	if (!isPeekMode()) {
		bool needSync = [&] {
			if (const auto* scheduler = diskChanger.getScheduler()) {
				auto now = scheduler->getCurrentTime();
				auto delta = now - lastAccess;
				lastAccess = now;
				return delta > EmuDuration::sec(1);
			} else {
				// Happens when dirasdisk is used in virtual_drive.
				return true;
			}
		}();
		if (needSync) {
			syncWithHost();
			flushCaches(); // e.g. sha1sum
			// Let the disk drive report the disk has been ejected.
			// E.g. a turbor machine uses this to flush its
			// internal disk caches.
			diskChanger.forceDiskChange(); // maybe redundant now? (see hasChanged()).
		}
	}

	// Simply return the sector from our virtual disk image.
	buf = sectors[sector];
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
	addNewHostFiles({}, firstDirSector);
}

void DirAsDSK::checkDeletedHostFiles()
{
	// This handles both host files and directories.
	auto copy = mapDirs;
	for (const auto& [dirIdx, mapDir] : copy) {
		if (!mapDirs.contains(dirIdx)) {
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
		auto fullHostName = tmpStrCat(hostDir, mapDir.hostName);
		auto isMSXDirectory = bool(msxDir(dirIdx).attrib &
		                           MSXDirEntry::Attrib::DIRECTORY);
		auto fst = FileOperations::getStat(fullHostName);
		if (!fst || (FileOperations::isDirectory(*fst) != isMSXDirectory)) {
			// TODO also check access permission
			// Error stat-ing file, or directory/file type is not
			// the same on the msx and host side (e.g. a host file
			// has been removed and a host directory with the same
			// name has been created). In both cases delete the msx
			// entry (if needed it will be recreated soon).
			deleteMSXFile(dirIdx);
		}
	}
}

void DirAsDSK::deleteMSXFile(DirIndex dirIndex)
{
	// Remove mapping between host and msx file (if any).
	mapDirs.erase(dirIndex);

	if (msxDir(dirIndex).filename[0] == one_of(0, char(0xE5))) {
		// Directory entry not in use, don't need to do anything.
		return;
	}

	if (msxDir(dirIndex).attrib & MSXDirEntry::Attrib::DIRECTORY) {
		// If we're deleting a directory then also (recursively)
		// delete the files/directories in this directory.
		if (const auto& msxName = msxDir(dirIndex).filename;
		    std::ranges::equal(msxName, std::string_view(".          ")) ||
		    std::ranges::equal(msxName, std::string_view("..         "))) {
			// But skip the "." and ".." entries.
			return;
		}
		// Sanity check on cluster range.
		unsigned cluster = msxDir(dirIndex).startCluster;
		if ((FIRST_CLUSTER <= cluster) && (cluster < maxCluster)) {
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
	vector<bool> visited(nofSectors, false);
	do {
		if (visited[msxDirSector]) {
			// cycle detected, invalid disk, but don't crash on it
			return;
		}
		visited[msxDirSector] = true;

		for (auto idx : xrange(DIR_ENTRIES_PER_SECTOR)) {
			deleteMSXFile(DirIndex(msxDirSector, idx));
		}
		msxDirSector = nextMsxDirSector(msxDirSector);
	} while (msxDirSector != unsigned(-1));
}

void DirAsDSK::freeFATChain(unsigned cluster)
{
	// Follow a FAT chain and mark all clusters on this chain as free.
	while ((FIRST_CLUSTER <= cluster) && (cluster < maxCluster)) {
		unsigned nextCl = readFAT(cluster);
		writeFAT12(cluster, FREE_FAT);
		cluster = nextCl;
	}
}

void DirAsDSK::checkModifiedHostFiles()
{
	auto copy = mapDirs;
	for (const auto& [dirIdx, mapDir] : copy) {
		if (!mapDirs.contains(dirIdx)) {
			// See comment in checkDeletedHostFiles().
			continue;
		}
		auto fullHostName = tmpStrCat(hostDir, mapDir.hostName);
		auto isMSXDirectory = bool(msxDir(dirIdx).attrib &
		                           MSXDirEntry::Attrib::DIRECTORY);
		auto fst = FileOperations::getStat(fullHostName);
		if (fst && (FileOperations::isDirectory(*fst) == isMSXDirectory)) {
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
			    ((mapDir.mtime    != fst->st_mtime) ||
			     (mapDir.filesize != size_t(fst->st_size)))) {
				importHostFile(dirIdx, *fst);
			}
		} else {
			// Only very rarely happens (because checkDeletedHostFiles()
			// checked this just recently).
			deleteMSXFile(dirIdx);
		}
	}
}

void DirAsDSK::importHostFile(DirIndex dirIndex, const FileOperations::Stat& fst)
{
	assert(!(msxDir(dirIndex).attrib & MSXDirEntry::Attrib::DIRECTORY));
	assert(mapDirs.contains(dirIndex));

	// Set _msx_ modification time.
	setMSXTimeStamp(dirIndex, fst);

	// Set _host_ modification time (and filesize)
	// Note: this is the _only_ place where we update the mapDir.mtime
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
	// or curCl >= maxCluster).
	if ((curCl < FIRST_CLUSTER) || (curCl >= maxCluster)) {
		moreClustersInChain = false;
		curCl = findFirstFreeCluster(); // maxCluster in case of disk-full
	}

	auto remainingSize = hostSize;
	unsigned prevCl = 0;
	try {
		File file(hostDir + mapDir.hostName,
		          "rb"); // don't uncompress

		while (remainingSize && (curCl < maxCluster)) {
			unsigned logicalSector = clusterToSector(curCl);
			for (auto i : xrange(SECTORS_PER_CLUSTER)) {
				unsigned sector = logicalSector + i;
				assert(sector < nofSectors);
				auto* buf = &sectors[sector];
				auto sz = std::min(remainingSize, SECTOR_SIZE);
				file.read(subspan(buf->raw, 0, sz));
				ranges::fill(subspan(buf->raw, sz), 0); // in case (end of) file only fills partial sector
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
				msxDir(dirIndex).startCluster = narrow<uint16_t>(curCl);
			}
			prevCl = curCl;

			// Check if we can follow the existing FAT chain or
			// need to allocate a free cluster.
			if (moreClustersInChain) {
				curCl = readFAT(curCl);
				if ((curCl == EOF_FAT)      || // normal end
				    (curCl < FIRST_CLUSTER) || // invalid
				    (curCl >= maxCluster)) {  // invalid
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
			cliComm.printWarning("Virtual disk image full: ",
			                     mapDir.hostName, " truncated.");
		}
	} catch (FileException& e) {
		// Error opening or reading host file.
		cliComm.printWarning("Error reading host file: ",
		                     mapDir.hostName, ": ", e.getMessage(),
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

void DirAsDSK::setMSXTimeStamp(DirIndex dirIndex, const FileOperations::Stat& fst)
{
	// Use intermediate param to prevent compilation error for Android
	time_t mtime = fst.st_mtime;
	const auto* mtim = localtime(&mtime);
	int t1 = mtim ? (mtim->tm_sec >> 1) + (mtim->tm_min << 5) +
	                (mtim->tm_hour << 11)
	              : 0;
	msxDir(dirIndex).time = narrow<uint16_t>(t1);
	int t2 = mtim ? mtim->tm_mday + ((mtim->tm_mon + 1) << 5) +
	                ((mtim->tm_year + 1900 - 1980) << 9)
	              : 0;
	msxDir(dirIndex).date = narrow<uint16_t>(t2);
}

// Used to add 'regular' files before 'derived' files. E.g. when editing a file
// in a host editor, you often get backup/swap files like this:
//   myfile.txt  myfile.txt~  .myfile.txt.swp
// Currently the 1st and 2nd are mapped to the same MSX filename. If more
// host files map to the same MSX file then (currently) one of the two is
// ignored. Which one is ignored depends on the order in which they are added
// to the virtual disk. This routine/heuristic tries to add 'regular' files
// before derived files.
static size_t weight(const string& hostName)
{
	// TODO this weight function can most likely be improved
	size_t result = 0;
	auto [file, ext] = StringOp::splitOnLast(hostName, '.');
	// too many '.' characters
	result += std::ranges::count(file, '.') * 100;
	// too long extension
	result += ext.size() * 10;
	// too long file
	result += file.size();
	return result;
}

void DirAsDSK::addNewHostFiles(const string& hostSubDir, unsigned msxDirSector)
{
	assert(!hostSubDir.starts_with('/'));
	assert(hostSubDir.empty() || hostSubDir.ends_with('/'));

	vector<string> hostNames;
	{
		ReadDir dir(tmpStrCat(hostDir, hostSubDir));
		while (auto* d = dir.getEntry()) {
			hostNames.emplace_back(d->d_name);
		}
	}
	ranges::sort(hostNames, {}, [](const string& n) { return weight(n); });

	for (auto& hostName : hostNames) {
		try {
			if (hostName.starts_with('.')) {
				// skip '.' and '..'
				// also skip hidden files on unix
				continue;
			}
			auto fullHostName = tmpStrCat(hostDir, hostSubDir, hostName);
			auto fst = FileOperations::getStat(fullHostName);
			if (!fst) {
				throw MSXException("Error accessing ", fullHostName);
			}
			if (FileOperations::isDirectory(*fst)) {
				addNewDirectory(hostSubDir, hostName, msxDirSector, *fst);
			} else if (FileOperations::isRegularFile(*fst)) {
				addNewHostFile(hostSubDir, hostName, msxDirSector, *fst);
			} else {
				throw MSXException("Not a regular file: ", fullHostName);
			}
		} catch (MSXException& e) {
			cliComm.printWarning(e.getMessage());
		}
	}
}

void DirAsDSK::addNewDirectory(const string& hostSubDir, const string& hostName,
                               unsigned msxDirSector, const FileOperations::Stat& fst)
{
	DirIndex dirIndex = findHostFileInDSK(tmpStrCat(hostSubDir, hostName));
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
		msxDir(dirIndex).attrib = MSXDirEntry::Attrib::DIRECTORY;
		msxDir(dirIndex).startCluster = narrow<uint16_t>(cluster);

		// Initialize the new directory.
		newMsxDirSector = clusterToSector(cluster);
		ranges::fill(std::span{sectors[newMsxDirSector].raw.data(),
		                       SECTORS_PER_CLUSTER * SECTOR_SIZE},
			     0);
		DirIndex idx0(newMsxDirSector, 0); // entry for "."
		DirIndex idx1(newMsxDirSector, 1); //           ".."
		auto& e0 = msxDir(idx0);
		auto& e1 = msxDir(idx1);
		auto& f0 = e0.filename;
		auto& f1 = e1.filename;
		f0[0] = '.';              ranges::fill(subspan(f0, 1), ' ');
		f1[0] = '.'; f1[1] = '.'; ranges::fill(subspan(f1, 2), ' ');
		e0.attrib = MSXDirEntry::Attrib::DIRECTORY;
		e1.attrib = MSXDirEntry::Attrib::DIRECTORY;
		setMSXTimeStamp(idx0, fst);
		setMSXTimeStamp(idx1, fst);
		e0.startCluster = narrow<uint16_t>(cluster);
		e1.startCluster = msxDirSector == firstDirSector
		                ? uint16_t(0)
		                : narrow<uint16_t>(sectorToCluster(msxDirSector));
	} else {
		if (!(msxDir(dirIndex).attrib & MSXDirEntry::Attrib::DIRECTORY)) {
			// Should rarely happen because checkDeletedHostFiles()
			// recently checked this. (It could happen when a host
			// directory is *just*recently* created with the same
			// name as an existing msx file). Ignore, it will be
			// corrected in the next sync.
			return;
		}
		unsigned cluster = msxDir(dirIndex).startCluster;
		if ((cluster < FIRST_CLUSTER) || (cluster >= maxCluster)) {
			// Sanity check on cluster range.
			return;
		}
		newMsxDirSector = clusterToSector(cluster);
	}

	// Recursively process this directory.
	addNewHostFiles(strCat(hostSubDir, hostName, '/'), newMsxDirSector);
}

void DirAsDSK::addNewHostFile(const string& hostSubDir, const string& hostName,
                              unsigned msxDirSector, const FileOperations::Stat& fst)
{
	if (checkFileUsedInDSK(tmpStrCat(hostSubDir, hostName))) {
		// File is already present in the virtual disk, do nothing.
		return;
	}
	// TODO check for available free space on disk instead of max free space
	if (auto diskSpace = (nofSectors - firstDataSector) * SECTOR_SIZE;
	    narrow<size_t>(fst.st_size) > diskSpace) {
		cliComm.printWarning("File too large: ",
		                     hostDir, hostSubDir, hostName);
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
		auto msxFilename = hostToMsxName(hostName);
		if (checkMSXFileExists(msxFilename, msxDirSector)) {
			// TODO: actually should increase vfat abbreviation if possible!!
			throw MSXException(
				"MSX name ", msxToHostName(msxFilename),
				" already exists");
		}

		// Fill in hostName / msx filename.
		assert(!hostPath.ends_with('/'));
		mapDirs[dirIndex].hostName = hostPath;
		memset(&msxDir(dirIndex), 0, sizeof(MSXDirEntry)); // clear entry
		ranges::copy(msxFilename, msxDir(dirIndex).filename);
		return dirIndex;
	} catch (MSXException& e) {
		throw MSXException("Couldn't add ", hostPath, ": ",
		                   e.getMessage());
	}
}

DirAsDSK::DirIndex DirAsDSK::getFreeDirEntry(unsigned msxDirSector)
{
	vector<bool> visited(nofSectors, false);
	while (true) {
		if (visited[msxDirSector]) {
			// cycle detected, invalid disk, but don't crash on it
			throw MSXException("cycle in FAT");
		}
		visited[msxDirSector] = true;

		for (auto idx : xrange(DIR_ENTRIES_PER_SECTOR)) {
			DirIndex dirIndex(msxDirSector, idx);
			const auto& msxName = msxDir(dirIndex).filename;
			if (msxName[0] == one_of(char(0x00), char(0xE5))) {
				// Found an unused msx entry. There shouldn't
				// be any host file mapped to this entry.
				assert(!mapDirs.contains(dirIndex));
				return dirIndex;
			}
		}
		unsigned sector = nextMsxDirSector(msxDirSector);
		if (sector == unsigned(-1)) break;
		msxDirSector = sector;
	}

	// No free space in existing directory.
	if (msxDirSector == (firstDataSector - 1)) {
		// Can't extend root directory.
		throw MSXException("root directory full");
	}

	// Extend sub-directory: allocate and clear a new cluster, add this
	// cluster in the existing FAT chain.
	unsigned cluster = sectorToCluster(msxDirSector);
	unsigned newCluster = getFreeCluster(); // throws if disk full
	unsigned sector = clusterToSector(newCluster);
	ranges::fill(std::span{sectors[sector].raw.data(), SECTORS_PER_CLUSTER * SECTOR_SIZE}, 0);
	writeFAT12(cluster, newCluster);
	writeFAT12(newCluster, EOF_FAT);

	// First entry in this newly allocated cluster is free. Return it.
	return {sector, 0};
}

void DirAsDSK::writeSectorImpl(size_t sector_, const SectorBuffer& buf)
{
	assert(sector_ < nofSectors);
	assert(syncMode != SyncMode::READONLY);
	auto sector = unsigned(sector_);

	// Update last access time.
	if (const auto* scheduler = diskChanger.getScheduler()) {
		lastAccess = scheduler->getCurrentTime();
	}

	if (sector == 0) {
		// Ignore. We don't allow writing to the boot sector. It would
		// be very bad if the MSX tried to format this disk using other
		// disk parameters than this code assumes. It's also not useful
		// to write a different boot program to this disk because it
		// will be lost when this virtual disk is ejected.
	} else if (sector < firstSector2ndFAT) {
		writeFATSector(sector, buf);
	} else if (sector < firstDirSector) {
		// Write to 2nd FAT, only buffer it. Don't interpret the data
		// in FAT2 in any way (nor trigger any action on this write).
		sectors[sector] = buf;
	} else if (auto dirDirIndex = isDirSector(sector)) {
		// Either root- or sub-directory.
		writeDIRSector(sector, *dirDirIndex, buf);
	} else {
		writeDataSector(sector, buf);
	}
}

void DirAsDSK::writeFATSector(unsigned sector, const SectorBuffer& buf)
{
	// Create copy of old FAT (to be able to detect changes).
	auto oldFAT = to_vector(fat());

	// Update current FAT with new data.
	sectors[sector] = buf;

	// Look for changes.
	for (auto i : xrange(FIRST_CLUSTER, maxCluster)) {
		if (readFAT(i) != readFATHelper(oldFAT, i)) {
			exportFileFromFATChange(i, oldFAT);
		}
	}
	// At this point there should be no more differences.
	// Note: we can't use
	//   assert(std::ranges::equal(fat(), oldFAT));
	// because exportFileFromFATChange() only updates the part of the FAT
	// that actually contains FAT info. E.g. not the media ID at the
	// beginning nor the unused part at the end. And for example the 'CALL
	// FORMAT' routine also writes these parts of the FAT.
	for (auto i : xrange(FIRST_CLUSTER, maxCluster)) {
		assert(readFAT(i) == readFATHelper(oldFAT, i)); (void)i;
	}
}

void DirAsDSK::exportFileFromFATChange(unsigned cluster, std::span<SectorBuffer> oldFAT)
{
	// Get first cluster in the FAT chain that contains 'cluster'.
	auto [startCluster, chainLength] = getChainStart(cluster);

	// Copy this whole chain from FAT1 to FAT2 (so that the loop in
	// writeFATSector() sees this part is already handled).
	vector<bool> visited(maxCluster, false);
	unsigned tmp = startCluster;
	while ((FIRST_CLUSTER <= tmp) && (tmp < maxCluster)) {
		if (visited[tmp]) {
			// detected cycle, don't export file
			return;
		}
		visited[tmp] = true;

		unsigned next = readFAT(tmp);
		writeFATHelper(oldFAT, tmp, next);
		tmp = next;
	}

	// Find the corresponding direntry and (if found) export file based on
	// new cluster chain.
	if (auto result = getDirEntryForCluster(startCluster)) {
		exportToHost(result->dirIndex, result->dirDirIndex);
	}
}

std::pair<unsigned, unsigned> DirAsDSK::getChainStart(unsigned cluster)
{
	// Search for the first cluster in the chain that contains 'cluster'
	// Note: worst case (this implementation of) the search is O(N^2), but
	// because usually FAT chains are allocated in ascending order, this
	// search is fast O(N).
	unsigned chainLength = 0;
	for (unsigned i = FIRST_CLUSTER; i < maxCluster; ++i) { // note: i changed in loop!
		if (readFAT(i) == cluster) {
			// Found a predecessor.
			cluster = i;
			++chainLength;
			i = FIRST_CLUSTER - 1; // restart search
		}
	}
	return {cluster, chainLength};
}

// Generic helper function that walks over the whole MSX directory tree
// (possibly it stops early so it doesn't always walk over the whole tree).
// The action that is performed while walking depends on the functor parameter.
template<typename FUNC> bool DirAsDSK::scanMsxDirs(FUNC&& func, unsigned sector)
{
	size_t rdIdx = 0;
	vector<unsigned> dirs;  // TODO make vector of struct instead of
	vector<DirIndex> dirs2; //      2 parallel vectors.
	while (true) {
		do {
			// About to process a new directory sector.
			if (func.onDirSector(sector)) return true;

			for (auto idx : xrange(DIR_ENTRIES_PER_SECTOR)) {
				// About to process a new directory entry.
				DirIndex dirIndex(sector, idx);
				const MSXDirEntry& entry = msxDir(dirIndex);
				if (func.onDirEntry(dirIndex, entry)) return true;

				if ((entry.filename[0] == one_of(char(0x00), char(0xE5))) ||
				    !(entry.attrib & MSXDirEntry::Attrib::DIRECTORY)) {
					// Not a directory.
					continue;
				}
				unsigned cluster = msxDir(dirIndex).startCluster;
				if ((cluster < FIRST_CLUSTER) ||
				    (cluster >= maxCluster)) {
					// Cluster=0 happens for ".." entries to
					// the root directory, also be robust for
					// bogus data.
					continue;
				}
				unsigned dir = clusterToSector(cluster);
				if (contains(dirs, dir)) {
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
	void onVisitSubDir(DirAsDSK::DirIndex /*subdir*/) const {}

	// Called when a new sector of a (sub)directory is being scanned.
	[[nodiscard]] bool onDirSector(unsigned /*dirSector*/) const {
		return false;
	}

	// Called for each directory entry (in a sector).
	[[nodiscard]] bool onDirEntry(DirAsDSK::DirIndex /*dirIndex*/,
	                              const MSXDirEntry& /*entry*/) const {
		return false;
	}
};

// Base class for the IsDirSector and DirEntryForCluster scanner algorithms
// below. This class remembers the directory entry of the last visited subdir.
struct DirScanner : NullScanner {
	// Called right before we enter a new subdirectory.
	void onVisitSubDir(DirAsDSK::DirIndex subdir) {
		dirDirIndex = subdir;
	}

	DirAsDSK::DirIndex dirDirIndex{0, 0}; // entry for root dir
};

// Figure out whether a given sector is part of the msx directory structure.
struct IsDirSector : DirScanner {
	explicit IsDirSector(unsigned sector_) : sector(sector_) {}

	[[nodiscard]] bool onDirSector(unsigned dirSector) const {
		return sector == dirSector;
	}

	unsigned sector;
};
std::optional<DirAsDSK::DirIndex> DirAsDSK::isDirSector(unsigned sector)
{
	if (IsDirSector scanner{sector};
	    scanMsxDirs(scanner, firstDirSector)) {
		return scanner.dirDirIndex;
	}
	return std::nullopt;
}

// Search for the directory entry that has the given startCluster.
struct DirEntryForCluster : DirScanner {
	explicit DirEntryForCluster(unsigned cluster_) : cluster(cluster_) {}

	bool onDirEntry(DirAsDSK::DirIndex dirIndex_, const MSXDirEntry& entry) {
		if (entry.startCluster == cluster) {
			dirIndex = dirIndex_;
			return true;
		}
		return false;
	}

	unsigned cluster;
	DirAsDSK::DirIndex dirIndex{0, 0}; // dummy initial value, not used
};
std::optional<DirAsDSK::DirEntryForClusterResult> DirAsDSK::getDirEntryForCluster(unsigned cluster)
{
	if (DirEntryForCluster scanner{cluster};
	    scanMsxDirs(scanner, firstDirSector)) {
		return DirEntryForClusterResult{scanner.dirIndex, scanner.dirDirIndex};
	}
	return std::nullopt;
}

// Remove the mapping between the msx and host for all the files/dirs in the
// given msx directory (+ subdirectories).
struct UnmapHostFiles : NullScanner {
	explicit UnmapHostFiles(DirAsDSK::MapDirs& mapDirs_)
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
	if (msxDir(dirIndex).attrib & MSXDirEntry::Attrib::VOLUME) {
		// But ignore volume ID.
		return;
	}
	const auto& msxName = msxDir(dirIndex).filename;
	string hostName;
	if (const auto* v = lookup(mapDirs, dirIndex)) {
		// Hostname is already known.
		hostName = v->hostName;
	} else {
		// Host file/dir does not yet exist, create hostname from
		// msx name.
		if (msxName[0] == one_of(char(0x00), char(0xE5))) {
			// Invalid MSX name, don't do anything.
			return;
		}
		string hostSubDir;
		if (dirDirIndex.sector != 0) {
			// Not the msx root directory.
			const auto* v2 = lookup(mapDirs, dirDirIndex);
			assert(v2);
			hostSubDir = v2->hostName;
			assert(!hostSubDir.ends_with('/'));
			hostSubDir += '/';
		}
		hostName = hostSubDir + msxToHostName(msxName);
		mapDirs[dirIndex].hostName = hostName;
	}
	if (msxDir(dirIndex).attrib & MSXDirEntry::Attrib::DIRECTORY) {
		if (std::ranges::equal(msxName, std::string_view(".          ")) ||
		    std::ranges::equal(msxName, std::string_view("..         "))) {
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
		if ((cluster < FIRST_CLUSTER) || (cluster >= maxCluster)) {
			// Sanity check on cluster range.
			return;
		}
		unsigned msxDirSector = clusterToSector(cluster);

		// Create the host directory.
		FileOperations::mkdirp(hostDir + hostName);

		// Export all the components in this directory.
		vector<bool> visited(nofSectors, false);
		do {
			if (visited[msxDirSector]) {
				// detected cycle, invalid disk, but don't crash on it
				return;
			}
			visited[msxDirSector] = true;

			if (readFAT(sectorToCluster(msxDirSector)) == FREE_FAT) {
				// This happens e.g. on a TurboR when a directory
				// is removed: first the FAT is cleared before
				// the directory entry is updated.
				return;
			}
			for (auto idx : xrange(DIR_ENTRIES_PER_SECTOR)) {
				exportToHost(DirIndex(msxDirSector, idx), dirIndex);
			}
			msxDirSector = nextMsxDirSector(msxDirSector);
		} while (msxDirSector != unsigned(-1));
	} catch (FileException& e) {
		cliComm.printWarning("Error while syncing host directory: ",
		                     hostName, ": ", e.getMessage());
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

		File file(hostDir + hostName, File::OpenMode::TRUNCATE);
		unsigned offset = 0;
		vector<bool> visited(maxCluster, false);

		while ((FIRST_CLUSTER <= curCl) && (curCl < maxCluster)) {
			if (visited[curCl]) {
				// detected cycle, invalid disk, but don't crash on it
				return;
			}
			visited[curCl] = true;

			unsigned logicalSector = clusterToSector(curCl);
			for (auto i : xrange(SECTORS_PER_CLUSTER)) {
				if (offset >= msxSize) break;
				unsigned sector = logicalSector + i;
				assert(sector < nofSectors);
				auto writeSize = std::min<size_t>(msxSize - offset, SECTOR_SIZE);
				file.write(subspan(sectors[sector].raw, 0, writeSize));
				offset += SECTOR_SIZE;
			}
			if (offset >= msxSize) break;
			curCl = readFAT(curCl);
		}
	} catch (FileException& e) {
		cliComm.printWarning("Error while syncing host file: ",
		                     hostName, ": ", e.getMessage());
	}
}

void DirAsDSK::writeDIRSector(unsigned sector, DirIndex dirDirIndex,
                              const SectorBuffer& buf)
{
	// Look for changed directory entries.
	for (auto idx : xrange(DIR_ENTRIES_PER_SECTOR)) {
		const auto& newEntry = buf.dirEntry[idx];
		DirIndex dirIndex(sector, idx);
		if (msxDir(dirIndex) != newEntry) {
			writeDIREntry(dirIndex, dirDirIndex, newEntry);
		}
	}
	// At this point sector should be updated.
	assert(std::ranges::equal(sectors[sector].raw, buf.raw));
}

void DirAsDSK::writeDIREntry(DirIndex dirIndex, DirIndex dirDirIndex,
                             const MSXDirEntry& newEntry)
{
	if ((msxDir(dirIndex).filename != newEntry.filename) ||
	    ((msxDir(dirIndex).attrib & MSXDirEntry::Attrib::DIRECTORY) !=
	     (        newEntry.attrib & MSXDirEntry::Attrib::DIRECTORY))) {
		// Name or file-type in the direntry was changed.
		if (auto it = mapDirs.find(dirIndex); it != end(mapDirs)) {
			// If there is an associated host file, then delete it
			// (in case of a rename, the file will be recreated
			// below).
			auto fullHostName = tmpStrCat(hostDir, it->second.hostName);
			FileOperations::deleteRecursive(fullHostName); // ignore return value
			// Remove mapping between msx and host file/dir.
			mapDirs.erase(it);
			if (msxDir(dirIndex).attrib & MSXDirEntry::Attrib::DIRECTORY) {
				// In case of a directory also unmap all
				// sub-components.
				unsigned cluster = msxDir(dirIndex).startCluster;
				if ((FIRST_CLUSTER <= cluster) &&
				    (cluster < maxCluster)) {
					unmapHostFiles(clusterToSector(cluster));
				}
			}
		}
	}

	// Copy the new msx directory entry.
	msxDir(dirIndex) = newEntry;

	// (Re-)export the full file/directory.
	exportToHost(dirIndex, dirDirIndex);
}

void DirAsDSK::writeDataSector(unsigned sector, const SectorBuffer& buf)
{
	assert(sector >= firstDataSector);
	assert(sector < nofSectors);

	// Buffer the write, whether the sector is mapped to a file or not.
	sectors[sector] = buf;

	// Get first cluster in the FAT chain that contains this sector.
	auto [cluster, offset] = sectorToClusterOffset(sector);
	auto [startCluster, chainLength] = getChainStart(cluster);
	offset += narrow<unsigned>((sizeof(buf) * SECTORS_PER_CLUSTER) * chainLength);

	// Get corresponding directory entry.
	auto entry = getDirEntryForCluster(startCluster);
	if (!entry) return;
	const auto* v = lookup(mapDirs, entry->dirIndex);
	if (!v) return; // sector was not mapped to a file, nothing more to do.

	// Actually write data to host file.
	string fullHostName = hostDir + v->hostName;
	try {
		File file(fullHostName, "rb+"); // don't uncompress
		file.seek(offset);
		unsigned msxSize = msxDir(entry->dirIndex).size;
		if (msxSize > offset) {
			auto writeSize = std::min<size_t>(msxSize - offset, sizeof(buf));
			file.write(subspan(buf.raw, 0, writeSize));
		}
	} catch (FileException& e) {
		cliComm.printWarning("Couldn't write to file ", fullHostName,
		                     ": ", e.getMessage());
	}
}

} // namespace openmsx
