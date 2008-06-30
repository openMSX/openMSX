// $Id$

#ifndef DISKCHANGER_HH
#define DISKCHANGER_HH

#include "DiskContainer.hh"
#include "MSXEventListener.hh"
#include "noncopyable.hh"
#include <vector>
#include <string>
#include <memory>

namespace openmsx {

class CommandController;
class MSXEventDistributor;
class Scheduler;
class DiskManipulator;
class Disk;
class DiskCommand;
class CliComm;
class GlobalSettings;
class TclObject;

class DiskChanger : public DiskContainer, private MSXEventListener,
                    private noncopyable
{
public:
	DiskChanger(const std::string& driveName,
	            CommandController& commandController,
	            DiskManipulator& manipulator,
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
	virtual const std::string& getContainerName() const;

	template<typename Archive>
	void serialize(Archive& ar, unsigned version);

private:
	void insertDisk(const std::vector<TclObject*>& args);
	void ejectDisk();
	void changeDisk(std::auto_ptr<Disk> newDisk);
	void sendChangeDiskEvent(const std::vector<std::string>& args);

	// MSXEventListener
	virtual void signalEvent(shared_ptr<const Event> event,
	                         const EmuTime& time);

	CliComm& cliComm;
	GlobalSettings& globalSettings;
	MSXEventDistributor* msxEventDistributor;
	Scheduler* scheduler;
	DiskManipulator& manipulator;

	const std::string driveName;
	std::auto_ptr<Disk> disk;

	friend class DiskCommand;
	const std::auto_ptr<DiskCommand> diskCommand; // must come after driveName

	bool diskChangedFlag;
};

} // namespace openmsx

#endif
