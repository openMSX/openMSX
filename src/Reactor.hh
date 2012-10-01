// $Id$

#ifndef REACTOR_HH
#define REACTOR_HH

#include "Observer.hh"
#include "EventListener.hh"
#include "Semaphore.hh"
#include "noncopyable.hh"
#include "shared_ptr.hh"
#include <string>
#include <memory>
#include <set>
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

	static void getHwConfigs(const std::string& type,
	                         std::set<std::string>& result);

	void block();
	void unblock();

	// convenience methods
	GlobalSettings& getGlobalSettings();
	InfoCommand& getOpenMSXInfoCommand();
	CommandController& getCommandController();
	CliComm& getCliComm();
	std::string getMachineID() const;

	typedef shared_ptr<MSXMotherBoard> Board;
	Board createEmptyMotherBoard();
	void replaceBoard(MSXMotherBoard& oldBoard, const Board& newBoard); // for reverse

private:
	typedef std::vector<Board> Boards;

	void createMachineSetting();
	void switchBoard(const Board& newBoard);
	void deleteBoard(Board board);
	Board getMachine(const std::string& machineID) const;
	void getMachineIDs(std::set<std::string>& result) const;

	// Observer<Setting>
	virtual void update(const Setting& setting);

	// EventListener
	virtual int signalEvent(const shared_ptr<const Event>& event);

	void unpause();
	void pause();

	Semaphore mbSem; // this should come first, because it's still used by
	                 // the destructors of the auto_ptr below

	// note: order of auto_ptr's is important
	std::auto_ptr<EventDistributor> eventDistributor;
	std::auto_ptr<GlobalCliComm> globalCliComm;
	std::auto_ptr<GlobalCommandController> globalCommandController;
	std::auto_ptr<GlobalSettings> globalSettings;
	std::auto_ptr<InputEventGenerator> inputEventGenerator;
	std::auto_ptr<Display> display;
	std::auto_ptr<Mixer> mixer;
	std::auto_ptr<DiskFactory> diskFactory;
	std::auto_ptr<DiskManipulator> diskManipulator;
	std::auto_ptr<DiskChanger> virtualDrive;
	std::auto_ptr<FilePool> filePool;

	std::auto_ptr<EnumSetting<int> > machineSetting;
	std::auto_ptr<UserSettings> userSettings;
	std::auto_ptr<RomDatabase> softwareDatabase;

	std::auto_ptr<AfterCommand> afterCommand;
	std::auto_ptr<QuitCommand> quitCommand;
	std::auto_ptr<MessageCommand> messageCommand;
	std::auto_ptr<MachineCommand> machineCommand;
	std::auto_ptr<TestMachineCommand> testMachineCommand;
	std::auto_ptr<CreateMachineCommand> createMachineCommand;
	std::auto_ptr<DeleteMachineCommand> deleteMachineCommand;
	std::auto_ptr<ListMachinesCommand> listMachinesCommand;
	std::auto_ptr<ActivateMachineCommand> activateMachineCommand;
	std::auto_ptr<StoreMachineCommand> storeMachineCommand;
	std::auto_ptr<RestoreMachineCommand> restoreMachineCommand;
	std::auto_ptr<AviRecorder> aviRecordCommand;
	std::auto_ptr<ConfigInfo> extensionInfo;
	std::auto_ptr<ConfigInfo> machineInfo;
	std::auto_ptr<RealTimeInfo> realTimeInfo;
	std::auto_ptr<TclCallbackMessages> tclCallbackMessages;

	// Locking rules for activeBoard access:
	//  - main thread can always access activeBoard without taking a lock
	//  - changing activeBoard handle can only be done in the main thread
	//    and needs to take the mbSem lock
	//  - non-main thread can only access activeBoard via specific
	//    member functions (atm only via enterMainLoop()), it needs to take
	//    the mbSem lock
	Boards boards;
	Boards garbageBoards;
	Board activeBoard; // either NULL or a board inside 'boards'

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
