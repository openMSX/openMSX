// $Id$

#ifndef DIRASDSK_HH
#define DIRASDSK_HH

#include "SectorBasedDisk.hh"
#include "GlobalSettings.hh"
#include <map>
#include <vector>

struct stat;

namespace openmsx {

class CliComm;

class DirAsDSK : public SectorBasedDisk
{
public:
	static const unsigned SECTORS_PER_FAT = 3;
	static const unsigned NUM_FAT_ENTRIES = (SECTOR_SIZE * SECTORS_PER_FAT * 2) / 3;
	static const unsigned SECTORS_PER_DIR = 7;
	static const unsigned NUM_DIR_ENTRIES = SECTORS_PER_DIR * (SECTOR_SIZE / 32);

	DirAsDSK(CliComm& cliComm, GlobalSettings& globalSettings,
	         const std::string& fileName);
	virtual ~DirAsDSK();

private:
	// SectorBasedDisk
	virtual void readLogicalSector(unsigned sector, byte* buf);
	virtual void writeLogicalSector(unsigned sector, const byte* buf);
	virtual bool writeProtected();

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
		bool inUse() const { return !shortname.empty(); }

		MSXDirEntry msxinfo;
		std::string shortname;
		int filesize; // used to detect changes that need to be updated in the
		              // emulated disk, content changes are automatically
		              // handled :-)
	};

	enum Usage { CLEAN, CACHED, MIXED };
	struct ReverseSector {
		unsigned long fileOffset;
		Usage usage;
		unsigned dirEntryNr;
	};

	void writeFATSector (unsigned sector, const byte* buf);
	void writeDIRSector (unsigned sector, const byte* buf);
	void writeDataSector(unsigned sector, const byte* buf);
	void writeDIREntry(unsigned dirindex, const MSXDirEntry& entry);
	bool checkFileUsedInDSK(const std::string& filename);
	bool checkMSXFileExists(const std::string& msxfilename);
	void addFileToDSK(const std::string& filename, struct stat& fst);
	void checkAlterFileInDisk(const std::string& filename);
	void checkAlterFileInDisk(unsigned dirindex);
	void updateFileInDisk(unsigned dirindex, struct stat& fst);
	void updateFileInDisk(const std::string& filename);
	void transferFileToCache(unsigned dirindex);
	void extractCacheToFile(unsigned dirindex);
	void truncateCorrespondingFile(unsigned dirindex);
	unsigned findNextFreeCluster(unsigned curcl);
	unsigned findFirstFreeCluster();
	unsigned readFAT(unsigned clnr);
	unsigned readFAT2(unsigned clnr);
	void writeFAT(unsigned clnr, unsigned val);
	void writeFAT2(unsigned clnr, unsigned val);
	bool readCache();
	void saveCache();
	std::string condenseName(const char* buf);
	void updateFileFromAlteredFatOnly(unsigned somecluster);
	void cleandisk();
	void scanHostDir(bool onlyNewFiles);

	CliComm& cliComm; // TODO don't use CliComm to report errors/warnings

	MappedDirEntry mapdir[112]; // max nr of entries in root directory:
	                            // 7 sectors, each 16 entries
	ReverseSector sectormap[1440]; // was 1440, quick hack to fix formatting
	byte fat [SECTOR_SIZE * SECTORS_PER_FAT];
	byte fat2[SECTOR_SIZE * SECTORS_PER_FAT];

	byte bootBlock[SECTOR_SIZE];
	const std::string hostDir;
	typedef std::map<unsigned, std::vector<byte> > CachedSectors;
	CachedSectors cachedSectors;
	GlobalSettings& globalSettings;

	GlobalSettings::SyncMode syncMode;
	bool bootSectorWritten;
};

} // namespace openmsx

#endif
