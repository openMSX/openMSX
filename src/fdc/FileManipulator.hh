// $Id$

#ifndef FILEMANIPULATOR_HH
#define FILEMANIPULATOR_HH

#include "Command.hh"
#include <map>
#include <vector>
#include "SectorBasedDisk.hh"

namespace openmsx {

class DiskDrive;

class FileManipulator : public SimpleCommand
{
public: 
	static FileManipulator& instance();
	void registerDrive(DiskDrive* drive, const std::string& ImageName);
	void unregisterDrive(DiskDrive* drive, const std::string& ImageName);

private:
	FileManipulator();
	~FileManipulator();

	std::map<const std::string, DiskDrive*> diskimages;

	// Command interface
	virtual std::string execute(const std::vector<std::string>& tokens);
	virtual std::string help   (const std::vector<std::string>& tokens) const;
	virtual void tabCompletion(std::vector<std::string>& tokens) const;
};

} // namespace openmsx

#endif
