// $Id$

#ifndef FILEMANIPULATOR_HH
#define FILEMANIPULATOR_HH

#include "Command.hh"
#include "SectorAccessibleDisk.hh"
#include "Command.hh"
#include <map>

namespace openmsx {

class DiskContainer;
class Disk;
class DiskContainer;

class FileManipulator : public SimpleCommand
{
public:
	static FileManipulator& instance();
	void registerDrive(DiskContainer& drive, const std::string& imageName);
	void unregisterDrive(DiskContainer& drive, const std::string& imageName);

private:
	FileManipulator();
	~FileManipulator();


	// Command interface
	virtual std::string execute(const std::vector<std::string>& tokens);
	virtual std::string help   (const std::vector<std::string>& tokens) const;
	virtual void tabCompletion(std::vector<std::string>& tokens) const;

	struct DriveSettings
	{
		int partition;
		std::string workingDir;
		DiskContainer* drive;
	};
	typedef std::map<const std::string, DriveSettings*> DiskImages;
	DiskImages diskImages;

	void create(const std::vector<std::string>& tokens);
	void savedsk(DriveSettings* driveData, const std::string& filename);
	void usefile(const std::string& filename);
	void format(DriveSettings* driveData );
	void import(DriveSettings* driveData, const std::string& filename);
	void exprt(DriveSettings* driveData, const std::string& dirname); //using export as name is not feasable (reserved word)


};

} // namespace openmsx

#endif
