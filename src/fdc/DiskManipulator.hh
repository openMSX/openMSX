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

	void registerDrive(DiskContainer& drive, const std::string& prefix);
	void unregisterDrive(DiskContainer& drive);

private:
	static const unsigned MAX_PARTITIONS = 31;
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
	std::string help   (const std::vector<std::string>& tokens) const override;
	void tabCompletion(std::vector<std::string>& tokens) const override;

	std::string getMachinePrefix() const;
	Drives::iterator findDriveSettings(DiskContainer& drive);
	Drives::iterator findDriveSettings(std::string_view name);
	DriveSettings& getDriveSettings(std::string_view diskname);
	std::unique_ptr<DiskPartition> getPartition(
		const DriveSettings& driveData);
	std::unique_ptr<MSXtar> getMSXtar(SectorAccessibleDisk& disk,
	                                  DriveSettings& driveData);

	void create(span<const TclObject> tokens);
	void savedsk(const DriveSettings& driveData,
	             std::string_view filename);
	void format(DriveSettings& driveData, bool dos1);
	std::string chdir(DriveSettings& driveData, std::string_view filename);
	void mkdir(DriveSettings& driveData, std::string_view filename);
	std::string dir(DriveSettings& driveData);
	std::string import(DriveSettings& driveData,
	                   span<const TclObject> lists);
	void exprt(DriveSettings& driveData, std::string_view dirname,
	           span<const TclObject> lists);

	Reactor& reactor;
};

} // namespace openmsx

#endif
