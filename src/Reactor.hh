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
class GlobalCommandController;
class GlobalCliComm;
class CliComm;
class Display;
class Mixer;
class CommandConsole;
class InputEventGenerator;
class DiskManipulator;
class FilePool;
class UserSettings;
class BooleanSetting;
class MSXMotherBoard;
class Setting;
class CommandLineParser;
class QuitCommand;
class MachineCommand;
class TestMachineCommand;
class CreateMachineCommand;
class DeleteMachineCommand;
class ListMachinesCommand;
class ActivateMachineCommand;
class SaveMachineCommand;
class LoadMachineCommand;
class SaveMachineMemCommand;
class LoadMachineMemCommand;
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
	~Reactor();

	/**
	 * Main loop.
	 */
	void run(CommandLineParser& parser);

	void enterMainLoop();

	EventDistributor& getEventDistributor();
	GlobalCommandController& getGlobalCommandController();
	GlobalCliComm& getGlobalCliComm();
	InputEventGenerator& getInputEventGenerator();
	Display& getDisplay();
	Mixer& getMixer();
	CommandConsole& getCommandConsole();
	DiskManipulator& getDiskManipulator();
	FilePool& getFilePool();
	EnumSetting<int>& getMachineSetting();

	void switchMachine(const std::string& machine);
	MSXMotherBoard* getMotherBoard() const;

	static void getHwConfigs(const std::string& type,
	                         std::set<std::string>& result);

	// convenience methods
	GlobalSettings& getGlobalSettings();
	InfoCommand& getOpenMSXInfoCommand();
	CommandController& getCommandController();
	CliComm& getCliComm();

private:
	typedef shared_ptr<MSXMotherBoard> Board;
	typedef std::vector<Board> Boards;

	void createMachineSetting();
	void switchBoard(Board newBoard);
	void deleteBoard(Board board);
	Board getMachine(const std::string& machineID) const;
	std::string getMachineID() const;
	void getMachineIDs(std::set<std::string>& result) const;

	// Observer<Setting>
	virtual void update(const Setting& setting);

	// EventListener
	virtual bool signalEvent(shared_ptr<const Event> event);

	void block();
	void unblock();

	void unpause();
	void pause();

	// note: order of auto_ptr's is important
	std::auto_ptr<EventDistributor> eventDistributor;
	std::auto_ptr<GlobalCommandController> globalCommandController;
	std::auto_ptr<GlobalCliComm> globalCliComm;
	std::auto_ptr<InputEventGenerator> inputEventGenerator;
	std::auto_ptr<Display> display;
	std::auto_ptr<Mixer> mixer;
	std::auto_ptr<CommandConsole> commandConsole;
	std::auto_ptr<DiskManipulator> diskManipulator;
	std::auto_ptr<FilePool> filePool;

	BooleanSetting& pauseSetting;
	std::auto_ptr<EnumSetting<int> > machineSetting;

	const std::auto_ptr<UserSettings> userSettings;
	const std::auto_ptr<QuitCommand> quitCommand;
	const std::auto_ptr<MachineCommand> machineCommand;
	const std::auto_ptr<TestMachineCommand> testMachineCommand;
	const std::auto_ptr<CreateMachineCommand> createMachineCommand;
	const std::auto_ptr<DeleteMachineCommand> deleteMachineCommand;
	const std::auto_ptr<ListMachinesCommand> listMachinesCommand;
	const std::auto_ptr<ActivateMachineCommand> activateMachineCommand;
	const std::auto_ptr<SaveMachineCommand> saveMachineCommand;
	const std::auto_ptr<LoadMachineCommand> loadMachineCommand;
	const std::auto_ptr<SaveMachineMemCommand> saveMachineMemCommand;
	const std::auto_ptr<LoadMachineMemCommand> loadMachineMemCommand;
	const std::auto_ptr<AviRecorder> aviRecordCommand;
	const std::auto_ptr<ConfigInfo> extensionInfo;
	const std::auto_ptr<ConfigInfo> machineInfo;
	const std::auto_ptr<RealTimeInfo> realTimeInfo;

	std::vector<char> snapshot; // TODO temp code

	// Locking rules for activeBoard access:
	//  - main thread can always access activeBoard without taking a lock
	//  - changing activeBoard handle can only be done in the main thread
	//    and needs to take the mbSem lock
	//  - non-main thread can only access activeBoard via specific
	//    member functions (atm only via enterMainLoop()), it needs to take
	//    the mbSem lock
	Semaphore mbSem;
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

	friend class QuitCommand;
	friend class MachineCommand;
	friend class CreateMachineCommand;
	friend class DeleteMachineCommand;
	friend class ListMachinesCommand;
	friend class ActivateMachineCommand;
	friend class SaveMachineCommand;
	friend class LoadMachineCommand;
	friend class SaveMachineMemCommand;
	friend class LoadMachineMemCommand;
};

} // namespace openmsx

#endif // REACTOR_HH
