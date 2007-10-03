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

class DiskManipulator : public SimpleCommand
{
public:
	explicit DiskManipulator(CommandController& commandController);
	~DiskManipulator();

	void registerDrive(DiskContainer& drive);
	void unregisterDrive(DiskContainer& drive);

private:
	struct DriveSettings
	{
		DiskContainer* drive;
		std::string workingDir[31];
		int partition;
	};
	typedef std::vector<DriveSettings> DiskImages;
	DiskImages diskImages;
	std::auto_ptr<DiskChanger> virtualDrive;

	// Command interface
	virtual std::string execute(const std::vector<std::string>& tokens);
	virtual std::string help   (const std::vector<std::string>& tokens) const;
	virtual void tabCompletion(std::vector<std::string>& tokens) const;

	DiskImages::iterator findDriveSettings(DiskContainer& drive);
	DiskImages::iterator findDriveSettings(const std::string& name);
	DriveSettings& getDriveSettings(const std::string& diskname);
	SectorAccessibleDisk& getDisk(const DriveSettings& driveData);
	void restoreCWD(MSXtar& workhorse, DriveSettings& driveData);

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
