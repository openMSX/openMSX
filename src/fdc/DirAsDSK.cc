// $Id$

#include "DirAsDSK.hh"
#include "CliComm.hh"
#include "BootBlocks.hh"
#include "GlobalSettings.hh"
#include "BooleanSetting.hh"
#include "EnumSetting.hh"
#include "File.hh"
#include "FileException.hh"
#include "FileOperations.hh"
#include "ReadDir.hh"
#include <algorithm>
#include <cstring>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <cstdio>

using std::string;

namespace openmsx {

// Wrapper function to easily enable/disable debug prints
// Don't check-in this code with printing enabled:
//   printing stuff to stdout breaks stdio CliComm connections!
static void debug(const char* format, ...)
{
#if 0
	va_list args;
	va_start(args, format);
	vprintf(format, args);
	va_end(args);
#endif
}

static const int EOF_FAT = 0xFFF;
static const int MAX_CLUSTER = 720;
static const string bootBlockFileName = ".sector.boot";
static const string cachedSectorsFileName = ".sector.cache";


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

static unsigned readFATHelper(const byte* buf, unsigned cluster)
{
	const byte* p = buf + (cluster * 3) / 2;
	return (cluster & 1)
	     ? (p[0] >> 4) + (p[1] << 4)
	     : p[0] + ((p[1] & 0x0F) << 8);
}

static void writeFATHelper(byte* buf, unsigned cluster, unsigned val)
{
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

int DirAsDSK::findFirstFreeCluster()
{
	int cluster = 2;
	while ((cluster <= MAX_CLUSTER) && readFAT(cluster)) {
		++cluster;
	}
	return cluster;
}

// check if a filename is used in the emulated MSX disk
bool DirAsDSK::checkMSXFileExists(const string& msxfilename)
{
	for (int i = 0; i < 112; ++i) {
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
	for (int i = 0; i < 112; ++i) {
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

static const byte CACHE_ID_DIR    = 1;
static const byte CACHE_ID_SECTOR = 2;
static const byte CACHE_ID_END    = 255;

void DirAsDSK::saveCache()
{
	// safe bootsector file if needed
	if (bootSectorWritten) {
		try {
			File file(hostDir + '/' + bootBlockFileName,
			          File::TRUNCATE);
			file.write(bootBlock, SECTOR_SIZE);
		} catch (FileException& e) {
			cliComm.printWarning(
				"Couldn't create bootsector file.");
		}
	}

	// and now create the new and more complex sectorcache file
	try {
		byte tmpbuf[SECTOR_SIZE];
		File file(hostDir + '/' + cachedSectorsFileName,
		          File::TRUNCATE);

		// first save the new header
		string header("openMSX-sectorcache-v1");
		file.write(header.c_str(), header.size() + 1);

		// now save all the files that are in this disk at this moment
		for (unsigned i = 0; i < 112; ++i) {
			if (!mapdir[i].inUse()) continue;

			// first save CACHE-ID=1,
			// then filename, shortname and filessize in LE32 form
			// and finally the dirindex and the MSXDirEntry for this entry
			unsigned p = 0;
			tmpbuf[p++] = CACHE_ID_DIR;
			// TODO [wouter]: there's no need to store fullfilename
			string fullfilename = hostDir + '/' + mapdir[i].shortname;
			unsigned fs = fullfilename.size();
			memcpy(&tmpbuf[p], fullfilename.c_str(), fs + 1);
			p += fs + 1;
			unsigned ss = mapdir[i].shortname.size();
			memcpy(&tmpbuf[p], mapdir[i].shortname.c_str(), ss + 1);
			p += ss + 1;
			setLE32(&tmpbuf[p], mapdir[i].filesize);
			p += 4;
			// TODO [wouter]: Only one byte for direntry, this makes
			//                it impossible to in the future extend
			//                this to hard disk images
			tmpbuf[p++] = i;
			memcpy(&tmpbuf[p], &(mapdir[i].msxinfo), 32);
			p += 32;
			file.write(tmpbuf, p);

			// then a list of sectors in which the data of the file is stored
			unsigned curcl = getLE16(mapdir[i].msxinfo.startcluster);
			unsigned size = getLE32(mapdir[i].msxinfo.size);
			unsigned logicalSector = clusterToSector(curcl);
			while (size >= SECTOR_SIZE) {
				setLE16(&tmpbuf[0], logicalSector++);
				file.write(tmpbuf, 2);
				size -= SECTOR_SIZE;
				if (size >= SECTOR_SIZE) {
					setLE16(&tmpbuf[0], logicalSector);
					file.write(tmpbuf, 2);
					size -= SECTOR_SIZE;
					// get next cluster info
					curcl = readFAT(curcl);
					logicalSector = clusterToSector(curcl);
				}
			}
			// save last sector + padding info if needed
			if (size) {
				setLE16(&tmpbuf[0], logicalSector);
				file.write(tmpbuf, 2);
				// save padding bytes
				byte* buf = reinterpret_cast<byte*>(
						&cachedSectors[logicalSector][0]);
				file.write(&buf[size], SECTOR_SIZE - size);
			}
		}

		// Now save sectors that are not directly related to a given file.
		// always save fat and dir sectors
		// TODO [wouter]: Shouldn't this be 'i < 14'?
		for (unsigned i = 1; i <= 14; ++i) {
			tmpbuf[0] = CACHE_ID_SECTOR;
			setLE16(&tmpbuf[1], i);
			file.write(tmpbuf, 3);
			readLogicalSector(i, tmpbuf);
			file.write(tmpbuf, SECTOR_SIZE);
		}

		for (CachedSectors::const_iterator it = cachedSectors.begin();
		     it != cachedSectors.end(); ++it) {
			// avoid MIXED sectors
			if (sectormap[it->first].usage == CACHED) {
				tmpbuf[0] = CACHE_ID_SECTOR;
				setLE16(&tmpbuf[1], it->first);
				file.write(tmpbuf, 3);

				debug("writing to .sector.cache\n");
				debug("        sector %i\n", it->first);
				file.write(&(it->second[0]), SECTOR_SIZE);
			}
		}

		// end of file marker
		file.write(&CACHE_ID_END, 1);
	} catch (FileException& e) {
		cliComm.printWarning("Couldn't create cached sector file.");
	}
}

bool DirAsDSK::readCache()
{
	try {
		File file(hostDir + '/' + cachedSectorsFileName);

		// first read the new header
		string header("openMSX-sectorcache-v");
		byte tmpbuf[SECTOR_SIZE];
		file.read(tmpbuf, header.size() + 2);
		if (memcmp(tmpbuf, header.c_str(), header.size()) != 0) {
			cliComm.printWarning("Wrong header in sector cache.");
			return false;
		}
		if ((tmpbuf[header.size() + 0] != '1') ||
		    (tmpbuf[header.size() + 1] != 0)) {
			cliComm.printWarning("Wrong version of sector cache format.");
			return false;
		}

		byte id;
		do {
			file.read(&id, 1);
			switch (id) {
			case CACHE_ID_DIR: {
				// file info stored in cache

				// read long filename   !! NOT USED !!
				char c;
				do {
					file.read(&c, 1);
				} while (c);

				// read short filename
				string shortname;
				do {
					file.read(&c, 1);
					if (c) shortname += c;
				} while (c);

				// read filesize, dirindex and MSXDirEntry
				// TODO [wouter]: Shouldn't this be 4 + 1 + 32?
				file.read(tmpbuf, 6 + 32);
				unsigned filesize = getLE32(tmpbuf);
				unsigned dirindex = tmpbuf[4];
				// TODO [wouter]: Check dirindex < 112

				// fill mapdir with correct info
				mapdir[dirindex].shortname = shortname;
				mapdir[dirindex].filesize = filesize;
				memcpy(&(mapdir[dirindex].msxinfo), &tmpbuf[5], 32);

				// fail if size has changed!!
				string fullfilename = hostDir + '/' + shortname;
				struct stat fst;
				if (stat(fullfilename.c_str(), &fst)) {
					// Couldn't stat the file ??
					cliComm.printWarning("Cached file doesn't exist anymore: " + shortname);
					return false;
				}
				if (filesize != unsigned(fst.st_size)) {
					cliComm.printWarning("Filesize of cached file changed! Cache invalidated!");
					return false;
				}

				// read file into memory and fix metadata
				File hostOsFile(fullfilename);
				int offset = 0;
				while (filesize) {
					file.read(tmpbuf, 2);
					unsigned sector = getLE16(tmpbuf);

					if (syncMode == GlobalSettings::SYNC_FULL ||
					    syncMode == GlobalSettings::SYNC_NODELETE) {
						sectormap[sector].usage = MIXED;
					} else {
						sectormap[sector].usage = CACHED;
					}

					sectormap[sector].dirEntryNr = dirindex;
					sectormap[sector].fileOffset = offset;

					cachedSectors[sector].resize(SECTOR_SIZE);
					byte* buf = reinterpret_cast<byte*>(
							&cachedSectors[sector][0]);
					hostOsFile.read(buf, std::min<int>(SECTOR_SIZE, filesize));
					// read padding data if needed
					if (filesize < SECTOR_SIZE) {
						hostOsFile.read(&buf[filesize], (SECTOR_SIZE - filesize));
					}

					offset += SECTOR_SIZE;
					filesize -= std::min<int>(SECTOR_SIZE, filesize);
				}
				break;
			}
			case CACHE_ID_SECTOR: {
				// cached sectors
				file.read(tmpbuf, 2);
				unsigned sector = getLE16(tmpbuf);
				// meta data
				sectormap[sector].usage = CACHED;
				sectormap[sector].dirEntryNr = 0;
				sectormap[sector].fileOffset = 0;
				// sector data
				file.read(tmpbuf, SECTOR_SIZE);
				cachedSectors[sector].resize(SECTOR_SIZE);
				memcpy(&cachedSectors[sector][0], tmpbuf, SECTOR_SIZE);
			}
			case CACHE_ID_END:
				// end of cache marker
				break;
			default:
				cliComm.printWarning("Unknown data element in sector cache format.");
				return false;
			}
		} while (id != CACHE_ID_END);
	} catch (FileException& e) {
		cliComm.printWarning("Couldn't read cached sector file.");
	}

	// for the moment fail cache reading since we still need to implement this
	return false;
}


void DirAsDSK::scanHostDir()
{
	debug("Scanning HostDir for new files\n");
	ReadDir dir(hostDir);
	if (!dir.isValid()) {
		// The entire directory might have been moved/erased
		// but this should not stop the emulated disk from working
		// since all data is already cached by us.
		return;
	}
	// read directory and fill the fake disk
	while (struct dirent* d = dir.getEntry()) {
		string name(d->d_name);
		// check if file is added to diskimage
		if (!checkFileUsedInDSK(name)) {
			if ((name != bootBlockFileName) &&
			    (name != cachedSectorsFileName)) {
				debug("found new file %s\n", d->d_name);
				updateFileInDisk(name);
			}
		}
	}
}


void DirAsDSK::cleandisk()
{
	// assign empty directory entries
	for (int i = 0; i < 112; ++i) {
		memset(&mapdir[i].msxinfo, 0, sizeof(MSXDirEntry));
		mapdir[i].shortname.clear();
		mapdir[i].filesize = 0;
	}

	// Make a full clear FAT
	memset(fat, 0, SECTOR_SIZE * SECTORS_PER_FAT);
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

DirAsDSK::DirAsDSK(CliComm& cliComm_, GlobalSettings& globalSettings_,
                   const string& fileName)
	: SectorBasedDisk(fileName)
	, cliComm(cliComm_)
	, hostDir(fileName)
	, globalSettings(globalSettings_)
{
	//TODO: make these settings, for now as test purpose we define them here...
	syncMode = globalSettings.getSyncDirAsDSKSetting().getValue();
	bootSectorWritten = false;

	// create the diskimage based upon the files that can be
	// found in the host directory
	ReadDir dir(hostDir);
	if (!dir.isValid()) {
		throw MSXException("Not a directory");
	}

	// First create structure for the fake disk
	nbSectors = 1440; // asume a DS disk is used
	sectorsPerTrack = 9;
	nbSides = 2;

	try {
		// try to read boot block from file
		File file(hostDir + '/' + bootBlockFileName);
		file.read(bootBlock, SECTOR_SIZE);
	} catch (FileException& e) {
		// or use default when that fails
		const byte* bootSector
			= globalSettings.getBootSectorSetting().getValue()
			? BootBlocks::dos2BootBlock
			: BootBlocks::dos1BootBlock;
		memcpy(bootBlock, bootSector, SECTOR_SIZE);
	}
	// make a clean initial disk
	cleandisk();
	if (!readCache()) {
		cleandisk(); //make possible reads undone
		// read directory and fill the fake disk
		while (struct dirent* d = dir.getEntry()) {
			string name(d->d_name);
			if ((name != bootBlockFileName) &&
			    (name != cachedSectorsFileName)) {
				// add file into fake dsk
				updateFileInDisk(name);
			}
		}
	} else {
		// Cache file was valid and loaded so add possible new files
		// we can ofcourse skip this since a 'files' in the MSX will do
		// this in any case, but I like it more here :-)
		scanHostDir();
	}

	// OLD CODE: read the cached sectors
	//TODO: we should check if the other files have changed since we
	//      wrote the cached sectors, this could invalided the cache!
	/*
	try {
		File file(hostDir + '/' + cachedSectorsFileName);
		unsigned num = file.getSize() / (SECTOR_SIZE + sizeof(unsigned));
		for (unsigned i = 0; i < num; ++i) {
			unsigned sector;
			file.read(reinterpret_cast<byte*>(&sector), sizeof(unsigned));
			if (sector == 0) {
				// cached sector is 0, this should be impossible!
			} else if (sector < (1 + 2 * SECTORS_PER_FAT)) {
				// cached sector is FAT sector read from fat1
				unsigned f = (sector - 1) % SECTORS_PER_FAT;
				file.read(fat + f * SECTOR_SIZE, SECTOR_SIZE);
			} else if (sector < 14) {
				// cached sector is DIR sector
				unsigned d = sector - (1 + 2 * SECTORS_PER_FAT);
				for (int j = 0; j < 16; ++j) {
					byte* buf = reinterpret_cast<byte*>(
						&mapdir[d * 16 + j].msxinfo);
					file.read(buf, SECTOR_SIZE / 16);
				}
			} else {
				//regular cached sector
				cachedSectors[sector].resize(SECTOR_SIZE);
				file.read(&cachedSectors[sector][0], SECTOR_SIZE);
				sectormap[sector].usage = CACHED;
			}
		}
	} catch (FileException& e) {
		// couldn't open/read cached sector file
	}
	*/
}

DirAsDSK::~DirAsDSK()
{
	// write cached sectors to a file
	if (globalSettings.getPersistentDirAsDSKSetting().getValue()) {
		saveCache();
	}
}

void DirAsDSK::readLogicalSector(unsigned sector, byte* buf)
{
	debug("DirAsDSK::readLogicalSector: %i ", sector);
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
		//copy correct sector from FAT

		// quick-and-dirty:
		// we check all files in the faked disk for altered filesize
		// remapping each fat entry to its direntry and do some bookkeeping
		// to avoid multiple checks will probably be slower than this
		for (int i = 0; i < 112; ++i) {
			if (mapdir[i].inUse()) {
				checkAlterFileInDisk(i);
			}
		}

		sector = (sector - 1) % SECTORS_PER_FAT;
		memcpy(buf, fat + sector * SECTOR_SIZE, SECTOR_SIZE);

	} else if (sector < 14) {
		// create correct DIR sector
		sector -= (1 + 2 * SECTORS_PER_FAT);
		int dirCount = sector * 16;
		//check if there are new files on the HOST OS when we read this sector
		if (dirCount == 0) {
			scanHostDir();
		}
		// TODO [wouter]: This loop seems wrong to me:
		//   I think mapdir[i] should be mapdir[dirCount]
		for (int i = 0; i < 16; ++i) {
			if (mapdir[i].inUse()) {
				checkAlterFileInDisk(dirCount);
			}
			memcpy(buf, &(mapdir[dirCount++].msxinfo), 32);
			buf += 32;
		}

	} else {
		// else get map from sector to file and read correct block
		// folowing same numbering as FAT eg. first data block is cluster 2
		if (sectormap[sector].usage == CLEAN) {
			//return an 'empty' sector
			// 0xE5 is the value used on the Philips VG8250
			memset(buf, 0xE5, SECTOR_SIZE);
		} else if (sectormap[sector].usage == CACHED) {
			memcpy(buf, &cachedSectors[sector][0], SECTOR_SIZE);
		} else {
			// first copy cached data
			// in case (end of) file only fills partial sector
			memcpy(buf, &cachedSectors[sector][0], SECTOR_SIZE);
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
				memcpy(&cachedSectors[sector][0], buf, SECTOR_SIZE);
			} catch (FileException& e) {
				// couldn't open/read cached sector file
			}
		}
	}
}

void DirAsDSK::checkAlterFileInDisk(const string& filename)
{
	for (int i = 0; i < 112; ++i) {
		if (mapdir[i].shortname == filename) {
			checkAlterFileInDisk(i);
		}
	}
}

void DirAsDSK::checkAlterFileInDisk(int dirindex)
{
	if (!mapdir[dirindex].inUse()) {
		return;
	}

	string fullfilename = hostDir + '/' + mapdir[dirindex].shortname;
	struct stat fst;
	if (stat(fullfilename.c_str(), &fst) == 0) {
		if (mapdir[dirindex].filesize != fst.st_size) {
			// changed filesize
			updateFileInDisk(dirindex);
		}
	} else {
		//file can not be stat'ed => assume it has been deleted
		//and thus delete it from the MSX DIR sectors by marking
		//the first filename char as 0xE5
		debug(" host os file deleted ? %s\n", mapdir[dirindex].shortname.c_str());
		mapdir[dirindex].msxinfo.filename[0] = 0xE5;
		mapdir[dirindex].shortname.clear();
		// TODO [wouter]: Shouldn't we also clean the FAT?
	}
}

void DirAsDSK::updateFileInDisk(int dirindex)
{
	// compute time/date stamps
	string fullfilename = hostDir + '/' + mapdir[dirindex].shortname;
	struct stat fst;
	if (stat(fullfilename.c_str(), &fst)) {
		// TODO
	}
	struct tm* mtim = localtime(&(fst.st_mtime));
	int t1 = mtim
	       ? (mtim->tm_sec >> 1) + (mtim->tm_min << 5) +
	         (mtim->tm_hour << 11)
	       : 0;
	setLE16(mapdir[dirindex].msxinfo.time, t1);
	int t2 = mtim
	       ? mtim->tm_mday + ((mtim->tm_mon + 1) << 5) +
	         ((mtim->tm_year + 1900 - 1980) << 9)
	       : 0;
	setLE16(mapdir[dirindex].msxinfo.date, t2);

	int fsize = fst.st_size;
	mapdir[dirindex].filesize = fsize;
	int curcl = getLE16(mapdir[dirindex].msxinfo.startcluster);
	// if there is no cluster assigned yet to this file, then find a free cluster
	bool followFATClusters = true;
	if (curcl == 0) {
		followFATClusters = false;
		curcl = findFirstFreeCluster();
		setLE16(mapdir[dirindex].msxinfo.startcluster, curcl);
	}

	unsigned size = fsize;
	int prevcl = 0;
	File file(fullfilename, File::CREATE);

	while (size && (curcl <= MAX_CLUSTER)) {
		unsigned logicalSector = clusterToSector(curcl);
		sectormap[logicalSector].usage = MIXED;
		sectormap[logicalSector].dirEntryNr = dirindex;
		sectormap[logicalSector].fileOffset = fsize - size;
		cachedSectors[logicalSector].resize(SECTOR_SIZE);
		byte* buf = reinterpret_cast<byte*>(
		                        &cachedSectors[logicalSector][0]);
		memset(buf, 0, SECTOR_SIZE); // in case (end of) file only fills partial sector
		file.seek(sectormap[logicalSector].fileOffset);
		file.read(buf, std::min<int>(size, SECTOR_SIZE));

		size -= (size > SECTOR_SIZE) ? SECTOR_SIZE : size;

		if (size) {
			//fill next sector if there is data left
			sectormap[++logicalSector].dirEntryNr = dirindex;
			sectormap[logicalSector].usage = MIXED;
			sectormap[logicalSector].fileOffset = fsize - size;
			cachedSectors[logicalSector].resize(SECTOR_SIZE);
			byte* buf = reinterpret_cast<byte*>(
						&cachedSectors[logicalSector][0]);
			memset(buf, 0, SECTOR_SIZE); // in case (end of) file only fills partial sector
			file.seek(sectormap[logicalSector].fileOffset);
			file.read(buf, std::min<int>(size, SECTOR_SIZE));
			size -= (size > SECTOR_SIZE) ? SECTOR_SIZE : size;
		}

		if (prevcl) {
			writeFAT(prevcl, curcl);
		}
		prevcl = curcl;

		//now we check if we continue in the current clusterstring
		//or need to allocate extra unused blocks
		if (followFATClusters) {
			curcl = readFAT(curcl);
			if (curcl == EOF_FAT) {
				followFATClusters = false;
				curcl = findFirstFreeCluster();
			}
		} else {
			do {
				++curcl;
			} while ((curcl <= MAX_CLUSTER) && readFAT(curcl));
		}
		// Continuing at cluster 'curcl'
	}
	if ((size == 0) && (curcl <= MAX_CLUSTER)) {
		// TODO: check what an MSX does with filesize zero and fat allocation
		if (prevcl == 0) {
			writeFAT(curcl, EOF_FAT);
		} else {
			writeFAT(prevcl, EOF_FAT);
		}

		//clear remains of FAT if needed
		if (followFATClusters) {
			while ((curcl <= MAX_CLUSTER) && (curcl != 0) &&
			       (curcl != EOF_FAT)) {
				prevcl = curcl;
				curcl = readFAT(curcl);
				writeFAT(prevcl, 0);
				unsigned logicalSector = clusterToSector(prevcl);
				sectormap[logicalSector].usage = CLEAN;
				sectormap[logicalSector].dirEntryNr = 0;
				sectormap[logicalSector++].fileOffset = 0;
				sectormap[logicalSector].usage = CLEAN;
				sectormap[logicalSector].dirEntryNr = 0;
				sectormap[logicalSector].fileOffset = 0;
			}
			writeFAT(prevcl, 0);
			unsigned logicalSector = clusterToSector(prevcl);
			sectormap[logicalSector].usage = CLEAN;
			sectormap[logicalSector].dirEntryNr = 0;
			sectormap[logicalSector].fileOffset = 0;
		}
	} else {
		//TODO: don't we need a EOF_FAT in this case as well ?
		// find out and adjust code here
		cliComm.printWarning("Fake Diskimage full: " +
		                     mapdir[dirindex].shortname + " truncated.");
	}
	//write (possibly truncated) file size
	setLE32(mapdir[dirindex].msxinfo.size, fsize - size);
}

void DirAsDSK::truncateCorrespondingFile(const int dirindex)
{
	if (!mapdir[dirindex].inUse()) {
		//a total new file so we create the new name from the msx name
		byte* buf = reinterpret_cast<byte*>(
			mapdir[dirindex].msxinfo.filename);
		//special case file is deleted but becuase rest of dirEntry changed
		//while file is still deleted...
		if (buf[0] == 0xE5) return;
		string shname = condenseName(buf);
		mapdir[dirindex].shortname = shname;
		debug("      truncateCorrespondingFile of new Host OS file\n");
	}
	string fullfilename = hostDir + '/' + mapdir[dirindex].shortname;
	debug("      truncateCorrespondingFile %s\n", fullfilename.c_str());
	File file(fullfilename, File::CREATE);
	int cursize = getLE32(mapdir[dirindex].msxinfo.size);
	file.truncate(cursize);
	mapdir[dirindex].filesize = cursize;
}

void DirAsDSK::extractCacheToFile(const int dirindex)
{
	if (!mapdir[dirindex].inUse()) {
		//a total new file so we create the new name from the msx name
		byte* buf = reinterpret_cast<byte*>(
			mapdir[dirindex].msxinfo.filename);
		//special case: the file is deleted and we get here because the
		//startcluster is still intact
		if (buf[0] == 0xE5) return;
		string shname = condenseName(buf);
		mapdir[dirindex].shortname = shname;
	}
	string fullfilename = hostDir + '/' + mapdir[dirindex].shortname;
	File file(fullfilename,File::CREATE);
	int curcl = getLE16(mapdir[dirindex].msxinfo.startcluster);
	//if we start a new file the currentcluster can be set to zero

	unsigned cursize = getLE32(mapdir[dirindex].msxinfo.size);
	unsigned offset = 0;
	//if the size is set to zero then we truncate to zero and leave
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
				//transfer data
				byte* buf = reinterpret_cast<byte*>(
						&cachedSectors[logicalSector][0]);
				file.seek(offset);
				//OLD: int writesize = ((cursize - offset) > SECTOR_SIZE) ? SECTOR_SIZE : (cursize - offset);
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
}


void DirAsDSK::writeLogicalSector(unsigned sector, const byte* buf)
{
	// is this actually needed ?
	if (syncMode == GlobalSettings::SYNC_READONLY) return;

	debug("DirAsDSK::writeLogicalSector: %i ", sector);
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

	//
	//Regular sectors
	//
	if (sector >= 14) {
		// first and before all else buffer everything !!!!
		//check if cachedSectors exists, if not assign memory.
		cachedSectors[sector].resize(SECTOR_SIZE);
		memcpy(&cachedSectors[sector][0], buf, SECTOR_SIZE);

		// if in SYNC_CACHEDWRITE then simply mark sector as cached and be done with it
		if (syncMode == GlobalSettings::SYNC_CACHEDWRITE) {
			//change to a regular cached sector
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
			} catch (FileException& e) {
				cliComm.printWarning("Couldn't write to file.");
			}
		} else {
			// indicate data is cached, it might be CACHED already or it was CLEAN
			sectormap[sector].usage = CACHED;
		}
		return;
	}

	//
	//Special sectors
	//
	if (sector == 0) {
		// copy buffer into our fake bootsector
		memcpy(bootBlock, buf, SECTOR_SIZE);
		bootSectorWritten = true;

	} else if (sector < (1 + 2 * SECTORS_PER_FAT)) {
		//copy to correct sector from FAT

		//during formatting sectors > 1+SECTORS_PER_FAT are empty (all
		//bytes are 0) so we would erase the 3 bytes indentifier at the
		//beginning of the FAT !!
		//Since the two FATs should be identical after "normal" usage,
		//we use writes to the second FAT (which should be an exact backup
		//of the first FAT to detect changes (files might have
		//grown/shrunk/added) so that they can be passed on to the HOST-OS
		if (sector < (1 + SECTORS_PER_FAT)) {
			sector = (sector - 1) % SECTORS_PER_FAT;
			memcpy(fat + sector * SECTOR_SIZE, buf, SECTOR_SIZE);
			return;
		}
		//writes to the second FAT so we check for changes
		//but we fully ignore the sectors aftwerwards (see remark
		//about identifier bytes above)

		int startcluster = std::max<int>(2, int(((sector - 1 - SECTORS_PER_FAT) * 2) / 3));
		int endcluster = std::min<int>(startcluster + 342, int((SECTOR_SIZE * SECTORS_PER_FAT * 2) / 3));
		for (int i = startcluster; i < endcluster; ++i) {
			if (readFAT(i) != readFAT2(i)) {
				updateFileFromAlteredFatOnly(i);
			}
		}

		sector = (sector - 1) % SECTORS_PER_FAT;
		memcpy(fat2 + sector * SECTOR_SIZE, buf, SECTOR_SIZE);

	} else {
		//create correct DIR sector

		// We assume that the dir entry is updatet as latest: So the
		// fat and actual sectordata should already contain the correct
		// data. Most MSX disk roms honour this behaviour for normal
		// fileactions. Of course some diskcaching programs and disk
		// optimizers can abandon this behaviour and in such case the
		// logic used here goes haywire!!
		sector -= (1 + 2 * SECTORS_PER_FAT);
		int dirCount = sector * 16;
		for (int i = 0; i < 16; ++i) {
			//TODO check if changed and take apropriate actions if needed
			if (memcmp(mapdir[dirCount].msxinfo.filename, buf, 32) != 0) {
				// dir entry has changed
				//mapdir[dirCount].msxinfo.filename[0] == 0xE5
				//if already deleted....

				//the 3 vital informations needed
				bool chgName = memcmp(mapdir[dirCount].msxinfo.filename, buf, 11) != 0;
				bool chgClus = getLE16(mapdir[dirCount].msxinfo.startcluster) != getLE16(&buf[26]);
				bool chgSize = getLE32(mapdir[dirCount].msxinfo.size) != getLE32(&buf[28]);
				/*
				here are the possible combinations encountered in normal usage so far,
				the bool order is  chgName chgClus chgSize
				0 0 0 : nothing changed for this direntry... :-)
				0 0 1 : File has grown or shrunk
				0 1 1 : name remains but size and cluster changed => second step in new file creation (cheked on NMS8250)
				1 0 0 : a) Only create a name and size+cluster still zero => first step in new file creation (cheked on NMS8250)
					   if we start a new file the currentcluster can be set to zero
					   FI: on a Philips NMS8250 in Basic try : copy"con"to:"a.txt"
					   it will first write the first 7 sectors, then the data, then
					   update the 7 first sectors with correct data
					b) name changed, others remained unchanged => file renamed
					c) first char of name changed to 0xE5, others remained unchanged => file deleted
				1 1 1 : a new file has been created (as done by a Panasonic FS A1GT)
				*/



				debug("  dircount %i filename: %s\n", dirCount, mapdir[dirCount].shortname.c_str());
				debug("  chgName: %i chgClus: %i chgSize: %i\n", chgName, chgClus, chgSize);
				debug("  Old start %i   New start %i\n", getLE16(mapdir[dirCount].msxinfo.startcluster), getLE16(&buf[26]));
				debug("  Old size %i  New size %i\n\n", getLE32(mapdir[dirCount].msxinfo.size), getLE32(&buf[28]));

				if (chgName && !chgClus && !chgSize) {
					if (buf[0] == 0xE5 && syncMode == GlobalSettings::SYNC_FULL) {
						// dir entry has been deleted
						// delete file from host OS and 'clear' all sector data pointing to this HOST OS file
						string fullfilename = hostDir + '/' + mapdir[dirCount].shortname;
						unlink(fullfilename.c_str());
						for (int i = 14; i < 1440; ++i) {
							if (sectormap[i].dirEntryNr == dirCount) {
								 sectormap[i].usage = CACHED;
							}
						}

						mapdir[dirCount].shortname.clear();
					} else if (buf[0] != 0xE5 && (syncMode == GlobalSettings::SYNC_FULL || syncMode == GlobalSettings::SYNC_NODELETE)) {
						int newClus = getLE16(&buf[26]);
						int newSize = getLE32(&buf[28]);
						string shname = condenseName(buf);
						string newfilename = hostDir + '/' + shname;
						if (newClus == 0 && newSize == 0) {
							//creating a new file
							mapdir[dirCount].shortname = shname;
							// we do not need to write anything since the MSX will update this later when the size is altered
							try {
								File file(newfilename, File::TRUNCATE);
							} catch (FileException& e) {
								cliComm.printWarning(
									"Couldn't create new file.");
							}

						} else {
							//rename file on host OS
							string oldfilename = hostDir + '/' + mapdir[dirCount].shortname;
							if (rename(oldfilename.c_str(), newfilename.c_str()) == 0) {
								//renaming on host OS succeeeded
								mapdir[dirCount].shortname = shname;
							}
						}
					} else {
						cliComm.printWarning(
							"File has been renamed in emulated disk, Host OS file (" +
							mapdir[dirCount].shortname + ") remains untouched!");
					}
				}

				if (chgSize) {
					// Cluster might have changed is this is a new file so chgClu is ignored
					// content changed, extract the file
					// Als name might have been changed (on a turbo R the single shot Dir update when creating new files)
					if (getLE32(mapdir[dirCount].msxinfo.size) < getLE32(&buf[28])) {
						// new size is bigger, file has grown
						memcpy(&(mapdir[dirCount].msxinfo), buf, 32);
						extractCacheToFile(dirCount);
					} else {
						// new size is smaller, file has been reduced
						// luckily the entire file is in cache, we need this since onm some
						// MSX models during a copy from file to overwrite an existing file
						// the sequence is that first the actual data is written and then
						// the size is set to zero before it is set to the new value. If we
						// didn't cache this, then all the 'mapped' sectors would lose their
						// value

						memcpy(&(mapdir[dirCount].msxinfo), buf, 32);
						truncateCorrespondingFile(dirCount);

						if (getLE32(&buf[28]) != 0) {
							extractCacheToFile(dirCount); // see copy remark above
						}
					}
				}
				if (!chgName && chgClus && !chgSize) {
					cliComm.printWarning("this case of writing to DIR is  not yet implemented since we haven't encountered it in real life yet.");
				}
				//for now simply blindly take over info
				memcpy(&(mapdir[dirCount].msxinfo), buf, 32);
			}
			++dirCount;
			buf += 32;
		}
	}
}

void DirAsDSK:: updateFileFromAlteredFatOnly(int somecluster)
{
	// First look for the first cluster in this chain
	unsigned startcluster = somecluster;
	int i = 2;
	while (i < int((SECTOR_SIZE * SECTORS_PER_FAT * 2) / 3)) {
		if (readFAT(i) == startcluster) {
			startcluster = i;
			i = 1;
		}
		++i;
	}
	// Find the corresponding direntry if any
	// and extract file based on new clusterchain
	for (i = 0; i < 112; ++i) {
		if (startcluster == getLE16(mapdir[i].msxinfo.startcluster)) {
			extractCacheToFile(i);
			break;
		}
	}

	// from startcluster and somecluster on, update fat2 so that the check
	// in writeLogicalSecor doesn't call this routine again for the same
	// file-clusterfchain
	int curcl = startcluster;
	while ((curcl <= MAX_CLUSTER) && (curcl != EOF_FAT) && (curcl > 1)) {
		int next = readFAT(curcl);
		writeFAT2(curcl,next);
		curcl = next;
	}
	curcl = somecluster;
	while ((curcl <= MAX_CLUSTER) && (curcl != EOF_FAT) && (curcl > 1)) {
		int next = readFAT(curcl);
		writeFAT2(curcl,next);
		curcl = next;
	}
}


string DirAsDSK::condenseName(const byte* buf)
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

bool DirAsDSK::writeProtected()
{
	return (syncMode != GlobalSettings::SYNC_READONLY ? false : true);
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
		addFileToDSK(filename);
	} else {
		//really update file
		checkAlterFileInDisk(fullfilename);
	}
}

void DirAsDSK::addFileToDSK(const string& filename)
{
	//get emtpy dir entry
	int dirindex = 0;
	while (mapdir[dirindex].inUse()) {
		if (++dirindex == 112) {
			cliComm.printWarning(
				"Couldn't add " + filename +
				": root dir full");
			return;
		}
	}

	// create correct MSX filename
	string MSXfilename = makeSimpleMSXFileName(filename);
	if (checkMSXFileExists(MSXfilename)) {
		//TODO: actually should increase vfat abrev if possible!!
		cliComm.printWarning(
			"Couldn't add " + filename + ": MSX name " +
			MSXfilename + " existed already");
		return;
	}

	// fill in native file name
	mapdir[dirindex].shortname = filename;
	// fill in MSX file name
	memcpy(&(mapdir[dirindex].msxinfo.filename), MSXfilename.c_str(), 11);

	updateFileInDisk(dirindex);
}

} // namespace openmsx
