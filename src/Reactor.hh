// $Id$

#ifndef REACTOR_HH
#define REACTOR_HH

#include "Observer.hh"
#include "EventListener.hh"
#include "Semaphore.hh"
#include "noncopyable.hh"
#include "string_ref.hh"
#include <string>
#include <memory>
#include <vector>

namespace openmsx {

class EventDistributor;
class CommandController;
class InfoCommand;
class GlobalCliComm;
class GlobalCommandController;
class GlobalSettings;
class CliComm;
class Display;
class Mixer;
class InputEventGenerator;
class DiskFactory;
class DiskManipulator;
class DiskChanger;
class FilePool;
class UserSettings;
class RomDatabase;
class TclCallbackMessages;
class BooleanSetting;
class MSXMotherBoard;
class Setting;
class CommandLineParser;
class AfterCommand;
class QuitCommand;
class MessageCommand;
class MachineCommand;
class TestMachineCommand;
class CreateMachineCommand;
class DeleteMachineCommand;
class ListMachinesCommand;
class ActivateMachineCommand;
class StoreMachineCommand;
class RestoreMachineCommand;
class AviRecorder;
class ConfigInfo;
class RealTimeInfo;
class GlobalSettings;
class PollEventGenerator;
template <typename T> class EnumSetting;

/**
 * Contains the main loop of openMSX.
 * openMSX is almost single threaded: the main thread does most of the work,
 * we create additional threads only if we need blocking calls for
 * communicating with peripherals.
 * This class serializes all incoming requests so they can be handled by the
 * main thread.
 */
class Reactor : private Observer<Setting>, private EventListener,
                private noncopyable
{
public:
	Reactor();
	void init();
	~Reactor();

	/**
	 * Main loop.
	 */
	void run(CommandLineParser& parser);

	void enterMainLoop();
	void pollNow();

	EventDistributor& getEventDistributor();
	GlobalCliComm& getGlobalCliComm();
	GlobalCommandController& getGlobalCommandController();
	InputEventGenerator& getInputEventGenerator();
	Display& getDisplay();
	Mixer& getMixer();
	DiskFactory& getDiskFactory();
	DiskManipulator& getDiskManipulator();
	EnumSetting<int>& getMachineSetting();
	RomDatabase& getSoftwareDatabase();
	FilePool& getFilePool();

	void switchMachine(const std::string& machine);
	MSXMotherBoard* getMotherBoard() const;

	static std::vector<std::string> getHwConfigs(string_ref type);

	void block();
	void unblock();

	// convenience methods
	GlobalSettings& getGlobalSettings();
	InfoCommand& getOpenMSXInfoCommand();
	CommandController& getCommandController();
	CliComm& getCliComm();
	std::string getMachineID() const;

	typedef std::unique_ptr<MSXMotherBoard> Board;
	Board createEmptyMotherBoard();
	void replaceBoard(MSXMotherBoard& oldBoard, Board newBoard); // for reverse

private:
	typedef std::vector<Board> Boards;

	void createMachineSetting();
	void switchBoard(MSXMotherBoard* newBoard);
	void deleteBoard(MSXMotherBoard* board);
	MSXMotherBoard& getMachine(const std::string& machineID) const;
	std::vector<string_ref> getMachineIDs() const;

	// Observer<Setting>
	virtual void update(const Setting& setting);

	// EventListener
	virtual int signalEvent(const std::shared_ptr<const Event>& event);

	void unpause();
	void pause();

	Semaphore mbSem; // this should come first, because it's still used by
	                 // the destructors of the unique_ptr below

	// note: order of unique_ptr's is important
	std::unique_ptr<EventDistributor> eventDistributor;
	std::unique_ptr<GlobalCliComm> globalCliComm;
	std::unique_ptr<GlobalCommandController> globalCommandController;
	std::unique_ptr<GlobalSettings> globalSettings;
	std::unique_ptr<InputEventGenerator> inputEventGenerator;
	std::unique_ptr<Display> display;
	std::unique_ptr<Mixer> mixer;
	std::unique_ptr<DiskFactory> diskFactory;
	std::unique_ptr<DiskManipulator> diskManipulator;
	std::unique_ptr<DiskChanger> virtualDrive;
	std::unique_ptr<FilePool> filePool;

	std::unique_ptr<EnumSetting<int>> machineSetting;
	std::unique_ptr<UserSettings> userSettings;
	std::unique_ptr<RomDatabase> softwareDatabase;

	std::unique_ptr<AfterCommand> afterCommand;
	std::unique_ptr<QuitCommand> quitCommand;
	std::unique_ptr<MessageCommand> messageCommand;
	std::unique_ptr<MachineCommand> machineCommand;
	std::unique_ptr<TestMachineCommand> testMachineCommand;
	std::unique_ptr<CreateMachineCommand> createMachineCommand;
	std::unique_ptr<DeleteMachineCommand> deleteMachineCommand;
	std::unique_ptr<ListMachinesCommand> listMachinesCommand;
	std::unique_ptr<ActivateMachineCommand> activateMachineCommand;
	std::unique_ptr<StoreMachineCommand> storeMachineCommand;
	std::unique_ptr<RestoreMachineCommand> restoreMachineCommand;
	std::unique_ptr<AviRecorder> aviRecordCommand;
	std::unique_ptr<ConfigInfo> extensionInfo;
	std::unique_ptr<ConfigInfo> machineInfo;
	std::unique_ptr<RealTimeInfo> realTimeInfo;
	std::unique_ptr<TclCallbackMessages> tclCallbackMessages;
	std::unique_ptr<PollEventGenerator> pollEventGenerator;

	// Locking rules for activeBoard access:
	//  - main thread can always access activeBoard without taking a lock
	//  - changing activeBoard handle can only be done in the main thread
	//    and needs to take the mbSem lock
	//  - non-main thread can only access activeBoard via specific
	//    member functions (atm only via enterMainLoop()), it needs to take
	//    the mbSem lock
	Boards boards;
	Boards garbageBoards;
	MSXMotherBoard* activeBoard; // either nullptr or a board inside 'boards'

	int blockedCounter;
	bool paused;

	/**
	 * True iff the Reactor should keep running.
	 * When this is set to false, the Reactor will end the main loop after
	 * finishing the pending request(s).
	 */
	bool running;

	bool isInit; // has the init() method been run successfully

	friend class MachineCommand;
	friend class TestMachineCommand;
	friend class CreateMachineCommand;
	friend class DeleteMachineCommand;
	friend class ListMachinesCommand;
	friend class ActivateMachineCommand;
	friend class StoreMachineCommand;
	friend class RestoreMachineCommand;
};

} // namespace openmsx

#endif // REACTOR_HH
