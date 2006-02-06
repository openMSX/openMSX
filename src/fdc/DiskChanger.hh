// $Id$

#ifndef DISKCHANGER_HH
#define DISKCHANGER_HH

#include "DiskContainer.hh"
#include <vector>
#include <string>
#include <memory>

namespace openmsx {

class CommandController;
class FileManipulator;
class Disk;
class DiskCommand;
class CliComm;

class DiskChanger : public DiskContainer
{
public:
	DiskChanger(const std::string& driveName,
	            CommandController& commandController,
	            FileManipulator& manipulator);
	~DiskChanger();

	const std::string& getDriveName() const;
	const std::string& getDiskName() const;
	bool diskChanged();
	bool peekDiskChanged() const;
	Disk& getDisk();

	// DiskContainer
	virtual SectorAccessibleDisk* getSectorAccessibleDisk();

private:
	void insertDisk(const std::string& disk,
	                const std::vector<std::string>& patches);
	void ejectDisk();

	std::string driveName;
	std::string diskName;
	FileManipulator& manipulator;
	std::auto_ptr<Disk> disk;
	bool diskChangedFlag;

	friend class DiskCommand;
	const std::auto_ptr<DiskCommand> diskCommand;
	CliComm& cliComm;
};

} // namespace openmsx

#endif
