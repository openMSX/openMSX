// $Id$

#ifndef __FDC_DirAsDSK__HH__
#define __FDC_DirAsDSK__HH__

#include <map>
#include "SectorBasedDisk.hh"

using std::map;

namespace openmsx {

struct MSXDirEntry {
	byte filename[8];
	byte ext[3];
	byte attrib;
	byte reserved[10]; /* unused */
	byte time[2];
	byte date[2];
	byte startcluster[2];
	byte size[4];
};

struct MappedDirEntry {
	MSXDirEntry msxinfo;
	int filesize; // used to dedect changes that need to be updated in the emulated disk, content changes are automatically handled :-)
	string filename;
};

struct ReverseSector {
	int dirEntryNr;
	long fileOffset;
};

class FDC_DirAsDSK : public SectorBasedDisk
{
public: 
	FDC_DirAsDSK(const string& fileName);
	virtual ~FDC_DirAsDSK();
	virtual void read(byte track, byte sector,
			  byte side, int size, byte* buf);
	virtual void write(byte track, byte sector,
	   byte side, int size, const byte* buf);

	virtual bool writeProtected();
	virtual bool doubleSided();

private:
	static const int MAX_CLUSTER = 720;
	static const int SECTORS_PER_FAT = 3;
	void read(int logicalSector, int size, byte* buf);

	bool checkFileUsedInDSK(const string& fullfilename);
	bool checkMSXFileExists(const string& msxfilename);
	string makeSimpleMSXFileName(const string& fullfilename);
	void addFileToDSK(const string& fullfilename);
	void checkAlterFileInDisk(const string& fullfilename);
	void checkAlterFileInDisk(const int dirindex);
	void updateFileInDisk(const int dirindex);
	void updateFileInDSK(const string& fullfilename);
	int findFirstFreeCluster();
	//int markClusterGetNext();
	word ReadFAT(word clnr);
	void WriteFAT(word clnr, word val);
	MappedDirEntry mapdir[112];	// max nr of entries in root directory: 7 sectors, each 16 entries
	ReverseSector sectormap[1440]; // was 1440, quick hack to fix formatting
	byte FAT[SECTOR_SIZE * SECTORS_PER_FAT];

	static const byte DefaultBootBlock[];
	static const string BootBlockFileName ;
	static const string CachedSectorsFileName ;
	byte BootBlock[SECTOR_SIZE];
	string MSXrootdir;
	map<const int, byte*> cachedSectors;
	bool saveCachedSectors;
};

} // namespace openmsx

#endif
