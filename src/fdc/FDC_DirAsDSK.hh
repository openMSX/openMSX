// $Id$

#ifndef __FDC_DirAsDSK__HH__
#define __FDC_DirAsDSK__HH__

#include <map>
#include "SectorBasedDisk.hh"

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
	std::string filename;
};

struct ReverseSector {
	int dirEntryNr;
	long fileOffset;
};

class FDC_DirAsDSK : public SectorBasedDisk
{
public: 
	FDC_DirAsDSK(const std::string& fileName);
	virtual ~FDC_DirAsDSK();

	virtual void write(byte track, byte sector,
	                   byte side, unsigned size, const byte* buf);
	virtual bool writeProtected();

private:
	static const int MAX_CLUSTER = 720;
	static const int SECTORS_PER_FAT = 3;
	virtual void readLogicalSector(unsigned sector, byte* buf);

	bool checkFileUsedInDSK(const std::string& fullfilename);
	bool checkMSXFileExists(const std::string& msxfilename);
	std::string makeSimpleMSXFileName(const std::string& fullfilename);
	void addFileToDSK(const std::string& fullfilename);
	void checkAlterFileInDisk(const std::string& fullfilename);
	void checkAlterFileInDisk(const int dirindex);
	void updateFileInDisk(const int dirindex);
	void updateFileInDSK(const std::string& fullfilename);
	int findFirstFreeCluster();
	//int markClusterGetNext();
	word ReadFAT(word clnr);
	void WriteFAT(word clnr, word val);
	MappedDirEntry mapdir[112];	// max nr of entries in root directory: 7 sectors, each 16 entries
	ReverseSector sectormap[1440]; // was 1440, quick hack to fix formatting
	byte FAT[SECTOR_SIZE * SECTORS_PER_FAT];

	static const byte DefaultBootBlock[];
	static const std::string BootBlockFileName ;
	static const std::string CachedSectorsFileName ;
	byte BootBlock[SECTOR_SIZE];
	std::string MSXrootdir;
	std::map<const int, byte*> cachedSectors;
	bool saveCachedSectors;
};

} // namespace openmsx

#endif
