// $Id$

#ifndef FILEMANIPULATOR_HH
#define FILEMANIPULATOR_HH

#include "Command.hh"
#include <map>

namespace openmsx {

class DiskDrive;

class FileManipulator : public SimpleCommand
{
public: 
	static FileManipulator& instance();
	void registerDrive(DiskDrive& drive, const std::string& imageName);
	void unregisterDrive(DiskDrive& drive, const std::string& imageName);

private:
	FileManipulator();
	~FileManipulator();

	// Command interface
	virtual std::string execute(const std::vector<std::string>& tokens);
	virtual std::string help   (const std::vector<std::string>& tokens) const;
	virtual void tabCompletion(std::vector<std::string>& tokens) const;
	
	std::map<const std::string, DiskDrive*> diskimages;
};

} // namespace openmsx

#endif
