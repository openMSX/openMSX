// $Id$

#include "File.hh"
#include "FDC_DirAsDSK.hh"
#include "FileContext.hh"

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

/* read FAT-entry from FAT in memory */
word FDC_DirAsDSK::ReadFAT(word clnr)
{ register byte *P;

  P=FAT+(clnr*3)/2;
  return (clnr&1)? (P[0]>>4)+(P[1]<<4) : P[0]+((P[1]&0x0F)<<8);
}

/* write an entry to FAT in memory */
void FDC_DirAsDSK::WriteFAT(word clnr, word val)
{ register byte *P;

  P=FAT+(clnr*3)/2;
  if (clnr&1)
    { 
      P[0]=(P[0]&0x0F)+(val<<4);
      P[1]=val>>4;
    }
  else
    {
      P[0]=val;
      P[1]=(P[1]&0xF0)+((val>>8)&0x0F);
    }
}

/* check if a filename is used in the emulated MSX disk*/
bool FDC_DirAsDSK::checkMSXFileExists(std::string fullfilename)
{
	//TODO: complete this
	unsigned pos = fullfilename.find_last_of('/');
	std::string tmp;
	if (pos != std::string::npos) {
	  tmp = fullfilename.substr(pos+1);
	} else {
	  tmp=fullfilename;
	}
	tmp=makeSimpleMSXFileName(tmp);

	for (int i = 0; i < 112; i++) {
		if (strncmp((const char*)(mapdir[i].msxinfo.filename), tmp.c_str(), 11) == 0 ) {
		  return true;
		}
	}
	return false;
}

/* check if a file is already mapped into the fake DSK */
bool FDC_DirAsDSK::checkFileUsedInDSK(std::string fullfilename)
{
	for (int i = 0; i < 112; i++) {
		if (mapdir[i].filename == fullfilename) {
			return true;
		}
	}
	return false;
}

char toMSXChr(char a)
{
	a = ::toupper(a);
	if (a == ' ') {
		a = '_';
	}
	return a;
}

/* create an MSX filename 8.3 format, if needed in vfat like abreviation */
std::string FDC_DirAsDSK::makeSimpleMSXFileName(std::string fullfilename)
{
	unsigned pos = fullfilename.find_last_of('/');
	std::string tmp;
	if (pos != std::string::npos) {
	  tmp = fullfilename.substr(pos+1);
	} else {
	  tmp=fullfilename;
	}
	
	std::transform(tmp.begin(), tmp.end(), tmp.begin(), toMSXChr);
	
	std::string file, ext;
	pos = fullfilename.find_last_of('.');
	if (pos != std::string::npos) {
	  file = tmp.substr(0, pos);
	  ext  = tmp.substr(pos + 1);
	} else {
	  file = tmp;
	  //ext = "";
	}
	
	file += "        ";
	ext  += "   ";
	file = file.substr(0, 8);
	ext  = ext.substr(0, 3);

	return file + ext;
}



FDC_DirAsDSK::FDC_DirAsDSK(FileContext *context, const string &fileName)
{
	//Here we create the fake diskimages based upon the files that can be
	//found in the 'fileName' directory
	PRT_INFO("Creating FDC_DirAsDSK object");
	DIR* dir = opendir(fileName.c_str());
	
	if (dir == NULL ) {
		throw MSXException("Not a directory");
	} else {

	  //First create structure for the fake disk
	  //
	  nbSectors = 1440; // asume a DS disk is used
	  sectorsPerTrack = 9;
	  nbSides = 2;
	  // Assign empty directory entries
	  for (int i=0 ; i<112 ; i++ ) {
	  	memset(&mapdir[i].msxinfo,0,sizeof(MSXDirEntry));
	  };
	  
	  // Make a full clear FAT
	  FAT = (byte*)malloc(SECTOR_SIZE*5);
	  if (FAT == NULL){
		throw MSXException("Couldn't allocate memory for FAT");
	  }
	  memset(FAT,0,SECTOR_SIZE*5);
	  
	  //read directory and fill the fake disk
	  struct dirent* d = readdir(dir);
	  while (d) {
	    std::string name(d->d_name);
	    PRT_DEBUG("reading name in dir :" << name);
	    updateFileInDSK(fileName+name); // used here to add file into fake dsk
	    d = readdir(dir);
	  }
	  closedir(dir);
	}
}

FDC_DirAsDSK::~FDC_DirAsDSK()
{
	PRT_DEBUG("Destroying FDC_DirAsDSK object");
	free(FAT);
}

