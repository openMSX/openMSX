#ifndef DISKCHANGER_HH
#define DISKCHANGER_HH

#include "DiskContainer.hh"
#include "StateChangeListener.hh"
#include "serialize_meta.hh"
#include "span.hh"
#include <functional>
#include <memory>
#include <string>

namespace openmsx {

class CommandController;
class StateChangeDistributor;
class Scheduler;
class MSXMotherBoard;
class Reactor;
class Disk;
class DiskCommand;
class TclObject;
class DiskName;

class DiskChanger final : public DiskContainer, private StateChangeListener
{
public:
	DiskChanger(MSXMotherBoard& board,
	            std::string driveName,
	            bool createCmd = true,
	            bool doubleSidedDrive = true,
	            std::function<void()> preChangeCallback = {});
	DiskChanger(Reactor& reactor,
	            std::string driveName); // for virtual_drive
	~DiskChanger() override;

	void createCommand();

	const std::string& getDriveName() const { return driveName; }
	const DiskName& getDiskName() const;
	bool peekDiskChanged() const { return diskChangedFlag; }
	void forceDiskChange() { diskChangedFlag = true; }
	Disk& getDisk() { return *disk; }

	// DiskContainer
	SectorAccessibleDisk* getSectorAccessibleDisk() override;
	const std::string& getContainerName() const override;
	bool diskChanged() override;
	int insertDisk(std::string_view filename) override;

	// for NowindCommand
	void changeDisk(std::unique_ptr<Disk> newDisk);

	// for DirAsDSK
	Scheduler* getScheduler() const { return scheduler; }
	bool isDoubleSidedDrive() const { return doubleSidedDrive; }

	template<typename Archive>
	void serialize(Archive& ar, unsigned version);

private:
	void init(const std::string& prefix, bool createCmd);
	void insertDisk(span<const TclObject> args);
	void ejectDisk();
	void sendChangeDiskEvent(span<std::string> args);

	// StateChangeListener
	void signalStateChange(const std::shared_ptr<StateChange>& event) override;
	void stopReplay(EmuTime::param time) override;

	Reactor& reactor;
	CommandController& controller;
	StateChangeDistributor* stateChangeDistributor;
	Scheduler* scheduler;
	std::function<void()> preChangeCallback;

	const std::string driveName;
	std::unique_ptr<Disk> disk;

	friend class DiskCommand;
	std::unique_ptr<DiskCommand> diskCommand; // must come after driveName
	const bool doubleSidedDrive; // for DirAsDSK

	bool diskChangedFlag;
};
SERIALIZE_CLASS_VERSION(DiskChanger, 2);

} // namespace openmsx

#endif
