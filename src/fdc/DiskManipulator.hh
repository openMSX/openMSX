#ifndef FILEMANIPULATOR_HH
#define FILEMANIPULATOR_HH

#include "Command.hh"
#include "DiskPartition.hh"

#include <array>
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
class MsxChar2Unicode;
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
	std::vector<std::string> getDriveNamesForCurrentMachine() const;
	struct DriveAndPartition {
		DiskContainer* drive;
		std::unique_ptr<DiskPartition> partition; // will often be the full disk
	};
	DiskContainer* getDrive(std::string_view driveName) const;
	std::optional<DriveAndPartition> getDriveAndDisk(std::string_view driveName) const;

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
	                                      DriveSettings& driveData);

	void create(std::span<const TclObject> tokens);
	void partition(std::span<const TclObject> tokens);
	void savedsk(const DriveSettings& driveData, std::string filename);
	void format(std::span<const TclObject> tokens);
	std::string chdir(DriveSettings& driveData, std::string_view filename);
	void mkdir(DriveSettings& driveData, std::string_view filename);
	[[nodiscard]] std::string dir(DriveSettings& driveData);
	std::string import(DriveSettings& driveData,
	                   std::span<const TclObject> lists);
	void exprt(DriveSettings& driveData, std::string_view dirname,
	           std::span<const TclObject> lists);

	Reactor& reactor;
};

} // namespace openmsx

#endif