void FDC_DirAsDSK::read(byte track, byte sector, byte side,
                   int size, byte* buf)
{
	try {
		int logicalSector = physToLog(track, side, sector);
		if (logicalSector >= nbSectors) {
			throw NoSuchSectorException("No such sector");
		}
		if (logicalSector ==0){
			//copy our fake bootsector into the buffer
			memcpy(buf,BootBlock,size);
		} else if (logicalSector<11){
			//copy correct sector from FAT
			logicalSector-= (logicalSector>5 ? 1 : 6 );
			memcpy( buf , FAT+logicalSector*SECTOR_SIZE , size );
		} else if (logicalSector<18){
			//create correct DIR sector 
			logicalSector-= 11;
			int dirCount = logicalSector*16;
			for (int i = 0 ; i<16 ; i++) {
				memcpy( buf , &mapdir[dirCount++].msxinfo , 32 );
				buf+=32;
			}
		} else {
			//else get map from sector to file and read correct block
			// folowing same numbering as FAT eg. first data block is cluster 2
			int cluster=(int)((logicalSector-18)/2) + 2; 
			// open file and read data
			int offset=clustermap[cluster].fileOffset + (cluster & 1)*SECTOR_SIZE;
			std::string tmp=mapdir[clustermap[cluster].dirEntryNr].filename;
			FILE* file = fopen(tmp.c_str(), "r");
			if (!file) {
			  // actually maybe there isn't a file for this cluster assigned, so we
			  // should return a freshly formated sector ?
			  throw new FileException("Couldn't open file");
			}
			fseek(file,offset,SEEK_SET);
			fread(buf, 1, SECTOR_SIZE, file);
			fclose(file);
		}

	} catch (FileException &e) {
		throw DiskIOErrorException("Disk I/O error");
	}
}

