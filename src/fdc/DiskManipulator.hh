#ifndef FILEMANIPULATOR_HH
#define FILEMANIPULATOR_HH

#include "Command.hh"
#include "DiskPartition.hh"

#include <memory>
#include <optional>
#include <string_view>
#include <vector>

namespace openmsx {

class CommandController;
class DiskContainer;
class SectorAccessibleDisk;
class DiskPartition;
class MSXtar;
class Reactor;
enum class MSXBootSectorType;

class DiskManipulator final : public Command
{
public:
	explicit DiskManipulator(CommandController& commandController,
	                         Reactor& reactor);
	~DiskManipulator();

	void registerDrive(DiskContainer& drive, std::string_view prefix);
	void unregisterDrive(DiskContainer& drive);

	// for use in ImGuiDiskManipulator
	[[nodiscard]] std::vector<std::string> getDriveNamesForCurrentMachine() const;
	struct DriveAndPartition {
		DiskContainer* drive;
		std::unique_ptr<DiskPartition> partition; // will often be the full disk
	};
	[[nodiscard]] DiskContainer* getDrive(std::string_view fullName) const;
	[[nodiscard]] std::optional<DriveAndPartition> getDriveAndDisk(std::string_view fullName) const;
	void create(const std::string& filename_, MSXBootSectorType bootType, const std::vector<unsigned>& sizes) const;

private:
	struct DriveSettings
	{
		std::string getWorkingDir(unsigned p);
		void setWorkingDir(unsigned p, std::string_view dir);

		DiskContainer* drive;
		std::string driveName; // includes machine prefix
		/** 0 = whole disk, 1.. = partition number */
		unsigned partition;
	private:
		std::vector<std::string> workingDir;
	};
	using Drives = std::vector<DriveSettings>;
	Drives drives; // unordered

	// Command interface
	void execute(std::span<const TclObject> tokens, TclObject& result) override;
	[[nodiscard]] std::string help(std::span<const TclObject> tokens) const override;
	void tabCompletion(std::vector<std::string>& tokens) const override;

	[[nodiscard]] std::string getMachinePrefix() const;
	[[nodiscard]] Drives::iterator findDriveSettings(DiskContainer& drive);
	[[nodiscard]] Drives::iterator findDriveSettings(std::string_view driveName);
	[[nodiscard]] DriveSettings& getDriveSettings(std::string_view diskName);
	[[nodiscard]] static DiskPartition getPartition(
		const DriveSettings& driveData);
	[[nodiscard]] MSXtar getMSXtar(SectorAccessibleDisk& disk,
	                               DriveSettings& driveData) const;

	void create(std::span<const TclObject> tokens) const;
	void partition(std::span<const TclObject> tokens);
	void savedsk(const DriveSettings& driveData, std::string filename) const;
	void format(std::span<const TclObject> tokens);
	std::string chdir(DriveSettings& driveData, std::string_view filename) const;
	void mkdir(DriveSettings& driveData, std::string_view filename) const;
	[[nodiscard]] std::string dir(DriveSettings& driveData) const;
	[[nodiscard]] std::string deleteEntry(DriveSettings& driveData, std::string_view entry) const;
	[[nodiscard]] std::string rename(DriveSettings& driveData, std::string_view oldName, std::string_view newName) const;
	std::string imprt(DriveSettings& driveData,
	                  std::span<const TclObject> lists, bool overwrite) const;
	void exprt(DriveSettings& driveData, std::string_view dirname,
	           std::span<const TclObject> lists) const;

	Reactor& reactor;
};

} // namespace openmsx

#endif
