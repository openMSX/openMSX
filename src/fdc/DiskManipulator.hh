#ifndef FILEMANIPULATOR_HH
#define FILEMANIPULATOR_HH

#include "Command.hh"
#include <string_view>
#include <vector>
#include <memory>

namespace openmsx {

class CommandController;
class DiskContainer;
class SectorAccessibleDisk;
class DiskPartition;
class MSXtar;
class Reactor;

class DiskManipulator final : public Command
{
public:
	explicit DiskManipulator(CommandController& commandController,
	                         Reactor& reactor);
	~DiskManipulator();

	void registerDrive(DiskContainer& drive, std::string_view prefix);
	void unregisterDrive(DiskContainer& drive);

private:
	static constexpr unsigned MAX_PARTITIONS = 31;
	struct DriveSettings
	{
		DiskContainer* drive;
		std::string driveName; // includes machine prefix
		std::string workingDir[MAX_PARTITIONS + 1];
		/** 0 = whole disk, 1..MAX_PARTITIONS = partition number */
		unsigned partition;
	};
	using Drives = std::vector<DriveSettings>;
	Drives drives; // unordered

	// Command interface
	void execute(span<const TclObject> tokens, TclObject& result) override;
	[[nodiscard]] std::string help(span<const TclObject> tokens) const override;
	void tabCompletion(std::vector<std::string>& tokens) const override;

	[[nodiscard]] std::string getMachinePrefix() const;
	[[nodiscard]] Drives::iterator findDriveSettings(DiskContainer& drive);
	[[nodiscard]] Drives::iterator findDriveSettings(std::string_view driveName);
	[[nodiscard]] DriveSettings& getDriveSettings(std::string_view diskname);
	[[nodiscard]] static DiskPartition getPartition(
		const DriveSettings& driveData);
	[[nodiscard]] static MSXtar getMSXtar(SectorAccessibleDisk& disk,
	                                      DriveSettings& driveData);

	static void create(span<const TclObject> tokens);
	void savedsk(const DriveSettings& driveData, std::string filename);
	void format(DriveSettings& driveData, bool dos1);
	std::string chdir(DriveSettings& driveData, std::string_view filename);
	void mkdir(DriveSettings& driveData, std::string_view filename);
	[[nodiscard]] std::string dir(DriveSettings& driveData);
	std::string import(DriveSettings& driveData,
	                   span<const TclObject> lists);
	void exprt(DriveSettings& driveData, std::string_view dirname,
	           span<const TclObject> lists);

	Reactor& reactor;
};

} // namespace openmsx

#endif
