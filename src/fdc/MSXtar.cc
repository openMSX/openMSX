// Note: For Mac OS X 10.3 <ctime> must be included before <utime.h>.
#include <ctime>
#ifndef _MSC_VER
#include <utime.h>
#else
#include <sys/utime.h>
#endif

#include "MSXtar.hh"
#include "ReadDir.hh"
#include "SectorAccessibleDisk.hh"
#include "FileOperations.hh"
#include "MSXException.hh"
#include "StringOp.hh"
#include "File.hh"
#include <cstring>
#include <algorithm>
#include <cassert>
#include <sys/stat.h>
#include <ctype.h>

using std::string;

namespace openmsx {

static const unsigned BAD_FAT = 0xFF7;
static const unsigned EOF_FAT = 0xFFF; // actually 0xFF8-0xFFF, signals EOF in FAT12
static const unsigned SECTOR_SIZE = SectorAccessibleDisk::SECTOR_SIZE;

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
void MSXtar::parseBootSector(const MSXBootSector& boot)
{
	unsigned nbRootDirSectors = boot.dirEntries / 16;
	sectorsPerFat     = boot.sectorsFat;
	sectorsPerCluster = boot.spCluster;

	if (boot.nrSectors == 0) { // TODO: check limits more accurately
		throw MSXException(StringOp::Builder() <<
			"Illegal number of sectors: " << boot.nrSectors);
	}
	if (boot.nrSides == 0) { // TODO: check limits more accurately
		throw MSXException(StringOp::Builder() <<
			"Illegal number of sides: " << boot.nrSides);
	}
	if (boot.nrFats == 0) { // TODO: check limits more accurately
		throw MSXException(StringOp::Builder() <<
			"Illegal number of FATs: " << boot.nrFats);
	}
	if (sectorsPerFat == 0) { // TODO: check limits more accurately
		throw MSXException(StringOp::Builder() <<
			"Illegal number sectors per FAT: " << sectorsPerFat);
	}
	if (nbRootDirSectors == 0) { // TODO: check limits more accurately
		throw MSXException(StringOp::Builder() <<
			"Illegal number of root dir sectors: " << nbRootDirSectors);
	}
	if (sectorsPerCluster == 0) { // TODO: check limits more accurately
		throw MSXException(StringOp::Builder() <<
			"Illegal number of sectors per cluster: " << sectorsPerCluster);
	}

	rootDirStart = 1 + boot.nrFats * sectorsPerFat;
	chrootSector = rootDirStart;
	rootDirLast = rootDirStart + nbRootDirSectors - 1;
	maxCluster = sectorToCluster(boot.nrSectors);

	// Some (invalid) diskimages have a too small FAT to be able to address
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
		memcpy(&fatBuffer[fatSector], &buf, sizeof(buf));
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
		memcpy(&buf, &fatBuffer[fatSector], sizeof(buf));
	} else {
		disk.readSector(sector, buf);
	}
}

MSXtar::MSXtar(SectorAccessibleDisk& sectordisk)
	: disk(sectordisk)
{
	if (disk.getNbSectors() == 0) {
		throw MSXException("No disk inserted.");
	}
	try {
		SectorBuffer buf;
		disk.readSector(0, buf);
		parseBootSector(buf.bootSector);
	} catch (MSXException& e) {
		throw MSXException("Bad disk image: " + e.getMessage());
	}

	// cache complete FAT
	fatCacheDirty = false;
	fatBuffer.resize(sectorsPerFat);
	for (unsigned i = 0; i < sectorsPerFat; ++i) {
		disk.readSector(i + 1, fatBuffer[i]);
	}
}

MSXtar::~MSXtar()
{
	if (!fatCacheDirty) return;

	for (unsigned i = 0; i < sectorsPerFat; ++i) {
		try {
			disk.writeSector(i + 1, fatBuffer[i]);
		} catch (MSXException&) {
			// nothing
		}
	}
}

