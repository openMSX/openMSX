// $Id$

#ifndef FILEMANIPULATOR_HH
#define FILEMANIPULATOR_HH

#include "Command.hh"
#include "string_ref.hh"
#include <vector>
#include <memory>

namespace openmsx {

class CommandController;
class DiskContainer;
class SectorAccessibleDisk;
class DiskPartition;
class MSXtar;
class Reactor;

class DiskManipulator : public Command
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
	typedef std::vector<DriveSettings> Drives;
	Drives drives;

	// Command interface
	virtual std::string execute(const std::vector<std::string>& tokens);
	virtual std::string help   (const std::vector<std::string>& tokens) const;
	virtual void tabCompletion(std::vector<std::string>& tokens) const;

	std::string getMachinePrefix() const;
	Drives::iterator findDriveSettings(DiskContainer& drive);
	Drives::iterator findDriveSettings(string_ref name);
	DriveSettings& getDriveSettings(string_ref diskname);
	std::auto_ptr<DiskPartition> getPartition(
		const DriveSettings& driveData);
	std::auto_ptr<MSXtar> getMSXtar(SectorAccessibleDisk& disk,
	                                DriveSettings& driveData);

	void create(const std::vector<std::string>& tokens);
	void savedsk(const DriveSettings& driveData,
	             const std::string& filename);
	void format(DriveSettings& driveData);
	void chdir(DriveSettings& driveData, const std::string& filename,
	           std::string& result);
	void mkdir(DriveSettings& driveData, const std::string& filename);
	void dir(DriveSettings& driveData, std::string& result);
	std::string import(DriveSettings& driveData,
	                   const std::vector<std::string>& lists);
	void exprt(DriveSettings& driveData, const std::string& dirname,
	           const std::vector<std::string>& lists);

	Reactor& reactor;
};

} // namespace openmsx

#endif
