// $Id$

#ifndef FILEMANIPULATOR_HH
#define FILEMANIPULATOR_HH

#include "Command.hh"
//#include "DiskContainer.hh"
#include "SectorAccessibleDisk.hh"
#include "Command.hh"
#include <map>

namespace openmsx {

class DiskContainer;
class Disk;
class DiskContainer;

class FileManipulator : public SimpleCommand, public SectorAccessibleDisk //, public DiskContainer
{
public:
	static FileManipulator& instance();
	void registerDrive(DiskContainer& drive, const std::string& imageName);
	void unregisterDrive(DiskContainer& drive, const std::string& imageName);

private:
	FileManipulator();
	~FileManipulator();

	static int partition;

	// Command interface
	virtual std::string execute(const std::vector<std::string>& tokens);
	virtual std::string help   (const std::vector<std::string>& tokens) const;
	virtual void tabCompletion(std::vector<std::string>& tokens) const;

	// SectorAccessibleDisk
	 void readLogicalSector(unsigned sector, byte* buf);
	 void writeLogicalSector(unsigned sector, const byte* buf);
	 unsigned getNbSectors() const;
	// DiskContainer
	// Disk& getDisk() const;
	
	void create(const std::vector<std::string>& tokens);
	void savedsk(DiskContainer* drive, const std::string& filename);
	void addDir(DiskContainer* drive, const std::string& filename);
	void getDir(DiskContainer* drive, const std::string& dirname);

	typedef std::map<const std::string, DiskContainer*> DiskImages;
	DiskImages diskImages;
};

} // namespace openmsx

#endif
