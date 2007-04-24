// $Id$

#ifndef FDC_DIRASDSK_HH
#define FDC_DIRASDSK_HH

#include "SectorBasedDisk.hh"
#include <map>
#include <vector>

namespace openmsx {

class CliComm;
class GlobalSettings;

class FDC_DirAsDSK : public SectorBasedDisk
{
public:
	FDC_DirAsDSK(CliComm& cliComm, GlobalSettings& globalSettings,
	             const std::string& fileName);
	virtual ~FDC_DirAsDSK();

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
	int findFirstFreeCluster();
	word readFAT(word clnr);
	void writeFAT(word clnr, word val);

	struct MSXDirEntry {
		byte filename[8];
		byte ext[3];
		byte attrib;
		byte reserved[10];
		byte time[2];
		byte date[2];
		byte startcluster[2];
		byte size[4];
	};

	struct MappedDirEntry {
		MSXDirEntry msxinfo;
		int filesize; // used to dedect changes that need to be updated in the
			      // emulated disk, content changes are automatically
			      // handled :-)
		std::string filename;
	};

	struct ReverseSector {
		int dirEntryNr;
		long fileOffset;
	};

	CliComm& cliComm; // TODO don't use CliComm to report errors/warnings

	MappedDirEntry mapdir[112]; // max nr of entries in root directory:
	                            // 7 sectors, each 16 entries
	ReverseSector sectormap[1440]; // was 1440, quick hack to fix formatting
	byte fat[SECTOR_SIZE * SECTORS_PER_FAT];

	byte bootBlock[SECTOR_SIZE];
	std::string hostDir;
	typedef std::map<unsigned, std::vector<byte> > CachedSectors;
	CachedSectors cachedSectors;
	bool saveCachedSectors;
};

} // namespace openmsx

#endif
