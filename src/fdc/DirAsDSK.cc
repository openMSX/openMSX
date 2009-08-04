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
#include <sys/types.h>

using std::string;

namespace openmsx {

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

static const unsigned BAD_FAT = 0xFF7;
static const unsigned EOF_FAT = 0xFFF; // actually 0xFF8-0xFFF
static const unsigned MAX_CLUSTER = 720;


// functions to set/get little endian 16/32 bit values
static void setLE16(byte* p, unsigned value)
{
	p[0] = (value >> 0) & 0xFF;
	p[1] = (value >> 8) & 0xFF;
}
static void setLE32(byte* p, unsigned value)
{
	p[0] = (value >>  0) & 0xFF;
	p[1] = (value >>  8) & 0xFF;
	p[2] = (value >> 16) & 0xFF;
	p[3] = (value >> 24) & 0xFF;
}
static unsigned getLE16(const byte* p)
{
	return p[0] + (p[1] << 8);
}
static unsigned getLE32(const byte* p)
{
	return p[0] + (p[1] << 8) + (p[2] << 16) + (p[3] << 24);
}

// transform BAD_FAT (0xFF7) and EOF_FAT-range (0xFF8-0xFFF)
// to a single value: EOF_FAT (0xFFF)
static unsigned normalizeFAT(unsigned cluster)
{
	return (cluster < BAD_FAT) ? cluster : EOF_FAT;
}

static unsigned readFATHelper(const byte* buf, unsigned cluster)
{
	assert(cluster < DirAsDSK::NUM_FAT_ENTRIES);
	const byte* p = buf + (cluster * 3) / 2;
	unsigned result = (cluster & 1)
	                ? (p[0] >> 4) + (p[1] << 4)
	                : p[0] + ((p[1] & 0x0F) << 8);
	return normalizeFAT(result);
}

static void writeFATHelper(byte* buf, unsigned cluster, unsigned val)
{
	assert(cluster < DirAsDSK::NUM_FAT_ENTRIES);
	byte* p = buf + (cluster * 3) / 2;
	if (cluster & 1) {
		p[0] = (p[0] & 0x0F) + (val << 4);
		p[1] = val >> 4;
	} else {
		p[0] = val;
		p[1] = (p[1] & 0xF0) + ((val >> 8) & 0x0F);
	}
}

// read FAT-entry from FAT in memory
unsigned DirAsDSK::readFAT(unsigned cluster)
{
	return readFATHelper(fat, cluster);
}

// read FAT-entry from second FAT in memory
// second FAT is only used internally in DirAsDisk to detect
// updates in the FAT, if the emulated MSX reads a sector from
// the second cache, it actually gets a sector from the first FAT
unsigned DirAsDSK::readFAT2(unsigned cluster)
{
	return readFATHelper(fat2, cluster);
}

// write an entry to FAT in memory
void DirAsDSK::writeFAT(unsigned cluster, unsigned val)
{
	writeFATHelper(fat, cluster, val);
}

// write an entry to FAT2 in memory
// see note at DirAsDSK::readFAT2
void DirAsDSK::writeFAT2(unsigned cluster, unsigned val)
{
	writeFATHelper(fat2, cluster, val);
}

unsigned DirAsDSK::findNextFreeCluster(unsigned curcl)
{
	do {
		++curcl;
	} while ((curcl <= MAX_CLUSTER) && readFAT(curcl));
	return curcl;
}
unsigned DirAsDSK::findFirstFreeCluster()
{
	return findNextFreeCluster(1);
}

// get start cluster from a directory entry,
// also takes care of BAD_FAT and EOF_FAT-range.
unsigned DirAsDSK::getStartCluster(const MSXDirEntry& entry)
{
	return normalizeFAT(getLE16(entry.startcluster));
}

// check if a filename is used in the emulated MSX disk
bool DirAsDSK::checkMSXFileExists(const string& msxfilename)
{
	for (unsigned i = 0; i < NUM_DIR_ENTRIES; ++i) {
		if (strncmp(mapdir[i].msxinfo.filename,
			    msxfilename.c_str(), 11) == 0) {
			return true;
		}
	}
	return false;
}

// check if a file is already mapped into the fake DSK
bool DirAsDSK::checkFileUsedInDSK(const string& filename)
{
	for (unsigned i = 0; i < NUM_DIR_ENTRIES; ++i) {
		if (mapdir[i].shortname == filename) {
			return true;
		}
	}
	return false;
}

// create an MSX filename 8.3 format, if needed in vfat like abreviation
static char toMSXChr(char a)
{
	return (a == ' ') ? '_' : ::toupper(a);
}
static string makeSimpleMSXFileName(string filename)
{
	transform(filename.begin(), filename.end(), filename.begin(), toMSXChr);

	string file, ext;
	StringOp::splitOnLast(filename, ".", file, ext);
	if (file.empty()) swap(file, ext);

	file.resize(8, ' ');
	ext .resize(3, ' ');
	return file + ext;
}

static unsigned clusterToSector(unsigned cluster)
{
	assert(cluster >= 2);
	return 14 + 2 * (cluster - 2);
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
		memset(&mapdir[i].msxinfo, 0, sizeof(MSXDirEntry));
		mapdir[i].shortname.clear();
		mapdir[i].filesize = 0;
	}

