// This code implements the functionality of my older msxtar program
// that could manipulate files and directories on dsk and ide-hd images
// Integrating it is seen as temporary bypassing of the need for a
// DirAsDisk2 that supports subdirs, partitions etc. since this class will
// of those functionalities although not on a dynamic base

#ifndef MSXTAR_HH
#define MSXTAR_HH

#include "MemBuffer.hh"
#include "DiskImageUtils.hh"
#include "zstring_view.hh"
#include <string_view>
#include <variant>

namespace openmsx {

class SectorAccessibleDisk;

namespace FAT {
	struct Free {
		[[nodiscard]] auto operator<=>(const Free&) const = default;
	};

	struct EndOfChain {
		[[nodiscard]] auto operator<=>(const EndOfChain&) const = default;
	};

	struct Cluster {
		unsigned index;
		[[nodiscard]] auto operator<=>(const Cluster&) const = default;
	};

	using DirCluster = std::variant<Free, Cluster>;
	using FatCluster = std::variant<Free, EndOfChain, Cluster>;
}

class MSXtar
{
public:
	explicit MSXtar(SectorAccessibleDisk& disk);
	MSXtar(MSXtar&& other) noexcept;
	~MSXtar();

	void chdir(std::string_view newRootDir);
	void mkdir(std::string_view newRootDir);
	std::string dir();
	std::string addFile(const std::string& filename);
	std::string addDir(std::string_view rootDirName);
	std::string getItemFromDir(std::string_view rootDirName, std::string_view itemName);
	void getDir(std::string_view rootDirName);

private:
	struct DirEntry {
		unsigned sector;
		unsigned index;
	};

	void writeLogicalSector(unsigned sector, const SectorBuffer& buf);
	void readLogicalSector (unsigned sector,       SectorBuffer& buf);

	[[nodiscard]] unsigned clusterToSector(FAT::Cluster cluster) const;
	[[nodiscard]] FAT::Cluster sectorToCluster(unsigned sector) const;
	void parseBootSector(const MSXBootSector& boot);
	[[nodiscard]] FAT::FatCluster readFAT(FAT::Cluster index) const;
	void writeFAT(FAT::Cluster index, FAT::FatCluster value);
	FAT::Cluster findFirstFreeCluster();
	unsigned findUsableIndexInSector(unsigned sector);
	unsigned getNextSector(unsigned sector);
	unsigned appendClusterToSubdir(unsigned sector);
	DirEntry addEntryToDir(unsigned sector);
	unsigned addSubdir(std::string_view msxName,
	                   uint16_t t, uint16_t d, unsigned sector);
	void alterFileInDSK(MSXDirEntry& msxDirEntry, const std::string& hostName);
	unsigned addSubdirToDSK(zstring_view hostName,
	                        std::string_view msxName, unsigned sector);
	DirEntry findEntryInDir(const std::string& name, unsigned sector,
	                        SectorBuffer& sectorBuf);
	std::string addFileToDSK(const std::string& fullHostName, unsigned sector);
	std::string recurseDirFill(std::string_view dirName, unsigned sector);
	void fileExtract(const std::string& resultFile, const MSXDirEntry& dirEntry);
	void recurseDirExtract(std::string_view dirName, unsigned sector);
	std::string singleItemExtract(std::string_view dirName, std::string_view itemName,
	                              unsigned sector);
	void chroot(std::string_view newRootDir, bool createDir);

private:
	[[nodiscard]] FAT::DirCluster getStartCluster(const MSXDirEntry& entry) const;
	void setStartCluster(MSXDirEntry& entry, FAT::DirCluster cluster) const;

	SectorAccessibleDisk& disk;
	MemBuffer<SectorBuffer> fatBuffer;

	unsigned clusterCount;
	unsigned fatCount;
	unsigned sectorsPerCluster;
	unsigned sectorsPerFat;
	unsigned fatStart;     // first sector of the first FAT
	unsigned rootDirStart; // first sector of the root directory
	unsigned dataStart;    // first sector of the cluster data
	unsigned chrootSector;
	bool fat16;

	bool fatCacheDirty;
};

} // namespace openmsx

#endif