// transform BAD_FAT (0xFF7) and EOF_FAT-range (0xFF8-0xFFF)
// to a single value: EOF_FAT (0xFFF)
static unsigned normalizeFAT(unsigned cluster)
{
	return (cluster < BAD_FAT) ? cluster : EOF_FAT;
}

// Get the next clusternumber from the FAT chain
unsigned MSXtar::readFAT(unsigned clnr) const
{
	assert(!fatBuffer.empty()); // FAT must already be cached
	auto* data = fatBuffer[0].raw;
	auto* p = &data[(clnr * 3) / 2];
	unsigned result = (clnr & 1)
	                ? (p[0] >> 4) + (p[1] << 4)
	                : p[0] + ((p[1] & 0x0F) << 8);
	return normalizeFAT(result);
}

// Write an entry to the FAT
void MSXtar::writeFAT(unsigned clnr, unsigned val)
{
	assert(!fatBuffer.empty()); // FAT must already be cached
	assert(val < 4096); // FAT12
	auto* data = fatBuffer[0].raw;
	auto* p = &data[(clnr * 3) / 2];
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

// get start cluster from a directory entry,
// also takes care of BAD_FAT and EOF_FAT-range.
unsigned MSXtar::getStartCluster(const MSXDirEntry& entry)
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
	memset(&buf, 0, sizeof(buf));
	for (unsigned i = 0; i < sectorsPerCluster; ++i) {
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
	for (unsigned i = 0; i < 16; ++i) {
		byte tmp = buf.dirEntry[i].filename[0];
		if ((tmp == 0x00) || (tmp == 0xE5)) {
			return i;
		}
	}
	return unsigned(-1);
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
static string makeSimpleMSXFileName(string_ref fullFilename)
{
	string_ref dir, fullFile;
	StringOp::splitOnLast(fullFilename, '/', dir, fullFile);

	// handle speciale case '.' and '..' first
	string result(8 + 3, ' ');
	if ((fullFile == ".") || (fullFile == "..")) {
		memcpy(&*begin(result), fullFile.data(), fullFile.size());
		return result;
	}

	string_ref file, ext;
	StringOp::splitOnLast(fullFile, '.', file, ext);
	if (file.empty()) std::swap(file, ext);

	StringOp::trimRight(file, ' ');
	StringOp::trimRight(ext,  ' ');

	// put in major case and create '_' if needed
	string fileS(file.data(), std::min<size_t>(8, file.size()));
	string extS (ext .data(), std::min<size_t>(3, ext .size()));
	std::transform(begin(fileS), end(fileS), begin(fileS), toMSXChr);
	std::transform(begin(extS ), end(extS ), begin(extS ), toMSXChr);

	// add correct number of spaces
	memcpy(&*begin(result) + 0, fileS.data(), fileS.size());
	memcpy(&*begin(result) + 8, extS .data(), extS .size());
	return result;
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
	SectorBuffer buf;
	readLogicalSector(result.sector, buf);

	auto& dirEntry = buf.dirEntry[result.index];
	dirEntry.attrib = T_MSX_DIR;
	dirEntry.time = t;
	dirEntry.date = d;
	memcpy(&dirEntry, makeSimpleMSXFileName(msxName).data(), 11);

	// dirEntry.filesize = fsize;
	unsigned curCl = findFirstFreeCluster();
	dirEntry.startCluster = curCl;
	writeFAT(curCl, EOF_FAT);

	// save the sector again
	writeLogicalSector(result.sector, buf);

	// clear this cluster
	unsigned logicalSector = clusterToSector(curCl);
	memset(&buf, 0, sizeof(buf));
	for (unsigned i = 0; i < sectorsPerCluster; ++i) {
		writeLogicalSector(i + logicalSector, buf);
	}

	// now add the '.' and '..' entries!!
	memset(&buf.dirEntry[0], 0, sizeof(MSXDirEntry));
	memset(&buf.dirEntry[0], ' ', 11);
	memset(&buf.dirEntry[0], '.', 1);
	buf.dirEntry[0].attrib = T_MSX_DIR;
	buf.dirEntry[0].time = t;
	buf.dirEntry[0].date = d;
	buf.dirEntry[0].startCluster = curCl;

	memset(&buf.dirEntry[1], 0, sizeof(MSXDirEntry));
	memset(&buf.dirEntry[1], ' ', 11);
	memset(&buf.dirEntry[1], '.', 2);
	buf.dirEntry[1].attrib = T_MSX_DIR;
	buf.dirEntry[1].time = t;
	buf.dirEntry[1].date = d;
	buf.dirEntry[1].startCluster = sectorToCluster(sector);

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
		// Some info indicates that st.st_mtime could be useless on win32 with vfat.
		// On Android 'st_mtime' is 'unsigned long' instead of 'time_t'
		// (like on linux), so we require a reinterpret_cast. That cast
		// is fine (but redundant) on linux.
		getTimeDate(reinterpret_cast<time_t&>(st.st_mtime), time, date);
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
// It only changes the file content (and the filesize in the msxDirEntry)
// It doesn't changes timestamps nor filename, filetype etc.
// @throws when something goes wrong
void MSXtar::alterFileInDSK(MSXDirEntry& msxDirEntry, const string& hostName)
{
	// get host file size
	struct stat st;
	if (stat(hostName.c_str(), &st)) {
		throw MSXException("Error reading host file: " + hostName);
	}
	unsigned hostSize = st.st_size;
	unsigned remaining = hostSize;

	// open host file for reading
	File file(FileOperations::expandTilde(hostName), "rb");

	// copy host file to image
	unsigned prevCl = 0;
	unsigned curCl = getStartCluster(msxDirEntry);
	while (remaining) {
		// allocate new cluster if needed
		try {
			if ((curCl == 0) || (curCl == EOF_FAT)) {
				unsigned newCl = findFirstFreeCluster();
				if (prevCl == 0) {
					msxDirEntry.startCluster = newCl;
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
			memset(&buf, 0, sizeof(buf));
			unsigned chunkSize = std::min(SECTOR_SIZE, remaining);
			file.read(&buf, chunkSize);
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
	while ((curCl != EOF_FAT) && (curCl != 0)) {
		unsigned nextCl = readFAT(curCl);
		writeFAT(curCl, 0);
		curCl = nextCl;
	}

	// write (possibly truncated) file size
	msxDirEntry.size = hostSize - remaining;

	if (remaining) {
		throw MSXException("Disk full, " + hostName + " truncated.");
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
			if (string(buf.dirEntry[result.index].filename, 11) == name) {
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
	string_ref directory, hostName;
	StringOp::splitOnLast(fullname, "/\\", directory, hostName);
	string msxName = makeSimpleMSXFileName(hostName);

	// first find out if the filename already exists in current dir
	SectorBuffer dummy;
	DirEntry fullMsxDirEntry = findEntryInDir(msxName, rootSector, dummy);
	if (fullMsxDirEntry.sector != 0) {
		// TODO implement overwrite option
		return "Warning: preserving entry " + hostName + '\n';
	}

	SectorBuffer buf;
	DirEntry entry = addEntryToDir(rootSector);
	readLogicalSector(entry.sector, buf);
	auto& dirEntry = buf.dirEntry[entry.index];
	memset(&dirEntry, 0, sizeof(dirEntry));
	memcpy(&dirEntry, msxName.data(), 11);
	dirEntry.attrib = T_MSX_REG;

	// compute time/date stamps
	unsigned time, date;
	getTimeDate(fullname, time, date);
	dirEntry.time = time;
	dirEntry.date = date;

	try {
		alterFileInDSK(dirEntry, fullname);
	} catch (MSXException&) {
		// still write directory entry
		writeLogicalSector(entry.sector, buf);
		throw;
	}
	writeLogicalSector(entry.sector, buf);
	return "";
}

// Transfer directory and all its subdirectories to the MSX diskimage
// @throws when an error occurs
string MSXtar::recurseDirFill(string_ref dirName, unsigned sector)
{
	string messages;
	ReadDir readDir(dirName.str());
	while (dirent* d = readDir.getEntry()) {
		string name(d->d_name);
		string fullName = dirName + '/' + name;

		FileOperations::Stat st;
		if (!FileOperations::getStat(fullName, st)) {
			// ignore, normally this should not happen
			continue;
		}

		if (FileOperations::isRegularFile(st)) {
			// add new file
			messages += addFileToDSK(fullName, sector);

		} else if (FileOperations::isDirectory(st) &&
			   (name != ".") && (name != "..")) {
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


string MSXtar::condensName(const MSXDirEntry& dirEntry)
{
	string result;
	for (unsigned i = 0; (i < 8) && (dirEntry.name.base[i] != ' '); ++i) {
		result += tolower(dirEntry.name.base[i]);
	}
	if (dirEntry.name.ext[0] != ' ') {
		result += '.';
		for (unsigned i = 0; (i < 3) && (dirEntry.name.ext[i] != ' '); ++i) {
			result += tolower(dirEntry.name.ext[i]);
		}
	}
	return result;
}


// Set the entries from dirEntry to the timestamp of resultFile
void MSXtar::changeTime(const string& resultFile, const MSXDirEntry& dirEntry)
{
	unsigned t = dirEntry.time;
	unsigned d = dirEntry.date;
	struct tm mtim;
	struct utimbuf utim;
	mtim.tm_sec   =  (t & 0x001f) << 1;
	mtim.tm_min   =  (t & 0x07e0) >> 5;
	mtim.tm_hour  =  (t & 0xf800) >> 11;
	mtim.tm_mday  =  (d & 0x001f);
	mtim.tm_mon   = ((d & 0x01e0) >> 5) - 1;
	mtim.tm_year  = ((d & 0xfe00) >> 9) + 80;
	mtim.tm_isdst = -1;
	utim.actime  = mktime(&mtim);
	utim.modtime = mktime(&mtim);
	utime(resultFile.c_str(), &utim);
}

string MSXtar::dir()
{
	StringOp::Builder result;
	for (unsigned sector = chrootSector; sector != 0; sector = getNextSector(sector)) {
		SectorBuffer buf;
		readLogicalSector(sector, buf);
		for (unsigned i = 0; i < 16; ++i) {
			auto& dirEntry = buf.dirEntry[i];
			if ((dirEntry.filename[0] == char(0xe5)) ||
			    (dirEntry.filename[0] == char(0x00)) ||
			    (dirEntry.attrib == T_MSX_LFN)) continue;

			// filename first (in condensed form for human readablitly)
			string tmp = condensName(dirEntry);
			tmp.resize(13, ' ');
			result << tmp;
			// attributes
			result << (dirEntry.attrib & T_MSX_DIR  ? 'd' : '-')
			       << (dirEntry.attrib & T_MSX_READ ? 'r' : '-')
			       << (dirEntry.attrib & T_MSX_HID  ? 'h' : '-')
			       << (dirEntry.attrib & T_MSX_VOL  ? 'v' : '-') // TODO check if this is the output of files,l
			       << (dirEntry.attrib & T_MSX_ARC  ? 'a' : '-') // TODO check if this is the output of files,l
			       << "  ";
			// filesize
			result << dirEntry.size << '\n';
		}
	}
	return result;
}

// routines to update the global vars: chrootSector
void MSXtar::chdir(string_ref newRootDir)
{
	chroot(newRootDir, false);
}

void MSXtar::mkdir(string_ref newRootDir)
{
	unsigned tmpMSXchrootSector = chrootSector;
	chroot(newRootDir, true);
	chrootSector = tmpMSXchrootSector;
}

void MSXtar::chroot(string_ref newRootDir, bool createDir)
{
	if (newRootDir.starts_with("/") || newRootDir.starts_with("\\")) {
		// absolute path, reset chrootSector
		chrootSector = rootDirStart;
		StringOp::trimLeft(newRootDir, "/\\");
	}

	while (!newRootDir.empty()) {
		string_ref firstPart, lastPart;
		StringOp::splitOnFirst(newRootDir, "/\\", firstPart, lastPart);
		newRootDir = lastPart;
		StringOp::trimLeft(newRootDir, "/\\");

		// find 'firstPart' directory or create it if requested
		SectorBuffer buf;
		string simple = makeSimpleMSXFileName(firstPart);
		DirEntry entry = findEntryInDir(simple, chrootSector, buf);
		if (entry.sector == 0) {
			if (!createDir) {
				throw MSXException("Subdirectory " + firstPart +
				                   " not found.");
			}
			// creat new subdir
			time_t now;
			time(&now);
			unsigned t, d;
			getTimeDate(now, t, d);
			chrootSector = addSubdir(simple, t, d, chrootSector);
		} else {
			auto& dirEntry = buf.dirEntry[entry.index];
			if (!(dirEntry.attrib & T_MSX_DIR)) {
				throw MSXException(firstPart + " is not a directory.");
			}
			chrootSector = clusterToSector(getStartCluster(dirEntry));
		}
	}
}

void MSXtar::fileExtract(const string& resultFile, const MSXDirEntry& dirEntry)
{
	unsigned size = dirEntry.size;
	unsigned sector = clusterToSector(getStartCluster(dirEntry));

	File file(FileOperations::expandTilde(resultFile), "wb");
	while (size && sector) {
		SectorBuffer buf;
		readLogicalSector(sector, buf);
		unsigned savesize = std::min(size, SECTOR_SIZE);
		file.write(&buf, savesize);
		size -= savesize;
		sector = getNextSector(sector);
	}
	// now change the access time
	changeTime(resultFile, dirEntry);
}

// extracts a single item (file or directory) from the msximage to the host OS
string MSXtar::singleItemExtract(string_ref dirName, string_ref itemName,
                                 unsigned sector)
{
	// first find out if the filename exists in current dir
	SectorBuffer buf;
	string msxName = makeSimpleMSXFileName(itemName);
	DirEntry entry = findEntryInDir(msxName, sector, buf);
	if (entry.sector == 0) {
		return itemName + " not found!\n";
	}

	auto& msxDirEntry = buf.dirEntry[entry.index];
	// create full name for loacl filesystem
	string fullName = dirName + '/' + condensName(msxDirEntry);

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
	return "";
}


// extracts the contents of the directory (at sector) and all its subdirs to the host OS
void MSXtar::recurseDirExtract(string_ref dirName, unsigned sector)
{
	for (/* */ ; sector != 0; sector = getNextSector(sector)) {
		SectorBuffer buf;
		readLogicalSector(sector, buf);
		for (unsigned i = 0; i < 16; ++i) {
			auto& dirEntry = buf.dirEntry[i];
			if ((dirEntry.filename[0] == char(0xe5)) ||
			    (dirEntry.filename[0] == char(0x00)) ||
			    (dirEntry.filename[0] == '.')) continue;

			string filename = condensName(dirEntry);
			string fullName = filename;
			if (!dirName.empty()) {
				fullName = dirName + '/' + filename;
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

string MSXtar::addDir(string_ref rootDirName)
{
	return recurseDirFill(rootDirName, chrootSector);
}

string MSXtar::addFile(const string& filename)
{
	return addFileToDSK(filename, chrootSector);
}

string MSXtar::getItemFromDir(string_ref rootDirName, string_ref itemName)
{
	return singleItemExtract(rootDirName, itemName, chrootSector);
}

void MSXtar::getDir(string_ref rootDirName)
{
	recurseDirExtract(rootDirName, chrootSector);
}

} // namespace openmsx
