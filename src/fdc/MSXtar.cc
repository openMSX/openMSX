// Note: For Mac OS X 10.3 <ctime> must be included before <utime.h>.
#include <ctime>
#ifndef _MSC_VER
#include <utime.h>
#else
#include <sys/utime.h>
#endif

#include "MSXtar.hh"
#include "SectorAccessibleDisk.hh"
#include "FileOperations.hh"
#include "foreach_file.hh"
#include "MSXException.hh"
#include "StringOp.hh"
#include "strCat.hh"
#include "File.hh"
#include "narrow.hh"
#include "one_of.hh"
#include "stl.hh"
#include "xrange.hh"
#include <cstring>
#include <cassert>
#include <cctype>

using std::string;
using std::string_view;

namespace openmsx {

static constexpr unsigned BAD_FAT = 0xFF7;
static constexpr unsigned EOF_FAT = 0xFFF; // actually 0xFF8-0xFFF, signals EOF in FAT12
static constexpr unsigned SECTOR_SIZE = SectorAccessibleDisk::SECTOR_SIZE;

static constexpr uint8_t T_MSX_REG  = 0x00; // Normal file
static constexpr uint8_t T_MSX_READ = 0x01; // Read-Only file
static constexpr uint8_t T_MSX_HID  = 0x02; // Hidden file
static constexpr uint8_t T_MSX_SYS  = 0x04; // System file
static constexpr uint8_t T_MSX_VOL  = 0x08; // filename is Volume Label
static constexpr uint8_t T_MSX_DIR  = 0x10; // entry is a subdir
static constexpr uint8_t T_MSX_ARC  = 0x20; // Archive bit
// This particular combination of flags indicates that this dir entry is used
// to store a long Unicode file name.
// For details, read http://home.teleport.com/~brainy/lfn.htm
static constexpr uint8_t T_MSX_LFN  = 0x0F; // LFN entry (long files names)

/** Transforms a cluster number towards the first sector of this cluster
  * The calculation uses info read fom the boot sector
  */
unsigned MSXtar::clusterToSector(unsigned cluster) const
{
	return 1 + rootDirLast + sectorsPerCluster * (cluster - 2);
}

/** Transforms a sector number towards it containing cluster
  * The calculation uses info read fom the boot sector
  */
unsigned MSXtar::sectorToCluster(unsigned sector) const
{
	return 2 + ((sector - (1 + rootDirLast)) / sectorsPerCluster);
}


/** Initialize object variables by reading info from the boot sector
  */
void MSXtar::parseBootSector(const MSXBootSector& boot)
{
	unsigned nbRootDirSectors = boot.dirEntries / 16;
	sectorsPerFat     = boot.sectorsFat;
	sectorsPerCluster = boot.spCluster;

	if (boot.nrSectors == 0) { // TODO: check limits more accurately
		throw MSXException(
			"Illegal number of sectors: ", boot.nrSectors);
	}
	if (boot.nrSides == 0) { // TODO: check limits more accurately
		throw MSXException(
			"Illegal number of sides: ", boot.nrSides);
	}
	if (boot.nrFats == 0) { // TODO: check limits more accurately
		throw MSXException(
			"Illegal number of FATs: ", boot.nrFats);
	}
	if (sectorsPerFat == 0) { // TODO: check limits more accurately
		throw MSXException(
			"Illegal number sectors per FAT: ", sectorsPerFat);
	}
	if (nbRootDirSectors == 0) { // TODO: check limits more accurately
		throw MSXException(
			"Illegal number of root dir sectors: ", nbRootDirSectors);
	}
	if (sectorsPerCluster == 0) { // TODO: check limits more accurately
		throw MSXException(
			"Illegal number of sectors per cluster: ", sectorsPerCluster);
	}

	rootDirStart = 1 + boot.nrFats * sectorsPerFat;
	chrootSector = rootDirStart;
	rootDirLast = rootDirStart + nbRootDirSectors - 1;
	maxCluster = sectorToCluster(boot.nrSectors);

	// Some (invalid) disk images have a too small FAT to be able to address
	// all clusters of the image. OpenMSX SVN revisions pre-11326 even
	// created such invalid images for some disk sizes!!
	unsigned maxFatCluster = (2 * SECTOR_SIZE * sectorsPerFat) / 3;
	maxCluster = std::min(maxCluster, maxFatCluster);
}

void MSXtar::writeLogicalSector(unsigned sector, const SectorBuffer& buf)
{
	assert(!fatBuffer.empty());
	unsigned fatSector = sector - 1;
	if (fatSector < sectorsPerFat) {
		// we have a cache and this is a sector of the 1st FAT
		//   --> update cache
		fatBuffer[fatSector] = buf;
		fatCacheDirty = true;
	} else {
		disk.writeSector(sector, buf);
	}
}

void MSXtar::readLogicalSector(unsigned sector, SectorBuffer& buf)
{
	assert(!fatBuffer.empty());
	unsigned fatSector = sector - 1;
	if (fatSector < sectorsPerFat) {
		// we have a cache and this is a sector of the 1st FAT
		//   --> read from cache
		buf = fatBuffer[fatSector];
	} else {
		disk.readSector(sector, buf);
	}
}

MSXtar::MSXtar(SectorAccessibleDisk& sectorDisk)
	: disk(sectorDisk)
{
	if (disk.getNbSectors() == 0) {
		throw MSXException("No disk inserted.");
	}
	try {
		SectorBuffer buf;
		disk.readSector(0, buf);
		parseBootSector(buf.bootSector);
	} catch (MSXException& e) {
		throw MSXException("Bad disk image: ", e.getMessage());
	}

	// cache complete FAT
	fatCacheDirty = false;
	fatBuffer.resize(sectorsPerFat);
	disk.readSectors(std::span{fatBuffer.data(), sectorsPerFat}, 1);
}

// Not used when NRVO is used (but NRVO optimization is not (yet) mandated)
MSXtar::MSXtar(MSXtar&& other) noexcept
	: disk(other.disk)
	, fatBuffer(std::move(other.fatBuffer))
	, maxCluster(other.maxCluster)
	, sectorsPerCluster(other.sectorsPerCluster)
	, sectorsPerFat(other.sectorsPerFat)
	, rootDirStart(other.rootDirStart)
	, rootDirLast(other.rootDirLast)
	, chrootSector(other.chrootSector)
	, fatCacheDirty(other.fatCacheDirty)
{
	other.fatCacheDirty = false;
}

MSXtar::~MSXtar()
{
	if (!fatCacheDirty) return;

	for (auto i : xrange(sectorsPerFat)) {
		try {
			disk.writeSector(i + 1, fatBuffer[i]);
		} catch (MSXException&) {
			// nothing
		}
	}
}

// transform BAD_FAT (0xFF7) and EOF_FAT-range (0xFF8-0xFFF)
// to a single value: EOF_FAT (0xFFF)
static constexpr unsigned normalizeFAT(unsigned cluster)
{
	return (cluster < BAD_FAT) ? cluster : EOF_FAT;
}

// Get the next cluster number from the FAT chain
unsigned MSXtar::readFAT(unsigned clNr) const
{
	assert(!fatBuffer.empty()); // FAT must already be cached
	std::span<const uint8_t> data{fatBuffer[0].raw.data(), sectorsPerFat * size_t(SECTOR_SIZE)};
	auto p = subspan<2>(data, (clNr * 3) / 2);
	unsigned result = (clNr & 1)
	                ? (p[0] >> 4) + (p[1] << 4)
	                : p[0] + ((p[1] & 0x0F) << 8);
	return normalizeFAT(result);
}

// Write an entry to the FAT
void MSXtar::writeFAT(unsigned clNr, unsigned val)
{
	assert(!fatBuffer.empty()); // FAT must already be cached
	assert(val < 4096); // FAT12
	std::span data{fatBuffer[0].raw.data(), sectorsPerFat * size_t(SECTOR_SIZE)};
	auto p = subspan<2>(data, (clNr * 3) / 2);
	if (clNr & 1) {
		p[0] = narrow_cast<uint8_t>((p[0] & 0x0F) + (val << 4));
		p[1] = narrow_cast<uint8_t>(val >> 4);
	} else {
		p[0] = narrow_cast<uint8_t>(val);
		p[1] = narrow_cast<uint8_t>((p[1] & 0xF0) + ((val >> 8) & 0x0F));
	}
	fatCacheDirty = true;
}

// Find the next cluster number marked as free in the FAT
// @throws When no more free clusters
unsigned MSXtar::findFirstFreeCluster()
{
	for (auto cluster : xrange(2u, maxCluster)) {
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
		unsigned nextCl = readFAT(currCluster);
		return (nextCl == EOF_FAT) ? 0 : clusterToSector(nextCl);
	}
}

// get start cluster from a directory entry,
// also takes care of BAD_FAT and EOF_FAT-range.
static unsigned getStartCluster(const MSXDirEntry& entry)
{
	return normalizeFAT(entry.startCluster);
}

// If there are no more free entries in a subdirectory, the subdir is
// expanded with an extra cluster. This function gets the free cluster,
// clears it and updates the fat for the subdir
// returns: the first sector in the newly appended cluster
// @throws When disk is full
unsigned MSXtar::appendClusterToSubdir(unsigned sector)
{
	unsigned nextCl = findFirstFreeCluster();
	unsigned nextSector = clusterToSector(nextCl);

	// clear this cluster
	SectorBuffer buf;
	ranges::fill(buf.raw, 0);
	for (auto i : xrange(sectorsPerCluster)) {
		writeLogicalSector(i + nextSector, buf);
	}

	unsigned curCl = sectorToCluster(sector);
	assert(readFAT(curCl) == EOF_FAT);
	writeFAT(curCl, nextCl);
	writeFAT(nextCl, EOF_FAT);
	return nextSector;
}


// Returns the index of a free (or with deleted file) entry
//  In:  The current dir sector
//  Out: index number, if no index is found then -1 is returned
unsigned MSXtar::findUsableIndexInSector(unsigned sector)
{
	SectorBuffer buf;
	readLogicalSector(sector, buf);

	// find a not used (0x00) or delete entry (0xE5)
	for (auto i : xrange(16)) {
		if (buf.dirEntry[i].filename[0] == one_of(0x00, char(0xE5))) {
			return i;
		}
	}
	return unsigned(-1);
}

// This function returns the sector and dirIndex for a new directory entry
// if needed the involved subdirectory is expanded by an extra cluster
// returns: a DirEntry containing sector and index
// @throws When either root dir is full or disk is full
MSXtar::DirEntry MSXtar::addEntryToDir(unsigned sector)
{
	// this routine adds the msx name to a directory sector, if needed (and
	// possible) the directory is extened with an extra cluster
	DirEntry result;
	result.sector = sector;

	if (sector <= rootDirLast) {
		// add to the root directory
		for (/* */ ; result.sector <= rootDirLast; result.sector++) {
			result.index = findUsableIndexInSector(result.sector);
			if (result.index != unsigned(-1)) {
				return result;
			}
		}
		throw MSXException("Root directory full.");

	} else {
		// add to a subdir
		while (true) {
			result.index = findUsableIndexInSector(result.sector);
			if (result.index != unsigned(-1)) {
				return result;
			}
			unsigned nextSector = getNextSector(result.sector);
			if (nextSector == 0) {
				nextSector = appendClusterToSubdir(result.sector);
			}
			result.sector = nextSector;
		}
	}
}

// create an MSX filename 8.3 format, if needed in vfat like abbreviation
static char toMSXChr(char a)
{
	a = narrow<char>(toupper(a));
	if (a == one_of(' ', '.')) {
		a = '_';
	}
	return a;
}

// Transform a long hostname in a 8.3 uppercase filename as used in the
// dirEntries on an MSX
static string makeSimpleMSXFileName(string_view fullFilename)
{
	auto [dir, fullFile] = StringOp::splitOnLast(fullFilename, '/');

	// handle special case '.' and '..' first
	string result(8 + 3, ' ');
	if (fullFile == one_of(".", "..")) {
		ranges::copy(fullFile, result);
		return result;
	}

	auto [file, ext] = StringOp::splitOnLast(fullFile, '.');
	if (file.empty()) std::swap(file, ext);

	StringOp::trimRight(file, ' ');
	StringOp::trimRight(ext,  ' ');

	// put in major case and create '_' if needed
	string fileS(file.data(), std::min<size_t>(8, file.size()));
	string extS (ext .data(), std::min<size_t>(3, ext .size()));
	transform_in_place(fileS, toMSXChr);
	transform_in_place(extS,  toMSXChr);

	// add correct number of spaces
	ranges::copy(fileS, subspan<8>(result, 0));
	ranges::copy(extS,  subspan<3>(result, 8));
	return result;
}

// This function creates a new MSX subdir with given date 'd' and time 't'
// in the subdir pointed at by 'sector'. In the newly
// created subdir the entries for '.' and '..' are created
// returns: the first sector of the new subdir
// @throws in case no directory could be created
unsigned MSXtar::addSubdir(
	std::string_view msxName, uint16_t t, uint16_t d, unsigned sector)
{
	// returns the sector for the first cluster of this subdir
	DirEntry result = addEntryToDir(sector);

	// load the sector
	SectorBuffer buf;
	readLogicalSector(result.sector, buf);

	auto& dirEntry = buf.dirEntry[result.index];
	ranges::copy(makeSimpleMSXFileName(msxName), dirEntry.filename);
	dirEntry.attrib = T_MSX_DIR;
	dirEntry.time = t;
	dirEntry.date = d;

	// dirEntry.filesize = fsize;
	unsigned curCl = findFirstFreeCluster();
	dirEntry.startCluster = narrow<uint16_t>(curCl);
	writeFAT(curCl, EOF_FAT);

	// save the sector again
	writeLogicalSector(result.sector, buf);

	// clear this cluster
	unsigned logicalSector = clusterToSector(curCl);
	ranges::fill(buf.raw, 0);
	for (auto i : xrange(sectorsPerCluster)) {
		writeLogicalSector(i + logicalSector, buf);
	}

	// now add the '.' and '..' entries!!
	memset(&buf.dirEntry[0], 0, sizeof(MSXDirEntry));
	ranges::fill(buf.dirEntry[0].filename, ' ');
	buf.dirEntry[0].filename[0] = '.';
	buf.dirEntry[0].attrib = T_MSX_DIR;
	buf.dirEntry[0].time = t;
	buf.dirEntry[0].date = d;
	buf.dirEntry[0].startCluster = narrow<uint16_t>(curCl);

	memset(&buf.dirEntry[1], 0, sizeof(MSXDirEntry));
	ranges::fill(buf.dirEntry[1].filename, ' ');
	buf.dirEntry[1].filename[0] = '.';
	buf.dirEntry[1].filename[1] = '.';
	buf.dirEntry[1].attrib = T_MSX_DIR;
	buf.dirEntry[1].time = t;
	buf.dirEntry[1].date = d;
	buf.dirEntry[1].startCluster = narrow<uint16_t>(sectorToCluster(sector));

	// and save this in the first sector of the new subdir
	writeLogicalSector(logicalSector, buf);

	return logicalSector;
}

struct TimeDate {
	uint16_t time, date;
};
static TimeDate getTimeDate(time_t totalSeconds)
{
	if (tm* mtim = localtime(&totalSeconds)) {
		auto time = narrow<uint16_t>(
			(mtim->tm_sec >> 1) + (mtim->tm_min << 5) +
			(mtim->tm_hour << 11));
		auto date = narrow<uint16_t>(
			mtim->tm_mday + ((mtim->tm_mon + 1) << 5) +
			((mtim->tm_year + 1900 - 1980) << 9));
		return {time, date};
	}
	return {0, 0};
}

// Get the time/date from a host file in MSX format
static TimeDate getTimeDate(zstring_view filename)
{
	if (auto st = FileOperations::getStat(filename)) {
		// Some info indicates that st.st_mtime could be useless on win32 with vfat.
		// On Android 'st_mtime' is 'unsigned long' instead of 'time_t'
		// (like on linux), so we require a reinterpret_cast. That cast
		// is fine (but redundant) on linux.
		return getTimeDate(reinterpret_cast<time_t&>(st->st_mtime));
	} else {
		// stat failed
		return {0, 0};
	}
}

// Add an MSXsubdir with the time properties from the HOST-OS subdir
// @throws when subdir could not be created
unsigned MSXtar::addSubdirToDSK(zstring_view hostName, std::string_view msxName,
                                unsigned sector)
{
	auto [time, date] = getTimeDate(hostName);
	return addSubdir(msxName, time, date, sector);
}

// This file alters the filecontent of a given file
// It only changes the file content (and the filesize in the msxDirEntry)
// It doesn't changes timestamps nor filename, filetype etc.
// @throws when something goes wrong
void MSXtar::alterFileInDSK(MSXDirEntry& msxDirEntry, const string& hostName)
{
	// get host file size
	auto st = FileOperations::getStat(hostName);
	if (!st) {
		throw MSXException("Error reading host file: ", hostName);
	}
	unsigned hostSize = narrow<unsigned>(st->st_size);
	unsigned remaining = hostSize;

	// open host file for reading
	File file(hostName, "rb");

	// copy host file to image
	unsigned prevCl = 0;
	unsigned curCl = getStartCluster(msxDirEntry);
	while (remaining) {
		// allocate new cluster if needed
		try {
			if (curCl == one_of(0u, EOF_FAT)) {
				unsigned newCl = findFirstFreeCluster();
				if (prevCl == 0) {
					msxDirEntry.startCluster = narrow<uint16_t>(newCl);
				} else {
					writeFAT(prevCl, newCl);
				}
				writeFAT(newCl, EOF_FAT);
				curCl = newCl;
			}
		} catch (MSXException&) {
			// no more free clusters
			break;
		}

		// fill cluster
		unsigned logicalSector = clusterToSector(curCl);
		for (unsigned j = 0; (j < sectorsPerCluster) && remaining; ++j) {
			SectorBuffer buf;
			unsigned chunkSize = std::min(SECTOR_SIZE, remaining);
			file.read(subspan(buf.raw, 0, chunkSize));
			ranges::fill(subspan(buf.raw, chunkSize), 0);
			writeLogicalSector(logicalSector + j, buf);
			remaining -= chunkSize;
		}

		// advance to next cluster
		prevCl = curCl;
		curCl = readFAT(curCl);
	}

	// terminate FAT chain
	if (prevCl == 0) {
		msxDirEntry.startCluster = 0;
	} else {
		writeFAT(prevCl, EOF_FAT);
	}

	// free rest of FAT chain
	while (curCl != one_of(EOF_FAT, 0u)) {
		unsigned nextCl = readFAT(curCl);
		writeFAT(curCl, 0);
		curCl = nextCl;
	}

	// write (possibly truncated) file size
	msxDirEntry.size = hostSize - remaining;

	if (remaining) {
		throw MSXException("Disk full, ", hostName, " truncated.");
	}
}

// Find the dir entry for 'name' in subdir starting at the given 'sector'
// with given 'index'
// returns: a DirEntry with sector and index filled in
//          sector is 0 if no match was found
MSXtar::DirEntry MSXtar::findEntryInDir(
	const string& name, unsigned sector, SectorBuffer& buf)
{
	DirEntry result;
	result.sector = sector;
	result.index = 0; // avoid warning (only some gcc versions complain)
	while (result.sector) {
		// read sector and scan 16 entries
		readLogicalSector(result.sector, buf);
		for (result.index = 0; result.index < 16; ++result.index) {
			if (ranges::equal(buf.dirEntry[result.index].filename, name)) {
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
string MSXtar::addFileToDSK(const string& fullHostName, unsigned rootSector)
{
	auto [directory, hostName] = StringOp::splitOnLast(fullHostName, "/\\");
	string msxName = makeSimpleMSXFileName(hostName);

	// first find out if the filename already exists in current dir
	SectorBuffer dummy;
	DirEntry fullMsxDirEntry = findEntryInDir(msxName, rootSector, dummy);
	if (fullMsxDirEntry.sector != 0) {
		// TODO implement overwrite option
		return strCat("Warning: preserving entry ", hostName, '\n');
	}

	SectorBuffer buf;
	DirEntry entry = addEntryToDir(rootSector);
	readLogicalSector(entry.sector, buf);
	auto& dirEntry = buf.dirEntry[entry.index];
	memset(&dirEntry, 0, sizeof(dirEntry));
	ranges::copy(msxName, dirEntry.filename);
	dirEntry.attrib = T_MSX_REG;

	// compute time/date stamps
	auto [time, date] = getTimeDate(fullHostName);
	dirEntry.time = time;
	dirEntry.date = date;

	try {
		alterFileInDSK(dirEntry, fullHostName);
	} catch (MSXException&) {
		// still write directory entry
		writeLogicalSector(entry.sector, buf);
		throw;
	}
	writeLogicalSector(entry.sector, buf);
	return {};
}

// Transfer directory and all its subdirectories to the MSX disk image
// @throws when an error occurs
string MSXtar::recurseDirFill(string_view dirName, unsigned sector)
{
	string messages;

	auto fileAction = [&](const string& path) {
		// add new file
		messages += addFileToDSK(path, sector);
	};
	auto dirAction = [&](const string& path, std::string_view name) {
		string msxFileName = makeSimpleMSXFileName(name);
		SectorBuffer buf;
		DirEntry entry = findEntryInDir(msxFileName, sector, buf);
		if (entry.sector != 0) {
			// entry already exists ..
			auto& msxDirEntry = buf.dirEntry[entry.index];
			if (msxDirEntry.attrib & T_MSX_DIR) {
				// .. and is a directory
				unsigned nextSector = clusterToSector(
					getStartCluster(msxDirEntry));
				messages += recurseDirFill(path, nextSector);
			} else {
				// .. but is NOT a directory
				strAppend(messages,
					  "MSX file ", msxFileName,
					  " is not a directory.\n");
			}
		} else {
			// add new directory
			unsigned nextSector = addSubdirToDSK(path, name, sector);
			messages += recurseDirFill(path, nextSector);
		}
	};
	foreach_file_and_directory(std::string(dirName), fileAction, dirAction);

	return messages;
}


static string condenseName(const MSXDirEntry& dirEntry)
{
	string result;
	for (unsigned i = 0; (i < 8) && (dirEntry.base()[i] != ' '); ++i) {
		result += char(tolower(dirEntry.base()[i]));
	}
	if (dirEntry.ext()[0] != ' ') {
		result += '.';
		for (unsigned i = 0; (i < 3) && (dirEntry.ext()[i] != ' '); ++i) {
			result += char(tolower(dirEntry.ext()[i]));
		}
	}
	return result;
}


// Set the entries from dirEntry to the timestamp of resultFile
static void changeTime(zstring_view resultFile, const MSXDirEntry& dirEntry)
{
	unsigned t = dirEntry.time;
	unsigned d = dirEntry.date;
	struct tm mTim;
	struct utimbuf uTim;
	mTim.tm_sec   = narrow<int>((t & 0x001f) << 1);
	mTim.tm_min   = narrow<int>((t & 0x07e0) >> 5);
	mTim.tm_hour  = narrow<int>((t & 0xf800) >> 11);
	mTim.tm_mday  = narrow<int>( (d & 0x001f));
	mTim.tm_mon   = narrow<int>(((d & 0x01e0) >> 5) - 1);
	mTim.tm_year  = narrow<int>(((d & 0xfe00) >> 9) + 80);
	mTim.tm_isdst = -1;
	uTim.actime  = mktime(&mTim);
	uTim.modtime = mktime(&mTim);
	utime(resultFile.c_str(), &uTim);
}

string MSXtar::dir()
{
	string result;
	for (unsigned sector = chrootSector; sector != 0; sector = getNextSector(sector)) {
		SectorBuffer buf;
		readLogicalSector(sector, buf);
		for (auto& dirEntry : buf.dirEntry) {
			if ((dirEntry.filename[0] == one_of(char(0xe5), char(0x00))) ||
			    (dirEntry.attrib == T_MSX_LFN)) continue;

			// filename first (in condensed form for human readability)
			string tmp = condenseName(dirEntry);
			tmp.resize(13, ' ');
			strAppend(result, tmp,
			          // attributes
			          (dirEntry.attrib & T_MSX_DIR  ? 'd' : '-'),
			          (dirEntry.attrib & T_MSX_READ ? 'r' : '-'),
			          (dirEntry.attrib & T_MSX_HID  ? 'h' : '-'),
			          (dirEntry.attrib & T_MSX_VOL  ? 'v' : '-'), // TODO check if this is the output of files,l
			          (dirEntry.attrib & T_MSX_ARC  ? 'a' : '-'), // TODO check if this is the output of files,l
			          "  ",
			          // filesize
			          dirEntry.size, '\n');
		}
	}
	return result;
}

// routines to update the global vars: chrootSector
void MSXtar::chdir(string_view newRootDir)
{
	chroot(newRootDir, false);
}

void MSXtar::mkdir(string_view newRootDir)
{
	unsigned tmpMSXchrootSector = chrootSector;
	chroot(newRootDir, true);
	chrootSector = tmpMSXchrootSector;
}

void MSXtar::chroot(string_view newRootDir, bool createDir)
{
	if (newRootDir.starts_with('/') || newRootDir.starts_with('\\')) {
		// absolute path, reset chrootSector
		chrootSector = rootDirStart;
		StringOp::trimLeft(newRootDir, "/\\");
	}

	while (!newRootDir.empty()) {
		auto [firstPart, lastPart] = StringOp::splitOnFirst(newRootDir, "/\\");
		newRootDir = lastPart;
		StringOp::trimLeft(newRootDir, "/\\");

		// find 'firstPart' directory or create it if requested
		SectorBuffer buf;
		string simple = makeSimpleMSXFileName(firstPart);
		DirEntry entry = findEntryInDir(simple, chrootSector, buf);
		if (entry.sector == 0) {
			if (!createDir) {
				throw MSXException("Subdirectory ", firstPart,
				                   " not found.");
			}
			// creat new subdir
			time_t now;
			time(&now);
			auto [t, d] = getTimeDate(now);
			chrootSector = addSubdir(simple, t, d, chrootSector);
		} else {
			auto& dirEntry = buf.dirEntry[entry.index];
			if (!(dirEntry.attrib & T_MSX_DIR)) {
				throw MSXException(firstPart, " is not a directory.");
			}
			chrootSector = clusterToSector(getStartCluster(dirEntry));
		}
	}
}

void MSXtar::fileExtract(const string& resultFile, const MSXDirEntry& dirEntry)
{
	unsigned size = dirEntry.size;
	unsigned sector = clusterToSector(getStartCluster(dirEntry));

	File file(resultFile, "wb");
	while (size && sector) {
		SectorBuffer buf;
		readLogicalSector(sector, buf);
		unsigned saveSize = std::min(size, SECTOR_SIZE);
		file.write(subspan(buf.raw, 0, saveSize));
		size -= saveSize;
		sector = getNextSector(sector);
	}
	// now change the access time
	changeTime(resultFile, dirEntry);
}

// extracts a single item (file or directory) from the msx image to the host OS
string MSXtar::singleItemExtract(string_view dirName, string_view itemName,
                                 unsigned sector)
{
	// first find out if the filename exists in current dir
	SectorBuffer buf;
	string msxName = makeSimpleMSXFileName(itemName);
	DirEntry entry = findEntryInDir(msxName, sector, buf);
	if (entry.sector == 0) {
		return strCat(itemName, " not found!\n");
	}

	auto& msxDirEntry = buf.dirEntry[entry.index];
	// create full name for local filesystem
	string fullName = strCat(dirName, '/', condenseName(msxDirEntry));

	// ...and extract
	if  (msxDirEntry.attrib & T_MSX_DIR) {
		// recursive extract this subdir
		FileOperations::mkdirp(fullName);
		recurseDirExtract(
			fullName,
			clusterToSector(getStartCluster(msxDirEntry)));
	} else {
		// it is a file
		fileExtract(fullName, msxDirEntry);
	}
	return {};
}


// extracts the contents of the directory (at sector) and all its subdirs to the host OS
void MSXtar::recurseDirExtract(string_view dirName, unsigned sector)
{
	for (/* */ ; sector != 0; sector = getNextSector(sector)) {
		SectorBuffer buf;
		readLogicalSector(sector, buf);
		for (auto& dirEntry : buf.dirEntry) {
			if (dirEntry.filename[0] == one_of(char(0xe5), char(0x00), '.')) {
				continue;
			}
			string filename = condenseName(dirEntry);
			string fullName = filename;
			if (!dirName.empty()) {
				fullName = strCat(dirName, '/', filename);
			}
			if (dirEntry.attrib != T_MSX_DIR) { // TODO
				fileExtract(fullName, dirEntry);
			}
			if (dirEntry.attrib == T_MSX_DIR) {
				FileOperations::mkdirp(fullName);
				// now change the access time
				changeTime(fullName, dirEntry);
				recurseDirExtract(
					fullName,
					clusterToSector(getStartCluster(dirEntry)));
			}
		}
	}
}

string MSXtar::addDir(string_view rootDirName)
{
	return recurseDirFill(rootDirName, chrootSector);
}

string MSXtar::addFile(const string& filename)
{
	return addFileToDSK(filename, chrootSector);
}

string MSXtar::getItemFromDir(string_view rootDirName, string_view itemName)
{
	return singleItemExtract(rootDirName, itemName, chrootSector);
}

void MSXtar::getDir(string_view rootDirName)
{
	recurseDirExtract(rootDirName, chrootSector);
}

} // namespace openmsx
