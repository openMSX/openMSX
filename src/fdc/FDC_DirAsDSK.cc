// $Id$

#include "File.hh"
#include "FDC_DirAsDSK.hh"
#include "FileContext.hh"

#include <cassert>
#include <dirent.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>

/* macros to change DirEntries */
#define setsh(x,y) {x[0]=y;x[1]=y>>8;}
#define setlg(x,y) {x[0]=y;x[1]=y>>8;x[2]=y>>16;x[3]=y>>24;}

/* macros to read DirEntries */
#define rdsh(x) (x[0]+(x[1]<<8))
#define rdlg(x) (x[0]+(x[1]<<8)+(x[2]<<16)+(x[3]<<24))

#define EOF_FAT 0xFFF /* signals EOF in FAT */

//Bootblock taken from a philips  nms8250 formatted disk
const byte FDC_DirAsDSK::BootBlock[] =
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
	byte* P=FAT + (clnr * 3) / 2;
	if (clnr & 1) { 
		P[0] = (P[0] & 0x0F) + (val << 4);
		P[1] = val >> 4;
	} else {
		P[0] = val;
		P[1] = (P[1] & 0xF0) + ((val >> 8) & 0x0F);
	}
}

// check if a filename is used in the emulated MSX disk
bool FDC_DirAsDSK::checkMSXFileExists(const string& msxfilename)
{
	//TODO: complete this
	unsigned pos = msxfilename.find_last_of('/');
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
char toMSXChr(char a)
{
	a = ::toupper(a);
	if (a == ' ') {
		a = '_';
	}
	return a;
}
string FDC_DirAsDSK::makeSimpleMSXFileName(const string& fullfilename)
{
	unsigned pos = fullfilename.find_last_of('/');
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


FDC_DirAsDSK::FDC_DirAsDSK(FileContext &context, const string &fileName)
{
	// TODO reolve fileName in given context
	
	// Here we create the fake diskimages based upon the files that can be
	// found in the 'fileName' directory
	PRT_INFO("Creating FDC_DirAsDSK object");
	DIR* dir = opendir(fileName.c_str());

	if (dir == NULL ) {
		throw MSXException("Not a directory");
	}

	// First create structure for the fake disk
	
	nbSectors = 1440; // asume a DS disk is used
	sectorsPerTrack = 9;
	nbSides = 2;
	// Assign empty directory entries
	for (int i = 0; i < 112; i++) {
		memset(&mapdir[i].msxinfo, 0, sizeof(MSXDirEntry));
	}

	// Make a full clear FAT
	memset(FAT, 0, SECTOR_SIZE * SECTORS_PER_FAT);
	FAT[0] = 0xF9;
	FAT[1] = 0xFF;
	FAT[2] = 0xFF;

	//read directory and fill the fake disk
	struct dirent* d = readdir(dir);
	while (d) {
		string name(d->d_name);
		PRT_DEBUG("reading name in dir :" << name);
		updateFileInDSK(fileName + '/' + name); // used here to add file into fake dsk
		d = readdir(dir);
	}
	closedir(dir);
}

FDC_DirAsDSK::~FDC_DirAsDSK()
{
	PRT_DEBUG("Destroying FDC_DirAsDSK object");
}

void FDC_DirAsDSK::read(byte track, byte sector, byte side,
                   int size, byte* buf)
{
	assert(size == SECTOR_SIZE);
	
	int logicalSector = physToLog(track, side, sector);
	if (logicalSector >= nbSectors) {
		throw NoSuchSectorException("No such sector");
	}
	if (logicalSector == 0) {
		//copy our fake bootsector into the buffer
		memcpy(buf, BootBlock, SECTOR_SIZE);
	} else if (logicalSector < (1 + 2 * SECTORS_PER_FAT)) {
		//copy correct sector from FAT
		logicalSector = (logicalSector - 1) % SECTORS_PER_FAT;
		memcpy(buf, FAT + logicalSector * SECTOR_SIZE, size);
	} else if (logicalSector < 14) {
		//create correct DIR sector 
		logicalSector -= (1 + 2 * SECTORS_PER_FAT);
		int dirCount = logicalSector * 16;
		for (int i = 0; i < 16; i++) {
			memcpy(buf, &mapdir[dirCount++].msxinfo, 32);
			buf += 32;
		}
	} else {
		// else get map from sector to file and read correct block
		// folowing same numbering as FAT eg. first data block is cluster 2
		int cluster = (int)((logicalSector - 14) / 2) + 2; 
		// open file and read data
		int offset = clustermap[cluster].fileOffset + (cluster & 1) * SECTOR_SIZE;
		string tmp = mapdir[clustermap[cluster].dirEntryNr].filename;
		FILE* file = fopen(tmp.c_str(), "r");
		if (file) {
			fseek(file,offset,SEEK_SET);
			fread(buf, 1, SECTOR_SIZE, file);
			fclose(file);
		} else {
			// actually maybe there isn't a file for this cluster
			// assigned, so we // should return a freshly formated
			// sector ?
		}
		if (!file || ferror(file)) {
			throw DiskIOErrorException("Disk I/O error");
		}
	}
}

void FDC_DirAsDSK::write(byte track, byte sector, byte side, 
                    int size, const byte* buf)
{
	//
	//   for now simply ignore writes
	//
	throw WriteProtectedException(
		"Writing not yet supported for FDC_DirAsDSK");
	
	/*
	int logicalSector = physToLog(track, side, sector);
	if (logicalSector >= nbSectors) {
		throw NoSuchSectorException("No such sector");
	}
	*/
}

void FDC_DirAsDSK::readBootSector()
{
	// We can fake regular DS or SS disks
	if (nbSectors == 1440) {
		sectorsPerTrack = 9;
		nbSides = 2;
	} else if (nbSectors == 720) {
		sectorsPerTrack = 9;
		nbSides = 1;
	}
}


bool FDC_DirAsDSK::writeProtected()
{
	return true ; // for the moment we don't allow writing to this directory
}

bool FDC_DirAsDSK::doubleSided()
{
	return nbSides == 2;
}

void FDC_DirAsDSK::updateFileInDSK(const string& fullfilename)
{
	struct stat fst;

	if (stat(fullfilename.c_str(), &fst)) {
		PRT_INFO("Error accessing " << fullfilename);
		return;
	}
	if (!S_ISREG(fst.st_mode)) {
		// we only handle regular files for now
		PRT_INFO("Not a regular file: " << fullfilename);
		return;
	}
	    
	if (!checkFileUsedInDSK(fullfilename)) {
		// add file to fakedisk
		PRT_DEBUG("Going to addFileToDSK");
		addFileToDSK(fullfilename);
	//} else {
		//really update file (dir entry + fat) 
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
		PRT_INFO( "Couldn't add " << fullfilename << ": root dir full");
		return;
	}

	// fill in native file name
	mapdir[dirindex].filename = fullfilename;

	// create correct MSX filename
	string MSXfilename = makeSimpleMSXFileName(fullfilename);
	PRT_DEBUG("Using MSX filename " << MSXfilename );
	if (checkMSXFileExists(MSXfilename)) {
		//TODO: actually should increase vfat abrev if possible!!
		PRT_INFO("Couldn't add " << fullfilename << ": MSX name "
		         << MSXfilename<< "existed already");
		return;
	}
	
	// fill in MSX file name
	memcpy(&(mapdir[dirindex].msxinfo.filename), MSXfilename.c_str(), 11);
	
	//open file
	int fsize;
	//FILE* file = fopen(fullfilename.c_str(), "r");
	//if (!file) {
		//if open file fails (read permissions perhaps) then still enter
		//in MSX disk with zero info
		
		//TODO: find out if a file with size is zero also uses a cluster ! 
		//It probably does :-(
		//if so the code should be entered here.
		
		fsize = 0;	// TODO BUG nothing is done with fsize
		//return;
	//}
	//fclose(file);
	
	// compute time/date stamps
	struct stat fst;
	bzero(&fst,sizeof(struct stat));
	stat(fullfilename.c_str(), &fst);
	struct tm mtim = *localtime(&(fst.st_mtime));
	int t = (mtim.tm_sec >> 1) + (mtim.tm_min << 5) +
		(mtim.tm_hour << 11);
	setsh(mapdir[dirindex].msxinfo.time, t);
	t = mtim.tm_mday + ((mtim.tm_mon + 1) << 5) +
	    ((mtim.tm_year + 1900 - 1980) << 9);
	setsh(mapdir[dirindex].msxinfo.date, t);
	fsize = fst.st_size;

	//find first 'really' free cluster (= not from a deleted string of clusters)
	int curcl = 2;
	while ((curcl <= MAX_CLUSTER) && ReadFAT(curcl)) {
		curcl++;
	}
	PRT_DEBUG("Starting at cluster " << curcl );
	setsh(mapdir[dirindex].msxinfo.startcluster, curcl);

	int size = fsize;
	int prevcl = 0; 
	while (size && (curcl <= MAX_CLUSTER)) {
		clustermap[curcl].dirEntryNr = dirindex;
		clustermap[curcl].fileOffset = fsize - size;

		size -= (size > (SECTOR_SIZE * 2) ? (SECTOR_SIZE * 2) : size);
		if (prevcl) {
			WriteFAT(prevcl, curcl);
		}
		prevcl = curcl;
		do {
			curcl++;
		} while((curcl <= MAX_CLUSTER) && ReadFAT(curcl));
		PRT_DEBUG("Continuing at cluster " << curcl);
	}
	if ((size == 0) && (curcl <= MAX_CLUSTER)) {
		WriteFAT(prevcl, EOF_FAT);
	} else {
		PRT_INFO("Fake Diskimage full: " << MSXfilename << " truncated.");
	}
	//write (possibly truncated) file size
	setlg(mapdir[dirindex].msxinfo.size, fsize - size);

	/*ADAPT code from until here !!!!!*/
}

/*
	fullpath = fullfilename.c_str() ;
	for (i=0 ; i<11 ; name[i++]=(char)' ');
	name[11]=(char)0;

	//get rid of '/'
	if ( !(p=strrchr(fullpath,'/')) ) {
	  p=fullpath; //actually this shouldn't be possible since we expect a full path
	} else {
	  p++;
	};
	for( ; *p=='.'; p++); //remove 'hidden' atribute from *nix filename
	// seek extension
	for ( e=p ; *e ; e++ );				//go to end of string
	while ( e-- ; e != p && *e != '.'; e-- );	// track back for ext
	if ( e != p ) {
		// we have a real ext
		for(e++, i=8; (i<11) && *e ; e++,i++) {
		  name[i]=toupper(*e);
		}
	}

	//fill name with first 8 characters
	e=p;
	for(i=0; (i<8) && *p && *p !='.' ; p++,i++) {
	  if (*p != ' ') {
	    name[i]=toupper(p);
	  } else {
	    name[i]='_';
	  }
	}
	// now we should check if the filename is to long, if so then we need to use the counter like abrev thsi will return always nr 1 checks for doubles is not performed in this routine
	if ( *p != '.' && *p && i==8 ) {
		//filename to long
		name[6]='-';name[7]='1';
	}
*/
