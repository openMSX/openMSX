#ifndef IMGUI_DISK_MANIPULATOR_HH
#define IMGUI_DISK_MANIPULATOR_HH

#include "ImGuiPart.hh"

#include "DiskManipulator.hh"

#include <ctime>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace openmsx {

class DiskContainer;
class DiskPartition;
class ImGuiManager;
class MSXtar;

class ImGuiDiskManipulator final : public ImGuiPart
{
public:
	explicit ImGuiDiskManipulator(ImGuiManager& manager);

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

	struct DrivePartitionTar {
		DiskContainer* drive;
		std::unique_ptr<DiskPartition> disk; // will often be the full disk
		std::unique_ptr<MSXtar> tar;
	};
	DiskContainer* getDrive();
	std::optional<DiskManipulator::DriveAndPartition> getDriveAndDisk();
	std::optional<DrivePartitionTar> getMsxStuff();

	bool isValidMsxDirectory(DrivePartitionTar& stuff, const std::string& dir);
	std::string getDiskImageName();
	void refreshMsx();
	void refreshHost();
	void checkSort(std::vector<FileInfo>& files, bool& forceSort);
	std::string_view drawTable(std::vector<FileInfo>& files, int& lastClickIdx, bool& forceSort, bool drawAttrib);
	void insertMsxDisk();
	void exportDiskImage();
	void msxParentDirectory();
	void hostParentDirectory();
	void msxRefresh();
	void hostRefresh();

private:
	std::vector<FileInfo> msxFileCache;
	std::vector<FileInfo> hostFileCache;
	std::string selectedDrive = "virtual_drive";
	std::string msxDir = "/", editMsxDir = "/";
	std::string hostDir, editHostDir;
	std::string editNewDir;
	int msxLastClick = -1;
	int hostLastClick = -1;
	bool msxNeedRefresh = true;
	bool hostNeedRefresh = true;
	bool msxForceSort = false;
	bool hostForceSort = false;

	ImGuiManager& manager;
};

} // namespace openmsx

#endif
