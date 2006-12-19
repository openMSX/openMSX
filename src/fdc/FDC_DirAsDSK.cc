// $Id$

#include "FDC_DirAsDSK.hh"
#include "CliComm.hh"
#include "BootBlocks.hh"
#include "GlobalSettings.hh"
#include "EnumSetting.hh"
#include <algorithm>
#include <cassert>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <dirent.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>

using std::string;

namespace openmsx {

static const int EOF_FAT = 0xFFF;
static const int NODIRENTRY   = 4000;
static const int CACHEDSECTOR = 4001;
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


// read FAT-entry from FAT in memory
word FDC_DirAsDSK::readFAT(word cluster)
{
	const byte* p = fat + (cluster * 3) / 2;
	return (cluster & 1)
	     ? (p[0] >> 4) + (p[1] << 4)
	     : p[0] + ((p[1] & 0x0F) << 8);
}

// write an entry to FAT in memory
void FDC_DirAsDSK::writeFAT(word cluster, word val)
{
	byte* p = fat + (cluster * 3) / 2;
	if (cluster & 1) {
		p[0] = (p[0] & 0x0F) + (val << 4);
		p[1] = val >> 4;
	} else {
		p[0] = val;
		p[1] = (p[1] & 0xF0) + ((val >> 8) & 0x0F);
	}
}

int FDC_DirAsDSK::findFirstFreeCluster()
{
	int cluster = 2;
	while ((cluster <= MAX_CLUSTER) && readFAT(cluster)) {
		++cluster;
	}
	return cluster;
}

// check if a filename is used in the emulated MSX disk
bool FDC_DirAsDSK::checkMSXFileExists(const string& msxfilename)
{
	//TODO: complete this
	string dir, file;
	StringOp::splitOnLast(msxfilename, "/", dir, file);

	for (int i = 0; i < 112; ++i) {
		if (strncmp((const char*)(mapdir[i].msxinfo.filename),
			    file.c_str(), 11) == 0) {
			return true;
		}
	}
	return false;
}

// check if a file is already mapped into the fake DSK
bool FDC_DirAsDSK::checkFileUsedInDSK(const string& fullfilename)
{
	for (int i = 0; i < 112; ++i) {
		if (mapdir[i].filename == fullfilename) {
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
static string makeSimpleMSXFileName(const string& fullfilename)
{
	string dir, fullfile;
	StringOp::splitOnLast(fullfilename, "/", dir, fullfile);

	transform(fullfile.begin(), fullfile.end(), fullfile.begin(), toMSXChr);

	string file, ext;
	StringOp::splitOnLast(fullfile, ".", file, ext);
	if (file.empty()) swap(file, ext);

	file.resize(8, ' ');
	ext .resize(3, ' ');
	return file + ext;
}

FDC_DirAsDSK::FDC_DirAsDSK(CliComm& cliComm_, GlobalSettings& globalSettings,
                           const string& fileName)
	: SectorBasedDisk(fileName)
	, cliComm(cliComm_)
{
	// Here we create the fake diskimages based upon the files that can be
	// found in the 'fileName' directory
	DIR* dir = opendir(fileName.c_str());
	if (dir == NULL) {
		throw MSXException("Not a directory");
	}

	// store filename as chroot dir for the msx disk
	MSXrootdir = fileName;

	// First create structure for the fake disk
	nbSectors = 1440; // asume a DS disk is used
	sectorsPerTrack = 9;
	nbSides = 2;

	// set default boot sector
	const byte* bootSector = globalSettings.getBootSectorSetting().getValue()
	                       ? BootBlocks::dos2BootBlock
	                       : BootBlocks::dos1BootBlock;
	memcpy(bootBlock, bootSector, SECTOR_SIZE);

	// try to read boot block from file
	struct stat fst;
	bool readBootBlockFromFile = false;
	string tmpfilename = MSXrootdir + '/' + bootBlockFileName;
	if (stat(tmpfilename.c_str(), &fst) == 0) {
		if (fst.st_size == (int)SECTOR_SIZE) {
			readBootBlockFromFile = true;
			if (FILE* file = fopen(tmpfilename.c_str(), "rb")) {
				fread(bootBlock, 1, SECTOR_SIZE, file);
				fclose(file);
			}
		}
	}

	// assign empty directory entries
	for (int i = 0; i < 112; ++i) {
		memset(&mapdir[i].msxinfo, 0, sizeof(MSXDirEntry));
	}

	// Make a full clear FAT
	saveCachedSectors = false;
	memset(fat, 0, SECTOR_SIZE * SECTORS_PER_FAT);
	// for some reason the first 3bytes are used to indicate the end of a
	// cluster, making the first available cluster nr 2. Some sources say
	// that this indicates the disk format and fat[0] should 0xF7 for
	// single sided disk, and 0xF9 for double sided disks
	// TODO: check this :-)
	fat[0] = 0xF9;
	fat[1] = 0xFF;
	fat[2] = 0xFF;

	//clear the sectormap so that they all point to 'clean' sectors
	for (int i = 0; i < 1440; ++i) {
		sectormap[i].dirEntryNr = NODIRENTRY;
	}

	//read directory and fill the fake disk
	struct dirent* d = readdir(dir);
	while (d) {
		string name(d->d_name);
		//TODO: if bootsector read from file we should skip this file
		if (!(readBootBlockFromFile && (name == bootBlockFileName)) &&
		    (name != cachedSectorsFileName)) {
			// add file into fake dsk
			updateFileInDSK(fileName + '/' + name);
		}
		d = readdir(dir);
	}
	closedir(dir);

	// read the cached sectors
	//TODO: we should check if the other files have changed since we
	//      wrote the cached sectors, this could invalided the cache!
	tmpfilename = MSXrootdir + '/' + cachedSectorsFileName;
	if (stat(tmpfilename.c_str(), &fst) == 0) {
		if ((fst.st_size % (SECTOR_SIZE + sizeof(int))) == 0) {
			if (FILE* file = fopen(tmpfilename.c_str(), "rb")) {
				saveCachedSectors = true; // make sure that we don't destroy the cache when destroying this object
				int i;
				while (fread(&i, 1, sizeof(int), file)) { // feof(file)

					if (i == 0) {
						// cached sector is 0, this should be impossible!
					} else if (i < (1 + 2 * SECTORS_PER_FAT)) {
						// cached sector is FAT sector
						i = (i - 1) % SECTORS_PER_FAT;
						fread(fat + i * SECTOR_SIZE, 1, SECTOR_SIZE, file);
					} else if (i < 14) {
						// cached sector is DIR sector
						i -= (1 + 2 * SECTORS_PER_FAT);
						int dirCount = i * 16;
						for (int j = 0; j < 16; ++j) {
							fread(&(mapdir[dirCount++].msxinfo), 1, 32, file);
						}
					} else {
						//regular cached sector
						byte* tmpbuf = (byte*)malloc(SECTOR_SIZE);
						fread(tmpbuf, 1, SECTOR_SIZE, file);
						cachedSectors[i] = tmpbuf;
						sectormap[i].dirEntryNr = CACHEDSECTOR;
					}
				}
				fclose(file);
			} else {
				cliComm.printWarning(
					"Couldn't open file " + tmpfilename);
			}
		} else {
			// wrong filesize
		}
	} else {
		// couldn't stat cached sector file
	}
}

FDC_DirAsDSK::~FDC_DirAsDSK()
{
	//writing the cached sectors to a file
	string filename = MSXrootdir + '/' + cachedSectorsFileName;

	if (saveCachedSectors) {
		if (FILE* file = fopen(filename.c_str(), "wb")) {
			// if we have cached sectors we need also to save the fat and dir sectors!
			for (int i = 1; i <= 14; ++i) {
				byte tmpbuf[SECTOR_SIZE];
				readLogicalSector(i, tmpbuf);
				fwrite(&i, 1, sizeof(int), file);
				fwrite(tmpbuf, 1, SECTOR_SIZE, file);
			}

			for (CachedSectors::const_iterator it = cachedSectors.begin();
			     it != cachedSectors.end(); ++it) {
				fwrite(&(it->first), 1, sizeof(int), file);
				fwrite(it->second, 1, SECTOR_SIZE, file);
			}
			fclose(file);
		} else {
			cliComm.printWarning(
				"Couldn't create cached sector file " + filename);
		}
	} else {
		// remove cached sectors file if it exists
		unlink(filename.c_str());
	}

	//cleaning up memory from cached sectors
	for (CachedSectors::const_iterator it = cachedSectors.begin();
	     it != cachedSectors.end(); ++it) {
		free(it->second);
	}
}

void FDC_DirAsDSK::readLogicalSector(unsigned sector, byte* buf)
{
	if (sector == 0) {
		//copy our fake bootsector into the buffer
		memcpy(buf, bootBlock, SECTOR_SIZE);

	} else if (sector < (1 + 2 * SECTORS_PER_FAT)) {
		//copy correct sector from FAT

		// quick-and-dirty:
		// we check all files in the faked disk for altered filesize
		// remapping each fat entry to its direntry and do some bookkeeping
		// to avoid multiple checks will probably be slower then this
		for (int i = 0; i < 112; ++i) {
			if (!mapdir[i].filename.empty()) {
				checkAlterFileInDisk(i);
			}
		}

		sector = (sector - 1) % SECTORS_PER_FAT;
		memcpy(buf, fat + sector * SECTOR_SIZE, SECTOR_SIZE);

	} else if (sector < 14) {
		//create correct DIR sector
		sector -= (1 + 2 * SECTORS_PER_FAT);
		int dirCount = sector * 16;
		for (int i = 0; i < 16; ++i) {
			if (!mapdir[i].filename.empty()) {
				checkAlterFileInDisk(dirCount);
			}
			memcpy(buf, &(mapdir[dirCount++].msxinfo), 32);
			buf += 32;
		}

	} else {
		// else get map from sector to file and read correct block
		// folowing same numbering as FAT eg. first data block is cluster 2
		if (sectormap[sector].dirEntryNr == NODIRENTRY) {
			//return an 'empty' sector
			// 0xE5 is the value used on the Philips VG8250
			memset(buf, 0xE5, SECTOR_SIZE);
		} else if (sectormap[sector].dirEntryNr == CACHEDSECTOR) {
			memcpy(buf, cachedSectors[sector], SECTOR_SIZE);
		} else {
			// open file and read data
			int offset = sectormap[sector].fileOffset;
			string tmp = mapdir[sectormap[sector].dirEntryNr].filename;
			checkAlterFileInDisk(tmp);
			if (FILE* file = fopen(tmp.c_str(), "rb")) {
				fseek(file, offset, SEEK_SET);
				fread(buf, 1, SECTOR_SIZE, file);
				fclose(file);
			}
		}
	}
}

void FDC_DirAsDSK::checkAlterFileInDisk(const string& fullfilename)
{
	for (int i = 0; i < 112; ++i) {
		if (mapdir[i].filename == fullfilename) {
			checkAlterFileInDisk(i);
		}
	}
}

void FDC_DirAsDSK::checkAlterFileInDisk(int dirindex)
{
	if (mapdir[dirindex].filename.empty()) {
		return;
	}

	struct stat fst;
	stat(mapdir[dirindex].filename.c_str(), &fst);
	if (mapdir[dirindex].filesize != fst.st_size) {
		// changed filesize
		updateFileInDisk(dirindex);
	}
}

void FDC_DirAsDSK::updateFileInDisk(int dirindex)
{
	// compute time/date stamps
	struct stat fst;
	stat(mapdir[dirindex].filename.c_str(), &fst);
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
	while (size && (curcl <= MAX_CLUSTER)) {
		int logicalSector = 14 + 2 * (curcl - 2);

		sectormap[logicalSector].dirEntryNr = dirindex;
		sectormap[logicalSector].fileOffset = fsize - size;
		size -= (size > SECTOR_SIZE) ? SECTOR_SIZE : size;

		if (size) {
			//fill next sector if there is data left
			sectormap[++logicalSector].dirEntryNr = dirindex;
			sectormap[logicalSector].fileOffset = fsize - size;
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
			while ((curcl <= MAX_CLUSTER) && (curcl != EOF_FAT)) {
				prevcl = curcl;
				curcl = readFAT(curcl);
				writeFAT(prevcl, 0);
				int logicalSector = 14 + 2 * (prevcl - 2);
				sectormap[logicalSector].dirEntryNr = NODIRENTRY;
				sectormap[logicalSector++].fileOffset = 0;
				sectormap[logicalSector].dirEntryNr = NODIRENTRY;
				sectormap[logicalSector].fileOffset = 0;
			}
			writeFAT(prevcl, 0);
			int logicalSector= 14 + 2 * (prevcl - 2);
			sectormap[logicalSector].dirEntryNr = NODIRENTRY;
			sectormap[logicalSector].fileOffset = 0;
		}
	} else {
		//TODO: don't we need a EOF_FAT in this case as well ?
		// find out and adjust code here
		cliComm.printWarning("Fake Diskimage full: " +
		                     mapdir[dirindex].filename + " truncated.");
	}
	//write (possibly truncated) file size
	setLE32(mapdir[dirindex].msxinfo.size, fsize - size);
}

void FDC_DirAsDSK::writeLogicalSector(unsigned sector, const byte* buf)
{
	if (sector == 0) {
		//copy buffer into our fake bootsector and safe into file
		memcpy(bootBlock, buf, SECTOR_SIZE);
		string filename = MSXrootdir + '/' + bootBlockFileName;
		if (FILE* file = fopen(filename.c_str(), "wb")) {
			fwrite(buf, 1, SECTOR_SIZE, file);
			fclose(file);
		} else {
			cliComm.printWarning(
				"Couldn't create bootsector file " + filename);
		}

	} else if (sector < (1 + 2 * SECTORS_PER_FAT)) {
		//copy to correct sector from FAT
		//make sure we write changed sectors to the cache file later on
		saveCachedSectors = true;

		//during formatting sectors > 1+SECTORS_PER_FAT are empty (all
		//bytes are 0) so we would erase the 3 bytes indentifier at the
		//beginning of the FAT !!
		//Since the two FATs should be identical after "normal" usage,
		//we simply ignore writes (for now, later we could cache them
		//perhaps)
		if (sector < (1 + SECTORS_PER_FAT)) {
			sector = (sector - 1) % SECTORS_PER_FAT;
			memcpy(fat + sector * SECTOR_SIZE, buf, SECTOR_SIZE);
		}

	} else if (sector < 14) {
		//make sure we write changed sectors to the cache file later on
		saveCachedSectors = true;
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
				bool chgName = memcmp(mapdir[dirCount].msxinfo.filename, buf, 11) == 0;
				bool chgClus = getLE16(mapdir[dirCount].msxinfo.startcluster) == getLE16(&buf[25]);
				bool chgSize = getLE32(mapdir[dirCount].msxinfo.size) == getLE32(&buf[27]);

				if (chgClus && chgName) {
					// Major change: new file started!
				} else {
					cliComm.printWarning(
						"! A unussual change has been performed on this disk\n"
						"! are you running a disk editor or optimizer, or maybe\n"
						"! a diskcache program\n"
						"! Do not use 'dir as disk' emulation while running these kind of programs!\n");
				}
				if (chgName && !chgClus && !chgSize) {
					if (buf[0] == 0xE5) {
					// dir entry has been deleted
					//TODO: What now, really remove entry
					//and clean sectors or keep around in
					//case of an undelete ?? Later seems
					//more real though, but is it safe
					//enough for host OS files when writing
					//sectors?
					} else {
						cliComm.printWarning(
							"File has been renamed in emulated disk, Host OS file (" +
							mapdir[dirCount].filename + ") remains untouched!");
					}
				}

				if (!chgName && !chgClus && chgSize) {
					// content changed, extract the file
				}
				//for now simply blindly take over info
				memcpy(&(mapdir[dirCount].msxinfo), buf, 32);
			} else {
				// entry not changed
			}
			++dirCount;
			buf += 32;
		}
		cliComm.printWarning(
			"writing to DIR not yet fully implemented !!!!");

	} else {
		// simply buffering everything for now, no write-through to host-OS
		//check if cachedSectors exists, if not assign memory.

		//make sure we write cached sectors to a file later on
		saveCachedSectors = true;

		if (cachedSectors[sector] == NULL) {
			byte* tmp = (byte*)malloc(SECTOR_SIZE);
			cachedSectors[sector] = tmp;
		}
		memcpy(cachedSectors[sector], buf, SECTOR_SIZE);
		sectormap[sector].dirEntryNr = CACHEDSECTOR;
	}
}

bool FDC_DirAsDSK::writeProtected()
{
	return false;
}

void FDC_DirAsDSK::updateFileInDSK(const string& fullfilename)
{
	struct stat fst;
	if (stat(fullfilename.c_str(), &fst)) {
		cliComm.printWarning("Error accessing " + fullfilename);
		return;
	}
	if (!S_ISREG(fst.st_mode)) {
		// we only handle regular files for now
		cliComm.printWarning("Not a regular file: " + fullfilename);
		return;
	}
	if (!checkFileUsedInDSK(fullfilename)) {
		// add file to fakedisk
		addFileToDSK(fullfilename);
	} else {
		//really update file
		checkAlterFileInDisk(fullfilename);
	}
}

void FDC_DirAsDSK::addFileToDSK(const string& fullfilename)
{
	//get emtpy dir entry
	int dirindex = 0;
	while (!mapdir[dirindex].filename.empty()) {
		if (++dirindex == 112) {
			cliComm.printWarning(
				"Couldn't add " + fullfilename +
				": root dir full");
			return;
		}
	}

	// create correct MSX filename
	string MSXfilename = makeSimpleMSXFileName(fullfilename);
	if (checkMSXFileExists(MSXfilename)) {
		//TODO: actually should increase vfat abrev if possible!!
		cliComm.printWarning(
			"Couldn't add " + fullfilename + ": MSX name " +
			MSXfilename + " existed already");
		return;
	}

	// fill in native file name
	mapdir[dirindex].filename = fullfilename;
	// fill in MSX file name
	memcpy(&(mapdir[dirindex].msxinfo.filename), MSXfilename.c_str(), 11);

	updateFileInDisk(dirindex);
}

} // namespace openmsx
