// This code implements the functionality of my older msxtar program
// that could manipulate files and directories on dsk and ide-hd images
// Integrating it is seen as temporary bypassing of the need for a
// DirAsDisk2 that supports subdirs, partitions etc. since this class will
// of those functionalities although not on a dynamic base

#ifndef MSXTAR_HH
#define MSXTAR_HH

#include "MemBuffer.hh"
#include "DiskImageUtils.hh"
#include "openmsx.hh"
#include "noncopyable.hh"
#include "string_ref.hh"

namespace openmsx {

class SectorAccessibleDisk;

class MSXtar : private noncopyable
{
public:
	explicit MSXtar(SectorAccessibleDisk& disk);
	~MSXtar();

	void chdir(const std::string& newRootDir);
	void mkdir(const std::string& newRootDir);
	std::string dir();
	std::string addFile(const std::string& filename);
	std::string addDir(const std::string& rootDirName);
	std::string getItemFromDir(const std::string& rootDirName,
	                           const std::string& itemName);
	void getDir(const std::string& rootDirName);

private:
	struct DirEntry {
		unsigned sector;
		unsigned index;
	};

	void writeLogicalSector(unsigned sector, const byte* buf);
	void readLogicalSector (unsigned sector,       byte* buf);

	unsigned clusterToSector(unsigned cluster);
	unsigned sectorToCluster(unsigned sector);
	void parseBootSector(const MSXBootSector& boot);
	unsigned readFAT(unsigned clnr) const;
	void writeFAT(unsigned clnr, unsigned val);
	unsigned findFirstFreeCluster();
	unsigned findUsableIndexInSector(unsigned sector);
	unsigned getNextSector(unsigned sector);
	unsigned getStartCluster(const MSXDirEntry& entry);
	unsigned appendClusterToSubdir(unsigned sector);
	DirEntry addEntryToDir(unsigned sector);
	unsigned addSubdir(const std::string& msxName,
	                   unsigned t, unsigned d, unsigned sector);
	void alterFileInDSK(MSXDirEntry& msxDirEntry, const std::string& hostName);
	unsigned addSubdirToDSK(const std::string& hostName,
	                   const std::string& msxName, unsigned sector);
	DirEntry findEntryInDir(const std::string& name, unsigned sector,
	                        byte* sectorbuf);
	std::string addFileToDSK(const std::string& hostName, unsigned sector);
	std::string recurseDirFill(const std::string& dirName, unsigned sector);
	std::string condensName(const MSXDirEntry& dirEntry);
	void changeTime (const std::string& resultFile, const MSXDirEntry& dirEntry);
	void fileExtract(const std::string& resultFile, const MSXDirEntry& dirEntry);
	void recurseDirExtract(const std::string& dirName, unsigned sector);
	std::string singleItemExtract(const std::string& dirName,
	                              const std::string& itemName, unsigned sector);
	void chroot(string_ref newRootDir, bool createDir);


	SectorAccessibleDisk& disk;
	MemBuffer<byte> fatBuffer;

	unsigned maxCluster;
	unsigned sectorsPerCluster;
	unsigned sectorsPerFat;
	unsigned rootDirStart; // first sector from the root directory
	unsigned rootDirLast;  // last  sector from the root directory
	unsigned chrootSector;

	bool fatCacheDirty;
};

} // namespace openmsx

#endif
