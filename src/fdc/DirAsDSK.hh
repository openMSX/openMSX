// $Id$

#ifndef DIRASDSK_HH
#define DIRASDSK_HH

#include "SectorBasedDisk.hh"
#include "GlobalSettings.hh"
#include <map>
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

	bool checkFileUsedInDSK(const std::string& filename);
	bool checkMSXFileExists(const std::string& msxfilename);
	void addFileToDSK(const std::string& filename);
	void checkAlterFileInDisk(const std::string& filename);
	void checkAlterFileInDisk(int dirindex);
	void updateFileInDisk(int dirindex);
	void updateFileInDisk(const std::string& filename);
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
	void scanHostDir(bool onlyNewFiles);

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
		int dirEntryNr;
	};

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
