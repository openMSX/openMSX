// $Id$

#ifndef DIRASDSK_HH
#define DIRASDSK_HH

#include "SectorBasedDisk.hh"
#include "GlobalSettings.hh"
#include <map>
#include <set>
#include <vector>

namespace openmsx {

class CliComm;

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
	void checkAlterFileInDisk(int dirindex);
	void updateFileInDisk(int dirindex);
	void updateFileInDSK(const std::string& fullfilename);
	void transferFileToCache(int dirindex);
	void extractCacheToFile(int dirindex);
	void truncateCorrespondingFile(int dirindex);
	int findFirstFreeCluster();
	unsigned readFAT(unsigned clnr);
	unsigned readFAT2(unsigned clnr);
	void writeFAT(unsigned clnr, unsigned val);
	void writeFAT2(unsigned clnr, unsigned val);

	bool readCache();
	void saveCache();

	std::string condenseName(const byte* buf);
	void updateFileFromAlteredFatOnly(int somecluster);
	void cleandisk();
	void scanHostDir();

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

	enum Usage {CLEAN, CACHED,MIXED};
	struct ReverseSector {
		unsigned long fileOffset;
		Usage usage;
		int dirEntryNr;
	};

	CliComm& cliComm; // TODO don't use CliComm to report errors/warnings

	MappedDirEntry mapdir[112]; // max nr of entries in root directory:
	                            // 7 sectors, each 16 entries
	ReverseSector sectormap[1440]; // was 1440, quick hack to fix formatting
	byte fat [SECTOR_SIZE * SECTORS_PER_FAT];
	byte fat2[SECTOR_SIZE * SECTORS_PER_FAT];

	byte bootBlock[SECTOR_SIZE];
	std::string hostDir;
	typedef std::map<unsigned, std::vector<byte> > CachedSectors;
	CachedSectors cachedSectors;
	GlobalSettings& globalSettings;

	GlobalSettings::SyncMode syncMode;
	bool bootSectorWritten;

	typedef std::set<std::string> DiscoveredFiles;
	DiscoveredFiles discoveredFiles;
};

} // namespace openmsx

#endif
