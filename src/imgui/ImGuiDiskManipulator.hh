#ifndef IMGUI_DISK_MANIPULATOR_HH
#define IMGUI_DISK_MANIPULATOR_HH

#include "ImGuiPart.hh"

#include <ctime>
#include <string>
#include <string_view>
#include <vector>

namespace openmsx {

class ImGuiManager;

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
		std::string attrib;
		size_t size = 0;
		time_t modified = 0;
		bool isDirectory = false;
		bool isSelected = false;
	};
	void refreshHost();
	void checkSort(std::vector<FileInfo>& files, bool& forceSort);
	std::string_view drawTable(std::vector<FileInfo>& files, int& lastClickIdx, bool& forceSort, bool drawAttrib);
	void hostParentDirectory();
	void hostRefresh();

private:
	std::vector<FileInfo> msxFileCache;
	std::vector<FileInfo> hostFileCache;
	std::string hostDir, editHostDir;
	std::string editNewDir;
	int msxLastClick = -1;
	int hostLastClick = -1;
	bool hostNeedRefresh = true;
	bool msxForceSort = false;
	bool hostForceSort = false;

	ImGuiManager& manager;
};

} // namespace openmsx

#endif
