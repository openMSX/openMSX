// $Id$

#include "FDC_DirAsDSK.hh"
#include "CliComm.hh"

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
using std::transform;

/* macros to change DirEntries */
#define setsh(x,y) {x[0]=y;x[1]=y>>8;}
#define setlg(x,y) {x[0]=y;x[1]=y>>8;x[2]=y>>16;x[3]=y>>24;}

/* macros to read DirEntries */
#define rdsh(x) (x[0]+(x[1]<<8))
#define rdlg(x) (x[0]+(x[1]<<8)+(x[2]<<16)+(x[3]<<24))

namespace openmsx {

#define EOF_FAT 0xFFF /* signals EOF in FAT */
#define NODIRENTRY    4000
#define CACHEDSECTOR  4001

//Bootblock taken from a philips  nms8250 formatted disk
const string FDC_DirAsDSK::BootBlockFileName = ".sector.boot";
const string FDC_DirAsDSK::CachedSectorsFileName = ".sector.cache";
const byte FDC_DirAsDSK::DefaultBootBlock[] =
{
0xeb,0xfe,0x90,0x4e,0x4d,0x53,0x20,0x32,0x2e,0x30,0x50,0x00,0x02,0x02,0x01,0x00,
0x02,0x70,0x00,0xa0,0x05,0xf9,0x03,0x00,0x09,0x00,0x02,0x00,0x00,0x00,0xd0,0xed,
0x53,0x59,0xc0,0x32,0xd0,0xc0,0x36,0x56,0x23,0x36,0xc0,0x31,0x1f,0xf5,0x11,0xab,
0xc0,0x0e,0x0f,0xcd,0x7d,0xf3,0x3c,0xca,0x63,0xc0,0x11,0x00,0x01,0x0e,0x1a,0xcd,
0x7d,0xf3,0x21,0x01,0x00,0x22,0xb9,0xc0,0x21,0x00,0x3f,0x11,0xab,0xc0,0x0e,0x27,
0xcd,0x7d,0xf3,0xc3,0x00,0x01,0x58,0xc0,0xcd,0x00,0x00,0x79,0xe6,0xfe,0xfe,0x02,
0xc2,0x6a,0xc0,0x3a,0xd0,0xc0,0xa7,0xca,0x22,0x40,0x11,0x85,0xc0,0xcd,0x77,0xc0,
0x0e,0x07,0xcd,0x7d,0xf3,0x18,0xb4,0x1a,0xb7,0xc8,0xd5,0x5f,0x0e,0x06,0xcd,0x7d,
0xf3,0xd1,0x13,0x18,0xf2,0x42,0x6f,0x6f,0x74,0x20,0x65,0x72,0x72,0x6f,0x72,0x0d,
0x0a,0x50,0x72,0x65,0x73,0x73,0x20,0x61,0x6e,0x79,0x20,0x6b,0x65,0x79,0x20,0x66,
0x6f,0x72,0x20,0x72,0x65,0x74,0x72,0x79,0x0d,0x0a,0x00,0x00,0x4d,0x53,0x58,0x44,
0x4f,0x53,0x20,0x20,0x53,0x59,0x53,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00
};

// read FAT-entry from FAT in memory
word FDC_DirAsDSK::ReadFAT(word clnr)
{ 
  byte *P = FAT + (clnr * 3) / 2;
  return (clnr & 1) ?
         (P[0] >> 4) + (P[1] << 4) :
	 P[0] + ((P[1] & 0x0F) << 8);
}

// write an entry to FAT in memory
void FDC_DirAsDSK::WriteFAT(word clnr, word val)
{
	PRT_DEBUG("Updating FAT "<<clnr<<" will point to "<<val);
	byte* P=FAT + (clnr * 3) / 2;
	if (clnr & 1) { 
		P[0] = (P[0] & 0x0F) + (val << 4);
		P[1] = val >> 4;
	} else {
		P[0] = val;
		P[1] = (P[1] & 0xF0) + ((val >> 8) & 0x0F);
	}
}

int FDC_DirAsDSK::findFirstFreeCluster()
{
	int cluster=2;
	while ((cluster <= MAX_CLUSTER) && ReadFAT(cluster)) {
		cluster++;
	};
	return cluster;
}

// check if a filename is used in the emulated MSX disk
bool FDC_DirAsDSK::checkMSXFileExists(const string& msxfilename)
{
	//TODO: complete this
	string::size_type pos = msxfilename.find_last_of('/');
	string tmp;
	if (pos != string::npos) {
		tmp = msxfilename.substr(pos + 1);
	} else {
		tmp = msxfilename;
	}

	for (int i = 0; i < 112; i++) {
		if (strncmp((const char*)(mapdir[i].msxinfo.filename),
			    tmp.c_str(), 11) == 0 ) {
			return true;
		}
	}
	return false;
}

// check if a file is already mapped into the fake DSK
bool FDC_DirAsDSK::checkFileUsedInDSK(const string& fullfilename)
{
	for (int i = 0; i < 112; i++) {
		if (mapdir[i].filename == fullfilename) {
			return true;
		}
	}
	return false;
}

// create an MSX filename 8.3 format, if needed in vfat like abreviation
static char toMSXChr(char a)
{
	a = ::toupper(a);
	if (a == ' ') {
		a = '_';
	}
	return a;
}
string FDC_DirAsDSK::makeSimpleMSXFileName(const string& fullfilename)
{
	string::size_type pos = fullfilename.find_last_of('/');
	string tmp;
	if (pos != string::npos) {
		tmp = fullfilename.substr(pos + 1);
	} else {
		tmp = fullfilename;
	}
	PRT_DEBUG("filename before transform " << tmp);
	transform(tmp.begin(), tmp.end(), tmp.begin(), toMSXChr);
	PRT_DEBUG("filename after transform " << tmp);

	string file, ext;
	pos = tmp.find_last_of('.');
	if (pos != string::npos) {
		file = tmp.substr(0, pos);
		ext  = tmp.substr(pos + 1);
	} else {
		file = tmp;
		ext = "";
	}

	PRT_DEBUG("adding correct amount of spaces");
	file += "        ";
	ext  += "   ";
	file = file.substr(0, 8);
	ext  = ext.substr(0, 3);

	return file + ext;
}


FDC_DirAsDSK::FDC_DirAsDSK(const string& fileName)
{
	// Here we create the fake diskimages based upon the files that can be
	// found in the 'fileName' directory
	PRT_DEBUG("Trying FDC_DirAsDSK image");
	DIR* dir = opendir(fileName.c_str());

	if (dir == NULL ) {
		PRT_DEBUG("Not a FDC_DirAsDSK image");
		throw MSXException("Not a directory");
	}

	// store filename as chroot dir for the msx disk
	MSXrootdir=fileName;

	// First create structure for the fake disk
	
	nbSectors = 1440; // asume a DS disk is used
	sectorsPerTrack = 9;
	nbSides = 2;

	std::string tmpfilename(MSXrootdir);
	tmpfilename+="/"+BootBlockFileName ;

	// Assign default boot disk to this instance
	memcpy(BootBlock, DefaultBootBlock, SECTOR_SIZE);

	// try to read boot block from file
	struct stat fst;
	bool readBootBlockFromFile = false;

	if (stat(tmpfilename.c_str(), &fst) == 0) {
		if (fst.st_size == (int)SECTOR_SIZE) {
			readBootBlockFromFile = true;
			FILE* file = fopen(tmpfilename.c_str(), "rb");
			if (file) {
				PRT_DEBUG("reading bootblock from " << tmpfilename);
				//fseek(file,0,SEEK_SET);
				// Read boot block from file
				fread(BootBlock, 1, SECTOR_SIZE, file);
				fclose(file);
			}
		}
	}
	
	// Assign empty directory entries
	for (int i = 0; i < 112; i++) {
		memset(&mapdir[i].msxinfo, 0, sizeof(MSXDirEntry));
	}

	// Make a full clear FAT
	saveCachedSectors=false;
	memset(FAT, 0, SECTOR_SIZE * SECTORS_PER_FAT);
	// for some reason the first 3bytes are used to indicate the end of a cluster, making the first available cluster nr 2
	// some sources say that this indicates the disk fromat and FAT[0]should 0xF7 for single sided disk, and 0xF9 for double sided disks
	// TODO: check this :-)
	FAT[0] = 0xF9;
	FAT[1] = 0xFF;
	FAT[2] = 0xFF;
	PRT_DEBUG("FAT located at : " << FAT);
	
	//clear the sectormap so that they all point to 'clean' sectors
	for (int i = 0; i < 1440 ; i++) {
		sectormap[i].dirEntryNr=NODIRENTRY;
	};

	//read directory and fill the fake disk
	struct dirent* d = readdir(dir);
	while (d) {
		string name(d->d_name);
		PRT_DEBUG("reading name in dir :" << name);
		//TODO: if bootsector read from file we should skip this file
		if ( !(readBootBlockFromFile && name == BootBlockFileName) && name != CachedSectorsFileName ) {
			updateFileInDSK(fileName + '/' + name); // used here to add file into fake dsk
		}
		d = readdir(dir);
	}
	closedir(dir);
	
	// And now read the cached sectors
	//TODO: we should check if the other files have changed since we
	//      wrote the cached sectors, this could invalided the cache !
	tmpfilename=std::string(MSXrootdir) + "/" + CachedSectorsFileName ;
	PRT_DEBUG("Trying to read cached sector file" << tmpfilename);
	if (stat(tmpfilename.c_str(), &fst) ==0 ) {
		if ( ( fst.st_size % ( SECTOR_SIZE + sizeof(int)) ) == 0 ){
			FILE* file = fopen(tmpfilename.c_str(), "rb");
			if (file) {
				PRT_DEBUG("reading cached sectors from " << tmpfilename);
				saveCachedSectors=true; //make sure that we don't destroy the cache when destroying this object
				int i;
				byte* tmpbuf;
				while ( fread(&i , 1, sizeof(int), file) ){ // feof(file) 

					PRT_DEBUG("reading cached sector " << i);
					if (i == 0){
						PRT_DEBUG("cached sector is 0, this should be impossible!");
					} else if ( i < (1 + 2 * SECTORS_PER_FAT) ){
						i=(i-1) % SECTORS_PER_FAT;
						PRT_DEBUG("cached sector is FAT sector "<< i);
						fread(FAT + i * SECTOR_SIZE , 1, SECTOR_SIZE, file);
					} else if ( i < 14 ){
						i -= (1 + 2 * SECTORS_PER_FAT);
						PRT_DEBUG("cached sector is DIR sector "<< i);
						int dirCount = i * 16;
						for (int j = 0; j < 16; j++) {
							fread( &(mapdir[dirCount++].msxinfo), 1 ,32 ,file );
						}
					} else {
						//regular cached sector
						PRT_DEBUG("reading regular cached sector " << i);
						tmpbuf=(byte*)malloc(SECTOR_SIZE); //TODO check failure!!
						fread(tmpbuf , 1, SECTOR_SIZE, file);
						cachedSectors[i]=tmpbuf;
						sectormap[i].dirEntryNr = CACHEDSECTOR ;

					}
				}

				//fread(BootBlock, 1, SECTOR_SIZE, file);
				fclose(file);
			} else {
				CliComm::instance().printWarning(
					"Couldn't open file " + tmpfilename);
			}
		} else {
			// fst.st_size % ( SECTOR_SIZE + sizeof(int))
			PRT_DEBUG("Wrong filesize for file" << tmpfilename << "=" << fst.st_size);
		}
	} else {
		PRT_DEBUG("Couldn't stat cached sector file" << tmpfilename);
	}
}

FDC_DirAsDSK::~FDC_DirAsDSK()
{
	PRT_DEBUG("Destroying FDC_DirAsDSK object");
	std::map<const int, byte*>::const_iterator it;
	//writing the cached sectors to a file
	std::string filename=std::string(MSXrootdir) + "/" + CachedSectorsFileName ;
	//PRT_DEBUG (" cachedSectors.begin() " << (int)cachedSectors.begin() );
	//PRT_DEBUG (" cachedSectors.end() " << (int)cachedSectors.end() );

	//if ( cachedSectors.begin() != cachedSectors.end() )
	if ( saveCachedSectors ) {
		FILE* file = fopen(filename.c_str(), "wb");
		if (file) {
			// if we have cached sectors we need also to save the fat and dir sectors!
			byte tmpbuf[SECTOR_SIZE];
			for (int i = 1; i <= 14; ++i) {
				readLogicalSector(i, tmpbuf);
				fwrite(&i , 1, sizeof(int), file);
				fwrite(tmpbuf, 1, SECTOR_SIZE, file);
			}

			for (it = cachedSectors.begin(); it != cachedSectors.end(); ++it) {
				PRT_DEBUG("Saving cached sector file" << it->first);
				fwrite(&(it->first) , 1, sizeof(int), file);
				fwrite(it->second , 1, SECTOR_SIZE, file);
			}
			fclose(file);
		} else {
			CliComm::instance().printWarning(
				"Couldn't create cached sector file " + filename);
		}
	} else {
		// remove cached sectors file if it exists
		PRT_DEBUG("Removing " << filename);
		unlink(filename.c_str());
	}


	//cleaning up memory from cached sectors
	for ( it = cachedSectors.begin() ; it != cachedSectors.end() ; it++ ) {
		free( it->second ); // ?????
	}
}

void FDC_DirAsDSK::readLogicalSector(unsigned sector, byte* buf)
{
	PRT_DEBUG("Reading sector : " << sector );
	if (sector == 0) {
		//copy our fake bootsector into the buffer
		PRT_DEBUG("Reading boot sector");
		memcpy(buf, BootBlock, SECTOR_SIZE);
	} else if (sector < (1 + 2 * SECTORS_PER_FAT)) {
		//copy correct sector from FAT
		PRT_DEBUG("Reading fat sector : " << sector );

		// quick-and-dirty:
		// we check all files in the faked disk for altered filesize
		// remapping each fat entry to its direntry and do some bookkeeping
		// to avoid multiple checks will probably be slower then this 
		for (int i = 0; i < 112; i++) {
			if ( ! mapdir[i].filename.empty()) {
				//PRT_DEBUG("Running checkAlterFileInDisk : " << i );
				checkAlterFileInDisk(i);
			}
		};

		sector = (sector - 1) % SECTORS_PER_FAT;
		memcpy(buf, FAT + sector * SECTOR_SIZE, SECTOR_SIZE);
	} else if (sector < 14) {
		//create correct DIR sector 
		PRT_DEBUG("Reading dir sector : " << sector );
		sector -= (1 + 2 * SECTORS_PER_FAT);
		int dirCount = sector * 16;
		for (int i = 0; i < 16; i++) {
			if ( ! mapdir[i].filename.empty()) {
				checkAlterFileInDisk(dirCount);
			}
			memcpy(buf, &(mapdir[dirCount++].msxinfo), 32);
			buf += 32;
		}
	} else {
		PRT_DEBUG("Reading mapped sector : " << sector );
		// else get map from sector to file and read correct block
		// folowing same numbering as FAT eg. first data block is cluster 2
		//int cluster = (int)((sector - 14) / 2) + 2; 
		//PRT_DEBUG("Reading cluster " << cluster );
		if (sectormap[sector].dirEntryNr == NODIRENTRY ) {
			//return an 'empty' sector
			// 0xE5 is the value used on the Philips VG8250
			memset(buf, 0xE5, SECTOR_SIZE  );
		} else if (sectormap[sector].dirEntryNr == CACHEDSECTOR ) {
			PRT_DEBUG ("reading  cachedSectors["<<sector<<"]" );
			PRT_DEBUG ("cachedSectors["<<sector<<"] :" << cachedSectors[sector] );
			memcpy(buf, cachedSectors[sector] , SECTOR_SIZE);
		} else {
			// open file and read data
			int offset = sectormap[sector].fileOffset;
			string tmp = mapdir[sectormap[sector].dirEntryNr].filename;
			PRT_DEBUG("  Reading from file " << tmp );
			PRT_DEBUG("  Reading with offset " << offset );
			checkAlterFileInDisk(tmp);
			FILE* file = fopen(tmp.c_str(), "rb");
			if (file) {
				fseek(file,offset,SEEK_SET);
				fread(buf, 1, SECTOR_SIZE, file);
				fclose(file);
			} 
			//if (!file || ferror(file)) {
			//	throw DiskIOErrorException("Disk I/O error");
			//}
		}
	}
}

void FDC_DirAsDSK::checkAlterFileInDisk(const string& fullfilename)
{
	for (int i = 0; i < 112; i++) {
		if (mapdir[i].filename == fullfilename) {
			checkAlterFileInDisk(i);
		}
	}
}

void FDC_DirAsDSK::checkAlterFileInDisk(const int dirindex)
{
	if (  mapdir[dirindex].filename.empty()) {
		return;
	}
	
	int fsize;
	// compute time/date stamps
	struct stat fst;
	memset(&fst, 0, sizeof(struct stat));
	
	PRT_DEBUG("trying to stat : " << mapdir[dirindex].filename );
	stat(mapdir[dirindex].filename.c_str(), &fst);
	fsize = fst.st_size;

	if ( mapdir[dirindex].filesize != fsize ) {
		PRT_DEBUG("Changed filesize "<<mapdir[dirindex].filesize<<" != "<<fsize<<" for file "<<mapdir[dirindex].filename);
		updateFileInDisk(dirindex);
	};
}
void FDC_DirAsDSK::updateFileInDisk(const int dirindex)
{
	PRT_DEBUG("updateFileInDisk : " << mapdir[dirindex].filename << " with dirindex  " << dirindex);
	// compute time/date stamps
	int fsize;
	struct stat fst;
	memset(&fst, 0, sizeof(struct stat));
	stat(mapdir[dirindex].filename.c_str(), &fst);
	struct tm* mtim = localtime(&(fst.st_mtime));
	int t1 = mtim 
	       ? (mtim->tm_sec >> 1) + (mtim->tm_min << 5) +
	         (mtim->tm_hour << 11)
	       : 0;
	setsh(mapdir[dirindex].msxinfo.time, t1);
	int t2 = mtim
	       ? mtim->tm_mday + ((mtim->tm_mon + 1) << 5) +
	         ((mtim->tm_year + 1900 - 1980) << 9)
	       : 0;
	setsh(mapdir[dirindex].msxinfo.date, t2);
	fsize = fst.st_size;

	mapdir[dirindex].filesize=fsize;
	int curcl = 2;
	curcl=rdsh(mapdir[dirindex].msxinfo.startcluster );
	// if there is no cluster assigned yet to this file, then find a free cluster
	bool followFATClusters=true;
	if (curcl == 0) {
		followFATClusters=false;
		curcl=findFirstFreeCluster();
	setsh(mapdir[dirindex].msxinfo.startcluster, curcl);
	}
	PRT_DEBUG("Starting at cluster " << curcl );

	unsigned size = fsize;
	int prevcl = 0; 
	while (size && (curcl <= MAX_CLUSTER)) {
		int logicalSector= 14 + 2*( curcl - 2 );

		sectormap[logicalSector].dirEntryNr = dirindex;
		sectormap[logicalSector].fileOffset = fsize - size;
		size -= (size > SECTOR_SIZE  ? SECTOR_SIZE  : size);

		if (size){
			//fill next sector if there is data left
			sectormap[++logicalSector].dirEntryNr = dirindex;
			sectormap[logicalSector].fileOffset = fsize - size;
			size -= (size > SECTOR_SIZE  ? SECTOR_SIZE : size);
		}

		if (prevcl) {
			WriteFAT(prevcl, curcl);
		}
		prevcl = curcl;
		//now we check if we continue in the current clusterstring or need to allocate extra unused blocks
		if (followFATClusters){
			curcl=ReadFAT(curcl);
			if ( curcl == EOF_FAT ) {
				followFATClusters=false;
				curcl=findFirstFreeCluster(); 
			}
		} else {
			do {
				curcl++;
			} while((curcl <= MAX_CLUSTER) && ReadFAT(curcl));
		}
		PRT_DEBUG("Continuing at cluster " << curcl);
	}
	if ((size == 0) && (curcl <= MAX_CLUSTER)) {
		// TODO: check what an MSX does with filesize zero and fat allocation;
		if(prevcl==0) {
			WriteFAT(curcl, EOF_FAT);
		} else {
			WriteFAT(prevcl, EOF_FAT); 
		}

		//clear remains of FAT if needed
		if (followFATClusters) {
			int logicalSector;
			while((curcl <= MAX_CLUSTER) && (curcl != EOF_FAT )) {
				prevcl=curcl;
				curcl=ReadFAT(curcl);
				WriteFAT(prevcl, 0);
				logicalSector= 14 + 2*( prevcl - 2 );
				sectormap[logicalSector].dirEntryNr = NODIRENTRY;
				sectormap[logicalSector++].fileOffset = 0;
				sectormap[logicalSector].dirEntryNr = NODIRENTRY;
				sectormap[logicalSector].fileOffset = 0;
			}
			WriteFAT(prevcl, 0);
			logicalSector= 14 + 2*( prevcl - 2 );
			sectormap[logicalSector].dirEntryNr = NODIRENTRY;
			sectormap[logicalSector].fileOffset = 0;
		}
	} else {
		//TODO: don't we need a EOF_FAT in this case as well ?
		// find out and adjust code here
		CliComm::instance().printWarning("Fake Diskimage full: " +
			mapdir[dirindex].filename + " truncated.");
	}
	//write (possibly truncated) file size
	setlg(mapdir[dirindex].msxinfo.size, fsize - size);

}

void FDC_DirAsDSK::writeLogicalSector(unsigned sector, const byte* buf)
{
	PRT_DEBUG("Writing sector : " << sector);
	if (sector == 0) {
		//copy buffer into our fake bootsector and safe into file
		PRT_DEBUG("Reading boot sector");
		memcpy(BootBlock, buf, SECTOR_SIZE);
		std::string filename(MSXrootdir);
		filename+="/"+BootBlockFileName ;
		FILE* file = fopen(filename.c_str(), "wb");
		if (file) {
			PRT_DEBUG("Writing bootblock to " << filename);
			fwrite(buf, 1, SECTOR_SIZE, file);
			fclose(file);
		} else {
			CliComm::instance().printWarning(
				"Couldn't create bootsector file " + filename);
		}
	} else if (sector < (1 + 2 * SECTORS_PER_FAT)) {
		//copy to correct sector from FAT
		//make sure we write changed sectors to the cache file later on
		saveCachedSectors=true;

		//during formatting sectors > 1+SECTORS_PER_FAT are empty (all
		//bytes are 0) so we would erase the 3 bytes indentifier at the
		//beginning of the FAT !! 
		//Since the two FATs should be identical after "normal" usage,
		//we simply ignore writes (for now, later we could cache them
		//perhaps)
		if (sector < (1 + SECTORS_PER_FAT)) {
			sector = (sector - 1) % SECTORS_PER_FAT;
			memcpy(FAT + sector * SECTOR_SIZE, buf, SECTOR_SIZE);
		}
		PRT_DEBUG("FAT[0] :"<< std::hex << (int)FAT[0] );
		PRT_DEBUG("FAT[1] :"<< (int)FAT[1] );
		PRT_DEBUG("FAT[2] :"<< (int)FAT[2] );
		PRT_DEBUG("buf[0] :"<< std::hex << (int)buf[0] );
		PRT_DEBUG("buf[1] :"<< (int)buf[1] );
		PRT_DEBUG("buf[2] :"<< (int)buf[2] << std::dec );
	} else if (sector < 14) {
		//make sure we write changed sectors to the cache file later on
		saveCachedSectors=true;
		//create correct DIR sector 
		/*
		 We assume that the dir entry is updatet as latest: So 
		 the fat and actual sectordata should already contain the correct data
		 Most MSX disk roms honour this behaviour for normal fileactions
		 Ofcourse some diskcaching programs en disk optimizers can abandon this behaviour 
		 and in such case the logic used here goes haywire!!
		*/
		sector -= (1 + 2 * SECTORS_PER_FAT);
		int dirCount = sector * 16;
		for (int i = 0; i < 16; i++) {
			//TODO check if changed and take apropriate actions if needed
			if (memcmp( mapdir[dirCount].msxinfo.filename , buf, 32 ) != 0) {
				PRT_DEBUG("dir entry for "<<mapdir[dirCount].filename <<" has changed");
				//mapdir[dirCount].msxinfo.filename[0] == 0xE5 if already deleted....

				//the 3 vital informations needed
				bool chgName=(memcmp( mapdir[dirCount].msxinfo.filename , buf, 11 ) ==0 ) ? true : false ;
				
				int tmpint=(int)(buf[25] + buf[26]<<8);
				PRT_DEBUG(" buf[25] + buf[26]<<8 "<< tmpint << "?");
				bool chgClus=( rdsh(mapdir[dirCount].msxinfo.startcluster) ==
						(buf[25] + buf[26]<<8 )  ) ? true : false ;
				tmpint=(int)(buf[27] + buf[27]<<8 + buf[27]<<16 + buf[27]<<24 );
				PRT_DEBUG(" (buf[27] + buf[27]<<8 + buf[27]<<16 + buf[27]<<24 ) "<< tmpint << "?");
				bool chgSize=( rdlg(mapdir[dirCount].msxinfo.size) ==
						(buf[27] + buf[27]<<8 + buf[27]<<16 + buf[27]<<24 ) ) ? true : false ;

				PRT_DEBUG("	chgSize "<< (chgSize?"true":"false"));
				PRT_DEBUG("	chgClus "<< (chgClus?"true":"false"));
				PRT_DEBUG("	chgName "<< (chgName?"true":"false"));

				if ( chgClus && chgName){
					PRT_DEBUG("Major change: new file started !!!!");
				} else {
					CliComm::instance().printWarning(
						"! A unussual change has been performed on this disk\n"
						"! are you running a disk editor or optimizer, or maybe\n"
						"! a diskcache program\n"
						"! Do not use 'dir as disk' emulation while running these kind of programs!\n");
				}
				if ( chgName && !chgClus && !chgSize ) {
					if ( buf[0] == 0xE5 )  {
					PRT_DEBUG("dir entry for "<<mapdir[dirCount].filename <<" has been deleted");
					//TODO: What now, really remove entry
					//and clean sectors or keep around in
					//case of an undelete ?? Later seems
					//more real though, but is it safe
					//enough for host OS files when writing
					//sectors?
					} else {
						CliComm::instance().printWarning(
							"File has been renamed in emulated disk, Host OS file (" +
							mapdir[dirCount].filename + ") remains untouched!");
					}
				} 

				if ( !chgName && !chgClus && chgSize ) {
					PRT_DEBUG("Content of "<<mapdir[dirCount].filename <<" changed");
					// extract the file
				}
				//for now simply blindly take over info
				memcpy( &(mapdir[dirCount].msxinfo), buf, 32);
			} else {
				PRT_DEBUG("dir entry for "<<mapdir[dirCount].filename <<" not changed");
			}
			dirCount++;
			buf += 32;
		}
		CliComm::instance().printWarning(
			"writing to DIR not yet fully implemented !!!!");
	} else {
	  // simply buffering everything for now, no write-through to host-OS
		//check if cachedSectors exists, if not assign memory.

		//make sure we write cached sectors to a file later on
		saveCachedSectors=true;

		if ( cachedSectors[sector] == NULL ) { // is this test actually valid C++ ??
			PRT_DEBUG("cachedSectors["<<sector<<"] == NULL");
			byte *tmp;
			tmp=(byte*)malloc(SECTOR_SIZE);
			if (tmp == NULL){
			  throw WriteProtectedException( "Malloc failure for FDC_DirAsDSK");
			}
			cachedSectors[sector]=tmp;
		}
		memcpy( cachedSectors[sector] ,buf, SECTOR_SIZE);
		sectormap[sector].dirEntryNr = CACHEDSECTOR ;
	  //
	  //   for now simply ignore writes
	  //
	  // throw WriteProtectedException(
	  //	  "Writing not yet supported for FDC_DirAsDSK");
	}

}

bool FDC_DirAsDSK::writeProtected()
{
	return false ; // for the moment we don't allow writing to this directory
}

void FDC_DirAsDSK::updateFileInDSK(const string& fullfilename)
{
	struct stat fst;

	if (stat(fullfilename.c_str(), &fst)) {
		CliComm::instance().printWarning(
			"Error accessing " + fullfilename);
		return;
	}
	if (!S_ISREG(fst.st_mode)) {
		// we only handle regular files for now
		CliComm::instance().printWarning(
			"Not a regular file: " + fullfilename);
		return;
	}
	    
	if (!checkFileUsedInDSK(fullfilename)) {
		// add file to fakedisk
		PRT_DEBUG("Going to addFileToDSK");
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
	while ((dirindex < 112) && !mapdir[dirindex].filename.empty()) {
	     dirindex++;
	}
	PRT_DEBUG("Adding on dirindex " << dirindex);
	if (dirindex == 112) {
		CliComm::instance().printWarning(
			"Couldn't add " + fullfilename + ": root dir full");
		return;
	}

	// fill in native file name
	mapdir[dirindex].filename = fullfilename;

	// create correct MSX filename
	string MSXfilename = makeSimpleMSXFileName(fullfilename);
	PRT_DEBUG("Using MSX filename " << MSXfilename );
	if (checkMSXFileExists(MSXfilename)) {
		//TODO: actually should increase vfat abrev if possible!!
		CliComm::instance().printWarning(
			"Couldn't add " + fullfilename + ": MSX name "
		         + MSXfilename + " existed already");
		return;
	}
	
	// fill in MSX file name
	memcpy(&(mapdir[dirindex].msxinfo.filename), MSXfilename.c_str(), 11);
	// Here actually call to updateFileInDisk!!!!
	updateFileInDisk(dirindex);
	return;
}


} // namespace openmsx
