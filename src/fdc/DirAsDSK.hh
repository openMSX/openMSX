// $Id$

#ifndef DIRASDSK_HH
#define DIRASDSK_HH

#include "SectorBasedDisk.hh"
#include "DiskImageUtils.hh"
#include <map>

struct stat;

namespace openmsx {

class CliComm;

class DirAsDSK : public SectorBasedDisk
{
public:
	enum SyncMode { SYNC_READONLY, SYNC_CACHEDWRITE, SYNC_NODELETE, SYNC_FULL };
	enum BootSectorType { BOOTSECTOR_DOS1, BOOTSECTOR_DOS2 };

	static const unsigned SECTORS_PER_FAT = 3;
	static const unsigned SECTORS_PER_DIR = 7;
	static const unsigned DIR_ENTRIES_PER_SECTOR =
	       SECTOR_SIZE / sizeof(MSXDirEntry);
	static const unsigned NUM_DIR_ENTRIES =
		SECTORS_PER_DIR * DIR_ENTRIES_PER_SECTOR;
	static const unsigned NUM_SECTORS = 1440;

public:
	DirAsDSK(CliComm& cliComm, const Filename& filename,
	         SyncMode syncMode, BootSectorType bootSectorType);
	virtual ~DirAsDSK();

private:
	// SectorBasedDisk
	virtual void readSectorImpl(unsigned sector, byte* buf);
	virtual void writeSectorImpl(unsigned sector, const byte* buf);
	virtual bool isWriteProtectedImpl() const;

	struct MappedDirEntry {
		bool inUse() const { return !shortName.empty(); }

		MSXDirEntry msxInfo;
		std::string shortName;
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

	struct SectorData {
		byte data[SECTOR_SIZE];
	};

	void writeFATSector (unsigned sector, const byte* buf);
	void writeDIRSector (unsigned sector, const byte* buf);
	void writeDataSector(unsigned sector, const byte* buf);
	void writeDIREntry(unsigned dirIndex, const MSXDirEntry& entry);
	bool checkFileUsedInDSK(const std::string& filename);
	bool checkMSXFileExists(const std::string& msxfilename);
	void addFileToDSK(const std::string& filename, struct stat& fst);
	void checkAlterFileInDisk(const std::string& filename);
	void checkAlterFileInDisk(unsigned dirIndex);
	void updateFileInDisk(unsigned dirIndex, struct stat& fst);
	void updateFileInDisk(const std::string& filename);
	void extractCacheToFile(unsigned dirIndex);
	void truncateCorrespondingFile(unsigned dirIndex);
	unsigned findNextFreeCluster(unsigned curcl);
	unsigned findFirstFreeCluster();
	unsigned readFAT(unsigned clnr);
	unsigned readFAT2(unsigned clnr);
	void writeFAT12(unsigned clnr, unsigned val);
	void writeFAT2 (unsigned clnr, unsigned val);
	unsigned getStartCluster(const MSXDirEntry& entry);
	bool readCache();
	void saveCache();
	std::string condenseName(const char* buf);
	void updateFileFromAlteredFatOnly(unsigned someCluster);
	void cleandisk();
	void scanHostDir(bool onlyNewFiles);

	CliComm& cliComm; // TODO don't use CliComm to report errors/warnings

	MappedDirEntry mapDir[NUM_DIR_ENTRIES];
	ReverseSector sectorMap[NUM_SECTORS];
	byte fat [SECTOR_SIZE * SECTORS_PER_FAT];
	byte fat2[SECTOR_SIZE * SECTORS_PER_FAT];

	MSXBootSector bootBlock;
	const std::string hostDir;
	typedef std::map<unsigned, SectorData> CachedSectors;
	CachedSectors cachedSectors;

	SyncMode syncMode;

	// REDEFINE (first definition is in SectorAccessibleDisk.hh)
	//  to solve link error with i686-apple-darwin9-gcc-4.0.1
	static const unsigned SECTOR_SIZE;
};

} // namespace openmsx

#endif
