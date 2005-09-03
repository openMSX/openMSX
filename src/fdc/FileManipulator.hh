// $Id$

#ifndef FILEMANIPULATOR_HH
#define FILEMANIPULATOR_HH

#include "Command.hh"
#include <map>
#include <memory>

namespace openmsx {

class CommandController;
class GlobalSettings;
class DiskContainer;
class DiskChanger;
class SectorAccessibleDisk;
class MSXtar;

class FileManipulator : public SimpleCommand
{
public:
	FileManipulator(CommandController& commandController);
	~FileManipulator();

	void registerDrive(DiskContainer& drive, const std::string& imageName);
	void unregisterDrive(DiskContainer& drive, const std::string& imageName);

private:
	struct DriveSettings
	{
		int partition;
		std::string workingDir[31];
		DiskContainer* drive;
	};
	typedef std::map<const std::string, DriveSettings> DiskImages;
	DiskImages diskImages;
	std::auto_ptr<DiskChanger> virtualDrive;

	// Command interface
	virtual std::string execute(const std::vector<std::string>& tokens);
	virtual std::string help   (const std::vector<std::string>& tokens) const;
	virtual void tabCompletion(std::vector<std::string>& tokens) const;

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
	                   const std::vector<std::string>& filenames);
	void exprt(DriveSettings& driveData, const std::string& dirname);
};

} // namespace openmsx

#endif
