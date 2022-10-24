#ifndef DIRASDSK_HH
#define DIRASDSK_HH

#include "SectorBasedDisk.hh"
#include "DiskImageUtils.hh"
#include "FileOperations.hh"
#include "EmuTime.hh"
#include "hash_map.hh"
#include <utility>

namespace openmsx {

class DiskChanger;
class CliComm;

class DirAsDSK final : public SectorBasedDisk
{
public:
	enum SyncMode { SYNC_READONLY, SYNC_FULL };
	enum BootSectorType { BOOTSECTOR_DOS1, BOOTSECTOR_DOS2 };

public:
	DirAsDSK(DiskChanger& diskChanger, CliComm& cliComm,
	         const Filename& hostDir, SyncMode syncMode,
	         BootSectorType bootSectorType);

	// SectorBasedDisk
	void readSectorImpl (size_t sector,       SectorBuffer& buf) override;
	void writeSectorImpl(size_t sector, const SectorBuffer& buf) override;
	[[nodiscard]] bool isWriteProtectedImpl() const override;
	[[nodiscard]] bool hasChanged() const override;
	void checkCaches() override;

private:
	struct DirIndex {
		DirIndex() = default;
		DirIndex(unsigned sector_, unsigned idx_)
			: sector(sector_), idx(idx_) {}
		[[nodiscard]] constexpr bool operator==(const DirIndex&) const = default;

		unsigned sector;
		unsigned idx;
	};
	struct HashDirIndex {
		auto operator()(const DirIndex& d) const {
			std::hash<unsigned> subHasher;
			return 31 * subHasher(d.sector)
			          + subHasher(d.idx);
		}
	};
	struct MapDir {
		std::string hostName; // path relative to 'hostDir'
		// The following two are used to detect changes in the host
		// file compared to the last host->virtual-disk sync.
		time_t mtime; // Modification time of host file at the time of
		              // the last sync.
		size_t filesize; // Host file size, normally the same as msx
		                 // filesize, except when the host file was
		                 // truncated.
	};

	[[nodiscard]] SectorBuffer* fat();
	[[nodiscard]] SectorBuffer* fat2();
	[[nodiscard]] MSXDirEntry& msxDir(DirIndex dirIndex);
	void writeFATSector (unsigned sector, const SectorBuffer& buf);
	void writeDIRSector (unsigned sector, DirIndex dirDirIndex,
	                     const SectorBuffer& buf);
	void writeDataSector(unsigned sector, const SectorBuffer& buf);
	void writeDIREntry(DirIndex dirIndex, DirIndex dirDirIndex,
	                   const MSXDirEntry& newEntry);
	void syncWithHost();
	void checkDeletedHostFiles();
	void deleteMSXFile(DirIndex dirIndex);
	void deleteMSXFilesInDir(unsigned msxDirSector);
	void freeFATChain(unsigned cluster);
	void addNewHostFiles(const std::string& hostSubDir, unsigned msxDirSector);
	void addNewDirectory(const std::string& hostSubDir, const std::string& hostName,
	                     unsigned msxDirSector, FileOperations::Stat& fst);
	void addNewHostFile(const std::string& hostSubDir, const std::string& hostName,
	                    unsigned msxDirSector, FileOperations::Stat& fst);
	[[nodiscard]] DirIndex fillMSXDirEntry(
		const std::string& hostSubDir, const std::string& hostName,
		unsigned msxDirSector);
	[[nodiscard]] DirIndex getFreeDirEntry(unsigned msxDirSector);
	[[nodiscard]] DirIndex findHostFileInDSK(std::string_view hostName);
	[[nodiscard]] bool checkFileUsedInDSK(std::string_view hostName);
	[[nodiscard]] unsigned nextMsxDirSector(unsigned sector);
	[[nodiscard]] bool checkMSXFileExists(std::span<const char, 11> msxfilename,
	                                      unsigned msxDirSector);
	void checkModifiedHostFiles();
	void setMSXTimeStamp(DirIndex dirIndex, FileOperations::Stat& fst);
	void importHostFile(DirIndex dirIndex, FileOperations::Stat& fst);
	void exportToHost(DirIndex dirIndex, DirIndex dirDirIndex);
	void exportToHostDir (DirIndex dirIndex, const std::string& hostName);
	void exportToHostFile(DirIndex dirIndex, const std::string& hostName);
	[[nodiscard]] unsigned findNextFreeCluster(unsigned cluster);
	[[nodiscard]] unsigned findFirstFreeCluster();
	[[nodiscard]] unsigned getFreeCluster();
	[[nodiscard]] unsigned readFAT(unsigned cluster);
	void writeFAT12(unsigned cluster, unsigned val);
	void exportFileFromFATChange(unsigned cluster, SectorBuffer* oldFAT);
	std::pair<unsigned, unsigned> getChainStart(unsigned cluster);
	[[nodiscard]] bool isDirSector(unsigned sector, DirIndex& dirDirIndex);
	bool getDirEntryForCluster(unsigned cluster,
	                           DirIndex& dirIndex, DirIndex& dirDirIndex);
	[[nodiscard]] DirIndex getDirEntryForCluster(unsigned cluster);
	void unmapHostFiles(unsigned msxDirSector);
	template<typename FUNC> bool scanMsxDirs(
		FUNC func, unsigned msxDirSector);
	friend struct NullScanner;
	friend struct DirScanner;
	friend struct IsDirSector;
	friend struct DirEntryForCluster;
	friend struct UnmapHostFiles;

	// internal helper functions
	[[nodiscard]] unsigned readFATHelper(const SectorBuffer* fat, unsigned cluster) const;
	void writeFATHelper(SectorBuffer* fat, unsigned cluster, unsigned val) const;
	[[nodiscard]] unsigned clusterToSector(unsigned cluster) const;
	[[nodiscard]] std::pair<unsigned, unsigned> sectorToClusterOffset(unsigned sector) const;
	[[nodiscard]] unsigned sectorToCluster(unsigned sector) const;

private:
	DiskChanger& diskChanger; // used to query time / report disk change
	CliComm& cliComm; // TODO don't use CliComm to report errors/warnings
	const std::string hostDir;
	const SyncMode syncMode;

	EmuTime lastAccess; // last time there was a sector read/write

	// For each directory entry that has a mapped host file/directory we
	// store the name, last modification time and size of the corresponding
	// host file/dir.
	using MapDirs = hash_map<DirIndex, MapDir, HashDirIndex>;
	MapDirs mapDirs;

	// format parameters which depend on single/double sided
	// varying root parameters
	const unsigned nofSectors;
	const unsigned nofSectorsPerFat;
	// parameters that depend on these and thus also vary
	const unsigned firstSector2ndFAT;
	const unsigned firstDirSector;
	const unsigned firstDataSector;
	const unsigned maxCluster; // First cluster number that can NOT be used anymore.

	// Storage for the whole virtual disk.
	std::vector<SectorBuffer> sectors;
};

} // namespace openmsx

#endif