	// Make a full clear FAT
	memset(fat,  0, SECTOR_SIZE * SECTORS_PER_FAT);
	memset(fat2, 0, SECTOR_SIZE * SECTORS_PER_FAT);
	// for some reason the first 3bytes are used to indicate the end of a
	// cluster, making the first available cluster nr 2. Some sources say
	// that this indicates the disk format and fat[0] should 0xF7 for
	// single sided disk, and 0xF9 for double sided disks
	// TODO: check this :-)
	fat[0] = 0xF9;
	fat[1] = 0xFF;
	fat[2] = 0xFF;
	fat2[0] = 0xF9;
	fat2[1] = 0xFF;
	fat2[2] = 0xFF;

	// clear the sectormap so that they all point to 'clean' sectors
	for (int i = 0; i < 1440; ++i) {
		sectormap[i].usage = CLEAN;
		sectormap[i].dirEntryNr = 0;
		sectormap[i].fileOffset = 0;
	}
}

DirAsDSK::DirAsDSK(CliComm& cliComm_, const Filename& filename,
		GlobalSettings::SyncMode syncMode_,
		GlobalSettings::BootSectorType bootSectorType)
	: SectorBasedDisk(filename)
	, cliComm(cliComm_)
	, hostDir(filename.getResolved())
	, syncMode(syncMode_)
{
	// create the diskimage based upon the files that can be
	// found in the host directory
	ReadDir dir(hostDir);
	if (!dir.isValid()) {
		throw MSXException("Not a directory");
	}

	// First create structure for the fake disk
	setNbSectors(1440); // asume a DS disk is used
	setSectorsPerTrack(9);
	setNbSides(2);

	// use selected bootsector
	const byte* bootSector =
		  bootSectorType == GlobalSettings::BOOTSECTOR_DOS1
		? BootBlocks::dos1BootBlock
		: BootBlocks::dos2BootBlock;
	memcpy(bootBlock, bootSector, SECTOR_SIZE);

	// make a clean initial disk
	cleandisk();
	// read directory and fill the fake disk
	scanHostDir(false);
}

DirAsDSK::~DirAsDSK()
{
}

