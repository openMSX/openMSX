// $Id$

#ifndef DIRASDSK_HH
#define DIRASDSK_HH

#include "SectorBasedDisk.hh"
#include "GlobalSettings.hh"
#include <map>
#include <vector>

namespace openmsx {

class CliComm;
//class GlobalSettings;

class DirAsDSK : public SectorBasedDisk
{
public:
	DirAsDSK(CliComm& cliComm, GlobalSettings& globalSettings,
	             const std::string& fileName);
	virtual ~DirAsDSK();

private:
	static const int SECTORS_PER_FAT = 3;

	// SectorBasedDisk
	virtual void readLogicalSector(unsigned sector, byte* buf);
	virtual void writeLogicalSector(unsigned sector, const byte* buf);
	virtual bool writeProtected();

	bool checkFileUsedInDSK(const std::string& fullfilename);
	bool checkMSXFileExists(const std::string& msxfilename);
	void addFileToDSK(const std::string& fullfilename);
	void checkAlterFileInDisk(const std::string& fullfilename);
	void checkAlterFileInDisk(const int dirindex);
	void updateFileInDisk(const int dirindex);
	void updateFileInDSK(const std::string& fullfilename);
	void transferFileToCache(const int dirindex);
	void extractCacheToFile(const int dirindex);
	void truncateCorrespondingFile(const int dirindex);
	int findFirstFreeCluster();
	word readFAT(word clnr);
	word readFAT2(word clnr);
	void writeFAT(word clnr, word val);
	void writeFAT2(word clnr, word val);

	struct MSXDirEntry {
		char filename[8];
		byte ext[3];
		byte attrib[1];
		byte reserved[10];
		byte time[2];
		byte date[2];
		byte startcluster[2];
		byte size[4];
	};

	struct MappedDirEntry {
		MSXDirEntry msxinfo;
		std::string filename;
		std::string shortname;
		int filesize; // used to dedect changes that need to be updated in the
			      // emulated disk, content changes are automatically
			      // handled :-)
	};

	typedef enum Usage {CLEAN,CACHED,MIXED} Usage_t;
	struct ReverseSector {
		long fileOffset;
		Usage_t usage;
		int dirEntryNr;
	};

	CliComm& cliComm; // TODO don't use CliComm to report errors/warnings

	MappedDirEntry mapdir[112]; // max nr of entries in root directory:
	                            // 7 sectors, each 16 entries
	ReverseSector sectormap[1440]; // was 1440, quick hack to fix formatting
	byte fat[SECTOR_SIZE * SECTORS_PER_FAT];
	byte fat2[SECTOR_SIZE * SECTORS_PER_FAT];

	byte bootBlock[SECTOR_SIZE];
	std::string hostDir;
	typedef std::map<unsigned, std::vector<byte> > CachedSectors;
	CachedSectors cachedSectors;
	GlobalSettings* globalSettings;

	GlobalSettings::SyncMode_t syncMode;
	bool bootSectorWritten;
	bool readBootBlockFromFile;
	std::string condenseName(const byte* buf);
	void updateFileFromAlteredFatOnly(int somecluster);
	void scanHostDir();

	typedef std::map<std::string,bool> DiscoveredFiles;
	DiscoveredFiles discoveredFiles;
};

} // namespace openmsx

#endif
