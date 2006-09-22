// $Id$

#ifndef DISKCHANGER_HH
#define DISKCHANGER_HH

#include "DiskContainer.hh"
#include "MSXEventListener.hh"
#include <vector>
#include <string>
#include <memory>

namespace openmsx {

class CommandController;
class MSXEventDistributor;
class Scheduler;
class FileManipulator;
class Disk;
class DiskCommand;
class CliComm;
class GlobalSettings;
class TclObject;

class DiskChanger : public DiskContainer, private MSXEventListener
{
public:
	DiskChanger(const std::string& driveName,
	            CommandController& commandController,
	            FileManipulator& manipulator,
	            MSXEventDistributor* msxEventDistributor = NULL,
	            Scheduler* scheduler = NULL);
	~DiskChanger();

	const std::string& getDriveName() const;
	const std::string& getDiskName() const;
	bool diskChanged();
	bool peekDiskChanged() const;
	Disk& getDisk();

	// DiskContainer
	virtual SectorAccessibleDisk* getSectorAccessibleDisk();

private:
	void insertDisk(const std::vector<TclObject*>& args);
	void ejectDisk();
	void changeDisk(std::auto_ptr<Disk> newDisk);
	void sendChangeDiskEvent(const std::vector<std::string>& args);

	// MSXEventListener
	virtual void signalEvent(shared_ptr<const Event> event,
	                         const EmuTime& time);

	std::string driveName;
	FileManipulator& manipulator;
	std::auto_ptr<Disk> disk;
	bool diskChangedFlag;

	friend class DiskCommand;
	const std::auto_ptr<DiskCommand> diskCommand;
	CliComm& cliComm;
	GlobalSettings& globalSettings;
	MSXEventDistributor* msxEventDistributor;
	Scheduler* scheduler;
};

} // namespace openmsx

#endif
