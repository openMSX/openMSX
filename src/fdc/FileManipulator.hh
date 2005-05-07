// $Id$

#ifndef FILEMANIPULATOR_HH
#define FILEMANIPULATOR_HH

#include "Command.hh"
#include <map>
#include <memory>

namespace openmsx {

class DiskContainer;
class SectorAccessibleDisk;
class FileDriveCombo;
class MSXtar;

class FileManipulator : public SimpleCommand
{
public:
	static FileManipulator& instance();
	void registerDrive(DiskContainer& drive, const std::string& imageName);
	void unregisterDrive(DiskContainer& drive, const std::string& imageName);

private:
	FileManipulator();
	~FileManipulator();

	struct DriveSettings
	{
		int partition;
		std::string workingDir;
		DiskContainer* drive;
	};
	typedef std::map<const std::string, DriveSettings> DiskImages;
	DiskImages diskImages;

	std::auto_ptr<FileDriveCombo> imageFile;

	void unregisterImageFile();

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
	void usefile(const std::string& filename);
	void format(DriveSettings& driveData);
	void chdir(DriveSettings& driveData, const std::string& filename,
	           std::string& result);
	void mkdir(DriveSettings& driveData, const std::string& filename);
	void dir(DriveSettings& driveData, std::string& result);
	void import(DriveSettings& driveData, const std::string& filename);
	void exprt(DriveSettings& driveData, const std::string& dirname);
};

} // namespace openmsx

#endif