void FDC_DirAsDSK::write(byte track, byte sector, byte side, 
                    int size, const byte* buf)
{
	try {
		int logicalSector = physToLog(track, side, sector);
		if (logicalSector >= nbSectors) {
			throw NoSuchSectorException("No such sector");
		}
		//
		//   for now simply ignore writes
		//
	} catch (FileException &e) {
		throw DiskIOErrorException("Disk I/O error");
	}
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

void FDC_DirAsDSK::updateFileInDSK(std::string fullfilename)
{
	struct stat fst;

	stat(fullfilename.c_str(), &fst);
	if (!S_ISREG(fst.st_mode)) {
		// we only handle regular files for now
		PRT_INFO("Not a regular file: " << fullfilename);
		return;
	}
	    
	if (!checkFileUsedInDSK(fullfilename)) {
		// add file to fakedisk
		addFileToDSK(fullfilename);
	} else {
		//really update file (dir entry + fat) 
	}
}

void FDC_DirAsDSK::addFileToDSK(std::string fullfilename)
{
	int dirindex;
	FILE* file;
	int curcl;
	int prevcl;
	int size;
	int fsize;

	//get emtpy dir entry
	for (dirindex=0 ; mapdir[dirindex].filename.length() && (dirindex < 112) ; dirindex++);
	if (dirindex == 112) {
		PRT_INFO( "Couldn't add " << fullfilename << ": root dir full");
		return;
	};
	// create correct MSX filename
	std::string MSXfilename=makeSimpleMSXFileName(fullfilename);
	if (checkMSXFileExists(MSXfilename)) {
		//TODO: actually should increase vfat abrev if possible!!
		PRT_INFO( "Couldn't add " << fullfilename << ": MSX name "<<MSXfilename<< "existed already");
		return;
	};
	//set correct info in mapdir
	memcpy(&mapdir[dirindex].filename , MSXfilename.c_str() , 11 );
	//open file
	file=fopen(fullfilename.c_str(),"r");
	if ( ! file ) {
	  //if open file fails (read permissions perhaps) then still enter
	  //in MSX disk with zero info
	  fsize=0;
	  //TODO: find out if a file with size is zero also uses a cluster ! 
	  //It probably does :-(
	  //if so the code should be entered here.

	} else {
	  fclose(file);
	  /* compute time/date stamps */
	  {
	    struct stat fst;
	    struct tm mtim;
	    int t;

	    stat(fullfilename.c_str(), &fst);
	    mtim = *localtime(&(fst.st_mtime));
	    t=(mtim.tm_sec>>1)+(mtim.tm_min<<5)+(mtim.tm_hour<<11);
	    setsh(mapdir[dirindex].msxinfo.time,t);
	    t=mtim.tm_mday+((mtim.tm_mon+1)<<5)+((mtim.tm_year+1900-1980)<<9);
	    setsh(mapdir[dirindex].msxinfo.date,t);
	    fsize=fst.st_size;
	  }

	  //find first 'really' free cluster (= not from a deleted string of clusters)
	  for(curcl=2; (curcl <= MAX_CLUSTER) && ReadFAT(curcl); curcl++);
	  setsh( mapdir[dirindex].msxinfo.startcluster , curcl );

	  size=fsize; prevcl=0; 
	  while( size && (curcl<= MAX_CLUSTER) ) {
	    clustermap[curcl].dirEntryNr=dirindex;
	    clustermap[curcl].fileOffset=fsize-size;

	    size-=( size > (SECTOR_SIZE*2) ? (SECTOR_SIZE*2) : size );
	    if (prevcl) WriteFAT(prevcl,curcl);
	    for(prevcl=curcl++; (curcl <= MAX_CLUSTER) && ReadFAT(curcl); curcl++);

	  };
	  if ( (size==0) && (curcl<= MAX_CLUSTER)  ) {
	    WriteFAT(prevcl,EOF_FAT);
	  } else {
	    PRT_INFO("Fake Diskimage full: "<< MSXfilename <<" truncated.");
	  }
	  //write (possibly truncated) file size
	  setlg(mapdir[dirindex].msxinfo.size,fsize-size);
	  /*ADAPT code from until here !!!!!*/
	}
}

//stuff below should have been inherited from FDC_DSK !!!!!
//
void FDC_DirAsDSK::initWriteTrack(byte track, byte side)
{
	writeTrackBufCur = 0;
	writeTrack_track = track;
	writeTrack_side = side;
	writeTrack_sector = 1;
	writeTrack_CRCcount = 0;
}

void FDC_DirAsDSK::writeTrackData(byte data)
{
	// if it is a 0xF7 ("two CRC characters") then the previous 512
	// bytes could be actual sectordata bytes
	if (data == 0xF7) {
		if (writeTrack_CRCcount & 1) {
			// first CRC is sector header CRC, second CRC is actual
			// sector data CRC so write them 
			byte tempWriteBuf[512];
			for (int i = 0; i < 512; i++) {
				tempWriteBuf[i] =
				      writeTrackBuf[(writeTrackBufCur+i) & 511];
			}
			write(writeTrack_track, writeTrack_sector,
			      writeTrack_side, 512, tempWriteBuf);
			writeTrack_sector++; // update sector counter
		}
		writeTrack_CRCcount++; 
	} else {
		writeTrackBuf[writeTrackBufCur++] = data;
		writeTrackBufCur &= 511;
	}
}

void FDC_DirAsDSK::initReadTrack(byte track, byte side)
{
	// init following data structure
	// according to Alex Wulms
	// 122 bytes track header aka pre-gap
	// 9 * 628 bytes sectordata (sector header, data en closure gap)
	// 1080 bytes end-of-track gap
	//
	// This data comes from the TC8566AF manual
	// each track in IBM format contains
	// '4E' x 80, '00' x 12, 'C2' x 3
	// 'FC' x 1, '4E' x 50, sector data 1 to n
	// '4E' x ?? (closing gap)
	// each sector data contains
	// '00' x 12, 'A1' x 3, 'FE' x 1,
	// C,H,R,N,CRC(2bytes), '4E' x 22, '00' x 12,
	// 'A1' x 4,'FB'('F8') x 1, data(512 bytes),CRC(2bytes),'4E'(gap3)
	
	readTrackDataCount = 0;
	byte* tmppoint = readTrackDataBuf;
	// Fill track header
	for (int i = 0; i < 80; i++) *(tmppoint++) = 0x4E;
	for (int i = 0; i < 12; i++) *(tmppoint++) = 0x00;
	for (int i = 0; i <  3; i++) *(tmppoint++) = 0xC2;
	*(tmppoint++) = 0xFC;
	for (int i = 0; i < 50; i++) *(tmppoint++) = 0x4E;
	// Fill sector
	for (byte j = 0; j < 9; j++) {
		// Fill sector header
		for (int i = 0; i < 12; i++) *(tmppoint++) = 0x00;
		for (int i = 0; i <  3; i++) *(tmppoint++) = 0xA1;
		*(tmppoint++) = 0xFE;
		*(tmppoint++) = track;		//C: Cylinder number
		*(tmppoint++) = side;		//H: Head Address
		*(tmppoint++) = (byte)(j + 1);	//R: Record
		*(tmppoint++) = 0x02;		//N: Number (length of sector)
		*(tmppoint++) = 0x00;		//CRC byte 1
		*(tmppoint++) = 0x00;		//CRC byte 2
		// read sector data
		read(track, j + 1, side, 512, tmppoint);
		tmppoint += 512;
		// end of sector
		*(tmppoint++) = (byte)(j+1);	//CRC byte 1
		*(tmppoint++) = (byte)(j+1);	//CRC byte 2
		// TODO: check this number
		for (int i = 0; i < 55; i++) *(tmppoint++) = 0x4E;
		// TODO build correct CRC and read sector + place gap
	}
	// TODO look up value of this end-of-track gap
	for (int i = 0; i < 1080; i++) *(tmppoint++) = 0x4E;
}

byte FDC_DirAsDSK::readTrackData()
{
	if (readTrackDataCount == RAWTRACK_SIZE) {
		// end of command in any case
		return readTrackDataBuf[RAWTRACK_SIZE];
	} else {
		return readTrackDataBuf[readTrackDataCount++];
	}
}

bool FDC_DirAsDSK::ready()
{
	return true;
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
