#ifndef IMGUI_DISK_MANIPULATOR_HH
#define IMGUI_DISK_MANIPULATOR_HH

#include "ImGuiPart.hh"

#include "DiskManipulator.hh"
#include "SectorAccessibleDisk.hh"

#include <ctime>
#include <map>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <variant>
#include <vector>

namespace openmsx {

class DiskContainer;
class DiskPartition;
class MSXtar;

class ImGuiDiskManipulator final : public ImGuiPart
{
public:
	using ImGuiPart::ImGuiPart;

	void paint(MSXMotherBoard* motherBoard) override;

public:
	bool show = false;

private:
	struct FileInfo {
		std::string filename;
		size_t size = 0;
		time_t modified = 0;
		uint8_t attrib = 0;
		bool isDirectory = false;
		bool isSelected = false;
	};
	struct Nop {};
	struct ChangeDir { std::string_view name; };
	struct Delete { std::string_view name; };
	struct Rename { std::string_view name; };
	using Action = std::variant<Nop, ChangeDir, Delete, Rename>;

	struct DrivePartitionTar {
		DiskContainer* drive;
		std::unique_ptr<DiskPartition> disk; // will often be the full disk
		std::unique_ptr<MSXtar> tar;
	};
	[[nodiscard]] DiskContainer* getDrive();
	[[nodiscard]] std::optional<DiskManipulator::DriveAndPartition> getDriveAndDisk();
	[[nodiscard]] std::optional<DrivePartitionTar> getMsxStuff();

	[[nodiscard]] bool isValidMsxDirectory(DrivePartitionTar& stuff, const std::string& dir) const;
	[[nodiscard]] std::string getDiskImageName();
	[[nodiscard]] std::vector<FileInfo> dirMSX(DrivePartitionTar& stuff);
	void refreshMsx(DrivePartitionTar& stuff);
	void refreshHost();
	void checkSort(std::vector<FileInfo>& files, bool& forceSort) const;
	[[nodiscard]] Action drawTable(std::vector<FileInfo>& files, int& lastClickIdx, bool& forceSort, bool drawAttrib) const;
	void insertMsxDisk();
	void exportDiskImage();
	void msxParentDirectory();
	void hostParentDirectory();
	void msxRefresh();
	void hostRefresh();
	[[nodiscard]] bool setupTransferHostToMsx(DrivePartitionTar& stuff);
	void executeTransferHostToMsx(DrivePartitionTar& stuff);
	void transferMsxToHost(DrivePartitionTar& stuff);

private:
	struct PartitionSize {
		unsigned count;
		enum Unit : int { KB, MB };
		int unit = KB;

		[[nodiscard]] unsigned asSectorCount() const {
			return count * (((unit == KB) ? 1024 : (1024 * 1024)) / SectorAccessibleDisk::SECTOR_SIZE);
		}
	};

	std::vector<FileInfo> msxFileCache;
	std::vector<FileInfo> hostFileCache;
	std::string selectedDrive = "virtual_drive";
	std::string msxDir = "/", editMsxDir = "/";
	std::string hostDir, editHostDir;
	std::string editModal;
	std::string renameFrom;
	int msxLastClick = -1;
	int hostLastClick = -1;
	bool hostNeedRefresh = true;
	bool hostForceSort = false;

	enum TransferHostToMsxPhase {
		IDLE,
		CHECK,
		EXECUTE_PRESERVE,
		EXECUTE_OVERWRITE,
	} transferHostToMsxPhase = IDLE;
	std::vector<FileInfo> existingFiles;
	std::vector<FileInfo> existingDirs;
	std::map<std::string, std::vector<FileInfo>, std::less<>> duplicateEntries;

	enum NewDiskType : int { UNPARTITIONED = 0, PARTITIONED = 1 };
	int newDiskType = UNPARTITIONED;
	int bootType = static_cast<int>(MSXBootSectorType::DOS2);
	PartitionSize unpartitionedSize = {720, PartitionSize::KB};
	std::vector<PartitionSize> partitionSizes;
};

} // namespace openmsx

#endif
