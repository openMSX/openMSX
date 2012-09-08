// $Id$

#ifndef DIRASDSK_HH
#define DIRASDSK_HH

#include "SectorBasedDisk.hh"
#include "DiskImageUtils.hh"
#include "EmuTime.hh"
#include <map>

struct stat;

namespace openmsx {

class DiskChanger;
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

	// REDEFINE (first definition is in SectorAccessibleDisk.hh)
	//  to solve link error with i686-apple-darwin9-gcc-4.0.1
	static const unsigned SECTOR_SIZE = 512;

public:
	DirAsDSK(DiskChanger& diskChanger, CliComm& cliComm,
	         const Filename& hostDir, SyncMode syncMode,
	         BootSectorType bootSectorType);

	// SectorBasedDisk
	virtual void readSectorImpl(unsigned sector, byte* buf);
	virtual void writeSectorImpl(unsigned sector, const byte* buf);
	virtual bool isWriteProtectedImpl() const;
	virtual void checkCaches();

private:
	void writeFATSector (unsigned sector, const byte* buf);
	void writeDIRSector (unsigned sector, const byte* buf);
	void writeDataSector(unsigned sector, const byte* buf);
	void writeDIREntry(unsigned dirIndex, const MSXDirEntry& newEntry);
	bool checkFileUsedInDSK(const std::string& hostName);
	bool checkMSXFileExists(const std::string& msxfilename);
	void addFileToDSK(const std::string& hostName, struct stat& fst);
	void checkAlterFileInDisk(const std::string& hostName);
	void checkAlterFileInDisk(unsigned dirIndex);
	void updateFileInDisk(unsigned dirIndex, struct stat& fst);
	void updateFileInDisk(const std::string& hostName);
	void extractCacheToFile(unsigned dirIndex);
	void truncateCorrespondingFile(unsigned dirIndex);
	unsigned findNextFreeCluster(unsigned curcl);
	unsigned findFirstFreeCluster();
	unsigned readFAT(unsigned clnr);
	unsigned readFAT2(unsigned clnr);
	void writeFAT12(unsigned clnr, unsigned val);
	void writeFAT2 (unsigned clnr, unsigned val);
	void updateFileFromAlteredFatOnly(unsigned someCluster);
	void cleandisk();
	void scanHostDir(bool onlyNewFiles);

private:
	DiskChanger& diskChanger; // used to query time / report disk change
	CliComm& cliComm; // TODO don't use CliComm to report errors/warnings
	const std::string hostDir;
	const SyncMode syncMode;

	EmuTime lastAccess; // last time there was a sector read/write

	struct {
		bool inUse() const { return !hostName.empty(); }

		MSXDirEntry msxInfo;
		std::string hostName; // path relative to 'hostDir'
		int filesize; // used to detect changes that need to be updated in the
		              // emulated disk, content changes are automatically
		              // handled :-)
	} mapDir[NUM_DIR_ENTRIES];

	enum Usage { CLEAN, CACHED, MIXED };
	struct {
		unsigned long fileOffset;
		Usage usage;
		unsigned dirIndex;
	} sectorMap[NUM_SECTORS];

	struct SectorData {
		byte data[SECTOR_SIZE];
	};

	MSXBootSector bootBlock;
	byte fat [SECTOR_SIZE * SECTORS_PER_FAT];
	byte fat2[SECTOR_SIZE * SECTORS_PER_FAT];
	typedef std::map<unsigned, SectorData> CachedSectors;
	CachedSectors cachedSectors;
};

} // namespace openmsx

#endif
