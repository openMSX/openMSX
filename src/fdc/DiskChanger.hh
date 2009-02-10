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
class MSXMotherBoard;
class DiskManipulator;
class Disk;
class DiskCommand;
class TclObject;
class Filename;

class DiskChanger : public DiskContainer, private MSXEventListener,
                    private noncopyable
{
public:
	DiskChanger(const std::string& driveName,
	            CommandController& commandController,
	            DiskManipulator& manipulator,
	            MSXMotherBoard* board);
	~DiskChanger();

	const std::string& getDriveName() const;
	const Filename& getDiskName() const;
	bool diskChanged();
	bool peekDiskChanged() const;
	Disk& getDisk();

	// for nowind
	void insertDisk(const std::string& filename);

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
	                         EmuTime::param time);

	CommandController& controller;
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
