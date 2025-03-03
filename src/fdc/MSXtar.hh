// This code implements the functionality of my older msxtar program
// that could manipulate files and directories on dsk and ide-hd images
// Integrating it is seen as temporary bypassing of the need for a
// DirAsDisk2 that supports subdirs, partitions etc. since this class will
// of those functionalities although not on a dynamic base

#ifndef MSXTAR_HH
#define MSXTAR_HH

#include "DiskImageUtils.hh"
#include "TclObject.hh"

#include "MemBuffer.hh"
#include "zstring_view.hh"

#include <cstdint>
#include <string_view>
#include <variant>

namespace openmsx {

class SectorAccessibleDisk;
class MsxChar2Unicode;

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

	using FileName = decltype(MSXDirEntry::filename);
}

class MSXtar
{
public:
	enum class Add : uint8_t {
		PRESERVE,
		OVERWRITE,
	};
public:
	explicit MSXtar(SectorAccessibleDisk& disk, const MsxChar2Unicode& msxChars_);
	MSXtar(MSXtar&& other) noexcept;
	~MSXtar();

	void chdir(std::string_view newRootDir);
	void mkdir(std::string_view newRootDir);
	std::string dir(); // formatted output
	TclObject dirRaw(); // unformatted output
	std::string addItem(const std::string& hostItemName, Add add); // add file or directory
	std::string addFile(const std::string& filename, Add add);
	std::string addDir(std::string_view rootDirName, Add add); // add files from host-dir, but does not create a top-level dir on the msx side
	std::string getItemFromDir(std::string_view rootDirName, std::string_view itemName);
	void getDir(std::string_view rootDirName);
	std::string deleteItem(std::string_view itemName); // delete file or directory (recursive)
	std::string renameItem(std::string_view currentName, std::string_view newName); // rename file or directory
	[[nodiscard]] std::string convertToMsxName(std::string_view name) const; // truncate to 8.3 filename

	struct FreeSpaceResult {
		unsigned numFreeClusters;
		unsigned clusterSize; // in bytes
	};
	[[nodiscard]] FreeSpaceResult getFreeSpace() const;

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
	[[nodiscard]] FAT::Cluster findFirstFreeCluster();
	[[nodiscard]] unsigned countFreeClusters() const;
	[[nodiscard]] unsigned findUsableIndexInSector(unsigned sector);
	[[nodiscard]] unsigned getNextSector(unsigned sector) const;
	[[nodiscard]] unsigned appendClusterToSubdir(unsigned sector);
	[[nodiscard]] DirEntry addEntryToDir(unsigned sector);
	[[nodiscard]] unsigned addSubdir(const FAT::FileName& msxName,
	                                 uint16_t t, uint16_t d, unsigned sector);
	void alterFileInDSK(MSXDirEntry& msxDirEntry, const std::string& hostName);
	void deleteItem(std::string_view itemName, unsigned sector);
	std::string deleteEntry(const FAT::FileName& msxName, unsigned rootSector);
	void deleteEntry(MSXDirEntry& msxDirEntry);
	void deleteDirectory(unsigned sector);
	void freeFatChain(FAT::FatCluster cluster);
	[[nodiscard]] unsigned addSubdirToDSK(zstring_view hostName,
	                                      const FAT::FileName& msxName, unsigned sector);
	[[nodiscard]] DirEntry findEntryInDir(const FAT::FileName& msxName, unsigned sector,
	                                      SectorBuffer& sectorBuf);
	std::string addFileToDSK(const std::string& fullHostName, unsigned sector, Add add);
	std::string addOrCreateSubdir(zstring_view hostDirName, unsigned sector, Add add);
	std::string recurseDirFill(std::string_view dirName, unsigned sector, Add add);
	void fileExtract(const std::string& resultFile, const MSXDirEntry& dirEntry);
	void recurseDirExtract(std::string_view dirName, unsigned sector);
	std::string singleItemExtract(std::string_view dirName, std::string_view itemName,
	                              unsigned sector);
	void chroot(std::string_view newRootDir, bool createDir);

private:
	[[nodiscard]] FAT::DirCluster getStartCluster(const MSXDirEntry& entry) const;
	void setStartCluster(MSXDirEntry& entry, FAT::DirCluster cluster) const;

	[[nodiscard]] FAT::FileName hostToMSXFileName(std::string_view hostName) const;
	[[nodiscard]] std::string msxToHostFileName(const FAT::FileName& msxName) const;

	SectorAccessibleDisk& disk;
	MemBuffer<SectorBuffer> fatBuffer;
	const MsxChar2Unicode& msxChars;
	FAT::Cluster findFirstFreeClusterStart{0}; // all clusters before this one are in use

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
