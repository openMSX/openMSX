// $Id$

#ifndef FILEMANIPULATOR_HH
#define FILEMANIPULATOR_HH

#include "Command.hh"
#include <map>

namespace openmsx {

class RealDrive;

class FileManipulator : public SimpleCommand
{
public:
	static FileManipulator& instance();
	void registerDrive(RealDrive& drive, const std::string& imageName);
	void unregisterDrive(RealDrive& drive, const std::string& imageName);

private:
	FileManipulator();
	~FileManipulator();

	// Command interface
	virtual std::string execute(const std::vector<std::string>& tokens);
	virtual std::string help   (const std::vector<std::string>& tokens) const;
	virtual void tabCompletion(std::vector<std::string>& tokens) const;

	void savedsk(RealDrive* drive, const std::string& filename);
	void addDir(RealDrive* drive, const std::string& filename);
	
	typedef std::map<const std::string, RealDrive*> DiskImages;
	DiskImages diskImages;
};

} // namespace openmsx

#endif
