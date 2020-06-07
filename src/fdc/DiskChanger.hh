#ifndef DISKCHANGER_HH
#define DISKCHANGER_HH

#include "DiskContainer.hh"
#include "StateChangeListener.hh"
#include "RecordedCommand.hh"
#include "serialize_meta.hh"
#include <functional>
#include <memory>
#include <optional>
#include <span>
#include <string>

namespace openmsx {

class CommandController;
class StateChangeDistributor;
class Scheduler;
class MSXMotherBoard;
class Reactor;
class Disk;
class DiskChanger;
class TclObject;
class DiskName;

class DiskCommand final : public Command // TODO RecordedCommand
{
public:
	DiskCommand(CommandController& commandController,
	            DiskChanger& diskChanger);
	void execute(std::span<const TclObject> tokens,
	             TclObject& result) override;
	[[nodiscard]] std::string help(std::span<const TclObject> tokens) const override;
	void tabCompletion(std::vector<std::string>& tokens) const override;
	[[nodiscard]] bool needRecord(std::span<const TclObject> tokens) const /*override*/;
private:
	DiskChanger& diskChanger;
};

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

	[[nodiscard]] const std::string& getDriveName() const { return driveName; }
	[[nodiscard]] const DiskName& getDiskName() const;
	[[nodiscard]] bool peekDiskChanged() const { return diskChangedFlag; }
	void forceDiskChange() { diskChangedFlag = true; }
	[[nodiscard]]       Disk& getDisk()       { return *disk; }
	[[nodiscard]] const Disk& getDisk() const { return *disk; }

	// DiskContainer
	[[nodiscard]] SectorAccessibleDisk* getSectorAccessibleDisk() override;
	[[nodiscard]] std::string_view getContainerName() const override;
	bool diskChanged() override;
	int insertDisk(const std::string& filename) override;

	// for NowindCommand
	void changeDisk(std::unique_ptr<Disk> newDisk);

	// for DirAsDSK
	[[nodiscard]] Scheduler* getScheduler() const { return scheduler; }
	[[nodiscard]] bool isDoubleSidedDrive() const { return doubleSidedDrive; }

	template<typename Archive>
	void serialize(Archive& ar, unsigned version);

private:
	void init(std::string_view prefix, bool createCmd);
	void execute(std::span<const TclObject> tokens);
	void insertDisk(std::span<const TclObject> args);
	void ejectDisk();
	void sendChangeDiskEvent(std::span<const TclObject> args);

	// StateChangeListener
	void signalStateChange(const StateChange& event) override;
	void stopReplay(EmuTime::param time) noexcept override;

private:
	Reactor& reactor;
	CommandController& controller;
	StateChangeDistributor* stateChangeDistributor;
	Scheduler* scheduler;
	std::function<void()> preChangeCallback;

	const std::string driveName;
	std::unique_ptr<Disk> disk;

	friend class DiskCommand;
	std::optional<DiskCommand> diskCommand; // must come after driveName
	const bool doubleSidedDrive; // for DirAsDSK

	bool diskChangedFlag;
};
SERIALIZE_CLASS_VERSION(DiskChanger, 2);

} // namespace openmsx

#endif