void DirAsDSK::readSectorImpl(unsigned sector, byte* buf)
{
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
		memcpy(buf, bootBlock, SECTOR_SIZE);

	} else if (sector < (1 + 2 * SECTORS_PER_FAT)) {
		// copy correct sector from FAT

		// quick-and-dirty:
		// we check all files in the faked disk for altered filesize
		// remapping each fat entry to its direntry and do some bookkeeping
		// to avoid multiple checks will probably be slower than this
		for (unsigned i = 0; i < NUM_DIR_ENTRIES; ++i) {
			checkAlterFileInDisk(i);
		}

		unsigned fatSector = (sector - 1) % SECTORS_PER_FAT;
		memcpy(buf, fat + fatSector * SECTOR_SIZE, SECTOR_SIZE);

	} else if (sector < 14) {
		// create correct DIR sector
		sector -= (1 + 2 * SECTORS_PER_FAT);
		int dirCount = sector * 16;
		// check if there are new files on the host when we read the
		// first directory sector
		if (dirCount == 0) {
			scanHostDir(true);
		}
		for (int i = 0; i < 16; ++i, ++dirCount) {
			checkAlterFileInDisk(dirCount);
			memcpy(&buf[32 * i], &(mapdir[dirCount].msxinfo), 32);
		}

	} else {
		// else get map from sector to file and read correct block
		// folowing same numbering as FAT eg. first data block is cluster 2
		if (sectormap[sector].usage == CLEAN) {
			// return an 'empty' sector
			// 0xE5 is the value used on the Philips VG8250
			memset(buf, 0xE5, SECTOR_SIZE);
		} else if (sectormap[sector].usage == CACHED) {
			memcpy(buf, cachedSectors[sector].data, SECTOR_SIZE);
		} else {
			// first copy cached data
			// in case (end of) file only fills partial sector
			memcpy(buf, cachedSectors[sector].data, SECTOR_SIZE);
			// read data from host file
			int offset = sectormap[sector].fileOffset;
			string shortname = mapdir[sectormap[sector].dirEntryNr].shortname;
			checkAlterFileInDisk(shortname);
			// now try to read from file if possible
			try {
				string fullfilename = hostDir + '/' + shortname;
				File file(fullfilename);
				unsigned size = file.getSize();
				file.seek(offset);
				file.read(buf, std::min<int>(size - offset, SECTOR_SIZE));
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

void DirAsDSK::checkAlterFileInDisk(const string& filename)
{
	for (unsigned i = 0; i < NUM_DIR_ENTRIES; ++i) {
		if (mapdir[i].shortname == filename) {
			checkAlterFileInDisk(i);
		}
	}
}

void DirAsDSK::checkAlterFileInDisk(unsigned dirindex)
{
	if (!mapdir[dirindex].inUse()) {
		return;
	}

	string fullfilename = hostDir + '/' + mapdir[dirindex].shortname;
	struct stat fst;
	if (stat(fullfilename.c_str(), &fst) == 0) {
		if (mapdir[dirindex].filesize != fst.st_size) {
			// changed filesize
			updateFileInDisk(dirindex, fst);
		}
	} else {
		// file can not be stat'ed => assume it has been deleted
		// and thus delete it from the MSX DIR sectors by marking
		// the first filename char as 0xE5
		debug(" host os file deleted ? %s\n", mapdir[dirindex].shortname.c_str());
		mapdir[dirindex].msxinfo.filename[0] = char(0xE5);
		mapdir[dirindex].shortname.clear();
		// Since we do not clear the FAT (a real MSX doesn't either)
		// and all data is cached in memmory you now can use MSX-DOS
		// undelete tools to restore the file on your host-OS, using
		// the 8.3 msx name of course :-)
		//
		// TODO: It might be a good idea to mark all the sectors as
		// CACHED instead of MIXED since the original file is gone...
	}
}

void DirAsDSK::updateFileInDisk(unsigned dirindex, struct stat& fst)
{
	// compute time/date stamps
	struct tm* mtim = localtime(&(fst.st_mtime));
	int t1 = mtim ? (mtim->tm_sec >> 1) + (mtim->tm_min << 5) +
	                (mtim->tm_hour << 11)
	              : 0;
	setLE16(mapdir[dirindex].msxinfo.time, t1);
	int t2 = mtim ? mtim->tm_mday + ((mtim->tm_mon + 1) << 5) +
	                ((mtim->tm_year + 1900 - 1980) << 9)
	              : 0;
	setLE16(mapdir[dirindex].msxinfo.date, t2);

	int fsize = fst.st_size;
	mapdir[dirindex].filesize = fsize;
	unsigned curcl = getStartCluster(mapdir[dirindex].msxinfo);
	// if there is no cluster assigned yet to this file, then find a free cluster
	bool followFATClusters = true;
	if (curcl == 0) {
		followFATClusters = false;
		curcl = findFirstFreeCluster();
		setLE16(mapdir[dirindex].msxinfo.startcluster, curcl);
	}

	unsigned size = fsize;
	unsigned prevcl = 0;
	try {
		string fullfilename = hostDir + '/' + mapdir[dirindex].shortname;
		File file(fullfilename, "rb"); // don't uncompress

		while (size && (curcl <= MAX_CLUSTER)) {
			unsigned logicalSector = clusterToSector(curcl);
			for (int i = 0; i < 2; ++i) {
				sectormap[logicalSector + i].usage = MIXED;
				sectormap[logicalSector + i].dirEntryNr = dirindex;
				sectormap[logicalSector + i].fileOffset = fsize - size;
				byte* buf = cachedSectors[logicalSector + i].data;
				memset(buf, 0, SECTOR_SIZE); // in case (end of) file only fills partial sector
				file.seek(sectormap[logicalSector + i].fileOffset);
				file.read(buf, std::min<int>(size, SECTOR_SIZE));
				size -= std::min<int>(size, SECTOR_SIZE);
				if (size == 0) {
					// don't fill next sectors in this cluster
					// if there is no data left
					break;
				}
			}

			if (prevcl) {
				writeFAT(prevcl, curcl);
			}
			prevcl = curcl;

			// now we check if we continue in the current cluster chain
			// or need to allocate extra unused blocks
			if (followFATClusters) {
				curcl = readFAT(curcl);
				if (curcl == EOF_FAT) {
					followFATClusters = false;
					curcl = findFirstFreeCluster();
				}
			} else {
				curcl = findNextFreeCluster(curcl);
			}
			// Continuing at cluster 'curcl'
		}
	} catch (FileException&) {
		// Error opening or reading host file
		cliComm.printWarning("Error reading host file: " +
		                     mapdir[dirindex].shortname +
		                     ". Truncated file on MSX disk.");
		size = 0; // truncate MSX file
	}
	if ((size == 0) && (curcl <= MAX_CLUSTER)) {
		// TODO: check what an MSX does with filesize zero and fat allocation
		if (prevcl == 0) {
			writeFAT(curcl, EOF_FAT);
		} else {
			writeFAT(prevcl, EOF_FAT);
		}

		// clear remains of FAT if needed
		if (followFATClusters) {
			while ((curcl <= MAX_CLUSTER) && (curcl != 0) &&
			       (curcl != EOF_FAT)) {
				writeFAT(curcl, 0);
				unsigned logicalSector = clusterToSector(curcl);
				for (int i = 0; i < 2; ++i) {
					sectormap[logicalSector + i].usage = CLEAN;
					sectormap[logicalSector + i].dirEntryNr = 0;
					sectormap[logicalSector + i].fileOffset = 0;
				}
				prevcl = curcl;
				curcl = readFAT(curcl);
			}
			writeFAT(prevcl, 0);
			unsigned logicalSector = clusterToSector(prevcl);
			for (int i = 0; i < 2; ++i) {
				sectormap[logicalSector + i].usage = CLEAN;
				sectormap[logicalSector + i].dirEntryNr = 0;
				sectormap[logicalSector + i].fileOffset = 0;
			}
		}
	} else {
		// TODO: don't we need a EOF_FAT in this case as well ?
		// find out and adjust code here
		cliComm.printWarning("Fake Diskimage full: " +
		                     mapdir[dirindex].shortname + " truncated.");
	}
	// write (possibly truncated) file size
	setLE32(mapdir[dirindex].msxinfo.size, fsize - size);
}

void DirAsDSK::truncateCorrespondingFile(unsigned dirindex)
{
	if (!mapdir[dirindex].inUse()) {
		// a total new file so we create the new name from the msx name
		const char* buf = mapdir[dirindex].msxinfo.filename;
		// special case file is deleted but becuase rest of dirEntry changed
		// while file is still deleted...
		if (buf[0] == char(0xE5)) return;
		string shname = condenseName(buf);
		mapdir[dirindex].shortname = shname;
		debug("      truncateCorrespondingFile of new Host OS file\n");
	}
	debug("      truncateCorrespondingFile %s\n", mapdir[dirindex].shortname.c_str());
	int cursize = getLE32(mapdir[dirindex].msxinfo.size);
	mapdir[dirindex].filesize = cursize;

	// stuff below can fail, so do it as the last thing in this method
	try {
		string fullfilename = hostDir + '/' + mapdir[dirindex].shortname;
		File file(fullfilename, File::CREATE);
		file.truncate(cursize);
	} catch (FileException&) {
		cliComm.printWarning("Error while truncating host file: " +
		                     mapdir[dirindex].shortname);
	}
}

void DirAsDSK::extractCacheToFile(unsigned dirindex)
{
	if (!mapdir[dirindex].inUse()) {
		// a total new file so we create the new name from the msx name
		const char* buf = mapdir[dirindex].msxinfo.filename;
		// special case: the file is deleted and we get here because the
		// startcluster is still intact
		if (buf[0] == char(0xE5)) return;
		string shname = condenseName(buf);
		mapdir[dirindex].shortname = shname;
	}
	try {
		string fullfilename = hostDir + '/' + mapdir[dirindex].shortname;
		File file(fullfilename, File::CREATE);
		unsigned curcl = getStartCluster(mapdir[dirindex].msxinfo);
		// if we start a new file the current cluster can be set to zero

		unsigned cursize = getLE32(mapdir[dirindex].msxinfo.size);
		unsigned offset = 0;
		// if the size is zero then we truncate to zero and leave
		if (curcl == 0 || cursize == 0) {
			file.truncate(0);
			return;
		}

		while ((curcl <= MAX_CLUSTER) && (curcl != EOF_FAT) && (curcl != 0)) {
			unsigned logicalSector = clusterToSector(curcl);
			for (int i = 0; i < 2; ++i) {
				if ((sectormap[logicalSector].usage == CACHED ||
				     sectormap[logicalSector].usage == MIXED) &&
				    (cursize >= offset)) {
					// transfer data
					byte* buf = cachedSectors[logicalSector].data;
					file.seek(offset);
					unsigned writesize = std::min<int>(cursize - offset, SECTOR_SIZE);
					file.write(buf, writesize);

					sectormap[logicalSector].usage = MIXED;
					sectormap[logicalSector].dirEntryNr = dirindex;
					sectormap[logicalSector].fileOffset = offset;
				}
				++logicalSector;
				offset += SECTOR_SIZE;
			}
			curcl = readFAT(curcl);
		}
	} catch (FileException&) {
		cliComm.printWarning("Error while syncing host file: " +
		                     mapdir[dirindex].shortname);
	}
}


void DirAsDSK::writeSectorImpl(unsigned sector, const byte* buf)
{
	// is this actually needed ?
	if (syncMode == GlobalSettings::SYNC_READONLY) return;

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
	if (sector >= 14) {
		debug("  Mode: ");
		switch (sectormap[sector].usage) {
		case CLEAN:
			debug("CLEAN\n");
			break;
		case CACHED:
			debug("CACHED\n");
			break;
		case MIXED:
			debug("MIXED ");
			debug("  direntry : %i \n", sectormap[sector].dirEntryNr);
			debug("    => %s \n", mapdir[sectormap[sector].dirEntryNr].shortname.c_str());
			debug("  fileOffset : %li\n", sectormap[sector].fileOffset);
			break;
		}
	}

	if (sector == 0) {
		memcpy(bootBlock, buf, SECTOR_SIZE);
	} else if (sector < (1 + 2 * SECTORS_PER_FAT)) {
		writeFATSector(sector, buf);
	} else if (sector < 14) {
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
	if (sector < (1 + SECTORS_PER_FAT)) {
		unsigned fatSector = sector - 1;
		memcpy(fat + fatSector * SECTOR_SIZE, buf, SECTOR_SIZE);
		return;
	}

	// writes to the second FAT so we check for changes
	// but we fully ignore the sectors afterwards (see remark
	// about identifier bytes above)
	unsigned startcluster = std::max<int>(2, ((sector - 1 - SECTORS_PER_FAT) * 2) / 3);
	unsigned endcluster = std::min<int>(startcluster + 342, NUM_FAT_ENTRIES);
	for (unsigned i = startcluster; i < endcluster; ++i) {
		if (readFAT(i) != readFAT2(i)) {
			updateFileFromAlteredFatOnly(i);
		}
	}

	unsigned fatSector = (sector - 1) % SECTORS_PER_FAT;
	memcpy(fat2 + fatSector * SECTOR_SIZE, buf, SECTOR_SIZE);
}

void DirAsDSK::writeDIRSector(unsigned sector, const byte* buf)
{
	// We assume that the dir entry is updated last, so the
	// fat and actual sectordata should already contain the correct
	// data. Most MSX disk roms honour this behaviour for normal
	// fileactions. Of course some diskcaching programs and disk
	// optimizers can abandon this behaviour and in such case the
	// logic used here goes haywire!!
	sector -= (1 + 2 * SECTORS_PER_FAT);
	for (int i = 0; i < 16; ++i) {
		unsigned dirindex = sector * 16 + i;
		const MSXDirEntry& entry = *reinterpret_cast<const MSXDirEntry*>(&buf[32 * i]);
		if (memcmp(mapdir[dirindex].msxinfo.filename, &entry, 32) != 0) {
			writeDIREntry(dirindex, entry);
		}
	}
}

void DirAsDSK::writeDIREntry(unsigned dirindex, const MSXDirEntry& entry)
{
	int oldClus = getStartCluster(mapdir[dirindex].msxinfo);
	int newClus = getStartCluster(entry);
	int oldSize = getLE32(mapdir[dirindex].msxinfo.size);
	int newSize = getLE32(entry.size);

	// The 3 vital informations needed
	bool chgName = memcmp(mapdir[dirindex].msxinfo.filename, entry.filename, 11) != 0;
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
	debug("  dirindex %i filename: %s\n",
	      dirindex, mapdir[dirindex].shortname.c_str());
	debug("  chgName: %i chgClus: %i chgSize: %i\n", chgName, chgClus, chgSize);
	debug("  Old start %i   New start %i\n", oldClus, newClus);
	debug("  Old size %i  New size %i\n\n", oldSize, newSize);

	if (chgName && !chgClus && !chgSize) {
		if ((entry.filename[0] == char(0xE5)) &&
		    (syncMode == GlobalSettings::SYNC_FULL)) {
			// dir entry has been deleted
			// delete file from host OS and 'clear' all sector
			// data pointing to this HOST OS file
			string fullfilename = hostDir + '/' + mapdir[dirindex].shortname;
			FileOperations::unlink(fullfilename);
			for (int i = 14; i < 1440; ++i) {
				if (sectormap[i].dirEntryNr == dirindex) {
					 sectormap[i].usage = CACHED;
				}
			}
			mapdir[dirindex].shortname.clear();

		} else if ((entry.filename[0] != char(0xE5)) &&
			   ((syncMode == GlobalSettings::SYNC_FULL) ||
			    (syncMode == GlobalSettings::SYNC_NODELETE))) {
			string shname = condenseName(entry.filename);
			string newfilename = hostDir + '/' + shname;
			if (newClus == 0 && newSize == 0) {
				// creating a new file
				mapdir[dirindex].shortname = shname;
				// we do not need to write anything since the MSX
				// will update this later when the size is altered
				try {
					File file(newfilename, File::TRUNCATE);
				} catch (FileException&) {
					cliComm.printWarning("Couldn't create new file.");
				}
			} else {
				// rename file on host OS
				string oldfilename = hostDir + '/' + mapdir[dirindex].shortname;
				if (rename(oldfilename.c_str(), newfilename.c_str()) == 0) {
					// renaming on host OS succeeeded
					mapdir[dirindex].shortname = shname;
				}
			}
		} else {
			cliComm.printWarning(
				"File has been renamed in emulated disk. Host OS file (" +
				mapdir[dirindex].shortname + ") remains untouched!");
		}
	}

	if (chgSize) {
		// content changed, extract the file
		// Cluster might have changed is this is a new file so chgClus
		// is ignored. Also name might have been changed (on a turbo R
		// the single shot Dir update when creating new files)
		if (oldSize < newSize) {
			// new size is bigger, file has grown
			memcpy(&(mapdir[dirindex].msxinfo), &entry, 32);
			extractCacheToFile(dirindex);
		} else {
			// new size is smaller, file has been reduced
			// luckily the entire file is in cache, we need this since on some
			// MSX models during a copy from file to overwrite an existing file
			// the sequence is that first the actual data is written and then
			// the size is set to zero before it is set to the new value. If we
			// didn't cache this, then all the 'mapped' sectors would lose their
			// value
			memcpy(&(mapdir[dirindex].msxinfo), &entry, 32);
			truncateCorrespondingFile(dirindex);
			if (newSize != 0) {
				extractCacheToFile(dirindex); // see copy remark above
			}
		}
	}

	if (!chgName && chgClus && !chgSize) {
		cliComm.printWarning(
			"This case of writing to DIR is not yet implemented "
			"since we haven't encountered it in real life yet.");
	}

	// for now blindly take over info
	memcpy(&(mapdir[dirindex].msxinfo), &entry, 32);
}

void DirAsDSK::writeDataSector(unsigned sector, const byte* buf)
{
	assert(sector >= 14);

	// first and before all else buffer everything !!!!
	// check if cachedSectors exists, if not assign memory.
	memcpy(cachedSectors[sector].data, buf, SECTOR_SIZE);

	// if in SYNC_CACHEDWRITE then simply mark sector as cached and be done with it
	if (syncMode == GlobalSettings::SYNC_CACHEDWRITE) {
		// change to a regular cached sector
		sectormap[sector].usage = CACHED;
		sectormap[sector].dirEntryNr = 0;
		sectormap[sector].fileOffset = 0;
		return;
	}

	if (sectormap[sector].usage == MIXED) {
		// save data to host file
		try {
			int offset = sectormap[sector].fileOffset;
			int dirent = sectormap[sector].dirEntryNr;
			string fullfilename = hostDir + '/' + mapdir[dirent].shortname;
			File file(fullfilename);
			file.seek(offset);
			int cursize = getLE32(mapdir[dirent].msxinfo.size);
			unsigned writesize = std::min<int>(cursize - offset, SECTOR_SIZE);
			file.write(buf, writesize);
		} catch (FileException&) {
			cliComm.printWarning("Couldn't write to file.");
		}
	} else {
		// indicate data is cached, it might be CACHED already or it was CLEAN
		sectormap[sector].usage = CACHED;
	}
}

void DirAsDSK::updateFileFromAlteredFatOnly(unsigned somecluster)
{
	// First look for the first cluster in this chain
	unsigned startcluster = somecluster;
	for (unsigned i = 2; i < NUM_FAT_ENTRIES; ++i) {
		if (readFAT(i) == startcluster) {
			// found a predecessor
			startcluster = i;
			i = 1; // restart search
		}
	}

	// Find the corresponding direntry if any
	// and extract file based on new clusterchain
	for (unsigned i = 0; i < NUM_DIR_ENTRIES; ++i) {
		if (startcluster == getStartCluster(mapdir[i].msxinfo)) {
			extractCacheToFile(i);
			break;
		}
	}

	// from startcluster and somecluster on, update fat2 so that the check
	// in writeFATSector() don't call this routine again for the same file
	unsigned curcl = startcluster;
	while ((curcl <= MAX_CLUSTER) && (curcl != EOF_FAT) && (curcl > 1)) {
		unsigned next = readFAT(curcl);
		writeFAT2(curcl, next);
		curcl = next;
	}

	// since the new FAT chain can be shorter (file size shrunk)
	// we also start from 'somecluster', in such case
	// the loop above doesn't take care of this, since it will
	// stop at the new EOF_FAT||curcl==0 condition
	curcl = somecluster;
	while ((curcl <= MAX_CLUSTER) && (curcl != EOF_FAT) && (curcl > 1)) {
		unsigned next = readFAT(curcl);
		writeFAT2(curcl, next);
		curcl = next;
	}
}


string DirAsDSK::condenseName(const char* buf)
{
	string result;
	for (unsigned i = 0; (i < 8) && (buf[i] != ' '); ++i) {
		result += tolower(buf[i]);
	}
	if (buf[8] != ' ') {
		result += '.';
		for (unsigned i = 8; (i < 11) && (buf[i] != ' '); ++i) {
			result += tolower(buf[i]);
		}
	}
	return result;
}

bool DirAsDSK::isWriteProtectedImpl() const
{
	return syncMode == GlobalSettings::SYNC_READONLY;
}

void DirAsDSK::updateFileInDisk(const string& filename)
{
	string fullfilename = hostDir + '/' + filename;
	struct stat fst;
	if (stat(fullfilename.c_str(), &fst)) {
		cliComm.printWarning("Error accessing " + fullfilename);
		return;
	}
	if (!S_ISREG(fst.st_mode)) {
		// we only handle regular files for now
		if (filename != "." && filename != "..") {
			// don't warn for these files, as they occur in any directory except the root one
			cliComm.printWarning("Not a regular file: " + fullfilename);
		}
		return;
	}
	if (!checkFileUsedInDSK(filename)) {
		// add file to fakedisk
		addFileToDSK(filename, fst);
	} else {
		// really update file
		checkAlterFileInDisk(filename);
	}
}

void DirAsDSK::addFileToDSK(const string& filename, struct stat& fst)
{
	// get emtpy dir entry
	unsigned dirindex = 0;
	while (mapdir[dirindex].inUse()) {
		if (++dirindex == NUM_DIR_ENTRIES) {
			cliComm.printWarning(
				"Couldn't add " + filename +
				": root dir full");
			return;
		}
	}

	// create correct MSX filename
	string MSXfilename = makeSimpleMSXFileName(filename);
	if (checkMSXFileExists(MSXfilename)) {
		// TODO: actually should increase vfat abrev if possible!!
		cliComm.printWarning(
			"Couldn't add " + filename + ": MSX name " +
			MSXfilename + " existed already");
		return;
	}

	// fill in native file name
	mapdir[dirindex].shortname = filename;
	// fill in MSX file name
	memcpy(&(mapdir[dirindex].msxinfo.filename), MSXfilename.c_str(), 11);

	updateFileInDisk(dirindex, fst);
}

} // namespace openmsx
