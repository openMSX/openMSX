// $Id$

#ifndef FILEMANIPULATOR_HH
#define FILEMANIPULATOR_HH

#include "Command.hh"
#include <vector>
#include <memory>

namespace openmsx {

class CommandController;
class DiskContainer;
class DiskChanger;
class SectorAccessibleDisk;
class MSXtar;
class MSXMotherBoard;

class DiskManipulator : public SimpleCommand
{
public:
	explicit DiskManipulator(CommandController& commandController);
	~DiskManipulator();

	void registerDrive(DiskContainer& drive, MSXMotherBoard* board);
	void unregisterDrive(DiskContainer& drive);

private:
	struct DriveSettings
	{
		DiskContainer* drive;
		std::string driveName; // includes machine prefix
		std::string workingDir[32];
		int partition; // 0 = whole disk / 1-31 = partition number
	};
	typedef std::vector<DriveSettings> Drives;
	Drives drives;
	std::auto_ptr<DiskChanger> virtualDrive;

	// Command interface
	virtual std::string execute(const std::vector<std::string>& tokens);
	virtual std::string help   (const std::vector<std::string>& tokens) const;
	virtual void tabCompletion(std::vector<std::string>& tokens) const;

	std::string getMachinePrefix() const;
	Drives::iterator findDriveSettings(DiskContainer& drive);
	Drives::iterator findDriveSettings(const std::string& name);
	DriveSettings& getDriveSettings(const std::string& diskname);
	std::auto_ptr<SectorAccessibleDisk> getPartition(
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
};

} // namespace openmsx

#endif
