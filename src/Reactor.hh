#ifndef REACTOR_HH
#define REACTOR_HH

#include "Observer.hh"
#include "EventListener.hh"
#include <cassert>
#include <memory>
#include <mutex>
#include <string>
#include <string_view>
#include <vector>

namespace openmsx {

class RTScheduler;
class EventDistributor;
class CommandController;
class InfoCommand;
class GlobalCliComm;
class GlobalCommandController;
class GlobalSettings;
class CliComm;
class Interpreter;
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
class MSXMotherBoard;
class Setting;
class CommandLineParser;
class AfterCommand;
class ExitCommand;
class MessageCommand;
class MachineCommand;
class TestMachineCommand;
class CreateMachineCommand;
class DeleteMachineCommand;
class ListMachinesCommand;
class ActivateMachineCommand;
class StoreMachineCommand;
class RestoreMachineCommand;
class GetClipboardCommand;
class SetClipboardCommand;
class AviRecorder;
class ConfigInfo;
class RealTimeInfo;
class SoftwareInfoTopic;
template <typename T> class EnumSetting;

extern int exitCode;

/**
 * Contains the main loop of openMSX.
 * openMSX is almost single threaded: the main thread does most of the work,
 * we create additional threads only if we need blocking calls for
 * communicating with peripherals.
 * This class serializes all incoming requests so they can be handled by the
 * main thread.
 */
class Reactor final : private Observer<Setting>, private EventListener
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

	RTScheduler& getRTScheduler() { return *rtScheduler; }
	EventDistributor& getEventDistributor() { return *eventDistributor; }
	GlobalCliComm& getGlobalCliComm() { return *globalCliComm; }
	GlobalCommandController& getGlobalCommandController() { return *globalCommandController; }
	InputEventGenerator& getInputEventGenerator() { return *inputEventGenerator; }
	Display& getDisplay() { assert(display); return *display; }
	Mixer& getMixer() { return *mixer; }
	DiskFactory& getDiskFactory() { return *diskFactory; }
	DiskManipulator& getDiskManipulator() { return *diskManipulator; }
	EnumSetting<int>& getMachineSetting() { return *machineSetting; }
	FilePool& getFilePool() { return *filePool; }

	RomDatabase& getSoftwareDatabase();

	void switchMachine(const std::string& machine);
	MSXMotherBoard* getMotherBoard() const;

	static std::vector<std::string> getHwConfigs(std::string_view type);

	void block();
	void unblock();

	// convenience methods
	GlobalSettings& getGlobalSettings() { return *globalSettings; }
	InfoCommand& getOpenMSXInfoCommand();
	CommandController& getCommandController();
	CliComm& getCliComm();
	Interpreter& getInterpreter();
	std::string_view getMachineID() const;

	using Board = std::unique_ptr<MSXMotherBoard>;
	Board createEmptyMotherBoard();
	void replaceBoard(MSXMotherBoard& oldBoard, Board newBoard); // for reverse

private:
	using Boards = std::vector<Board>;

	void createMachineSetting();
	void switchBoard(MSXMotherBoard* newBoard);
	void deleteBoard(MSXMotherBoard* board);
	MSXMotherBoard& getMachine(std::string_view machineID) const;
	std::vector<std::string_view> getMachineIDs() const;

	// Observer<Setting>
	void update(const Setting& setting) override;

	// EventListener
	int signalEvent(const std::shared_ptr<const Event>& event) override;

	void unpause();
	void pause();

	std::mutex mbMutex; // this should come first, because it's still used by
	                    // the destructors of the unique_ptr below

	// note: order of unique_ptr's is important
	std::unique_ptr<RTScheduler> rtScheduler;
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
	std::unique_ptr<ExitCommand> exitCommand;
	std::unique_ptr<MessageCommand> messageCommand;
	std::unique_ptr<MachineCommand> machineCommand;
	std::unique_ptr<TestMachineCommand> testMachineCommand;
	std::unique_ptr<CreateMachineCommand> createMachineCommand;
	std::unique_ptr<DeleteMachineCommand> deleteMachineCommand;
	std::unique_ptr<ListMachinesCommand> listMachinesCommand;
	std::unique_ptr<ActivateMachineCommand> activateMachineCommand;
	std::unique_ptr<StoreMachineCommand> storeMachineCommand;
	std::unique_ptr<RestoreMachineCommand> restoreMachineCommand;
	std::unique_ptr<GetClipboardCommand> getClipboardCommand;
	std::unique_ptr<SetClipboardCommand> setClipboardCommand;
	std::unique_ptr<AviRecorder> aviRecordCommand;
	std::unique_ptr<ConfigInfo> extensionInfo;
	std::unique_ptr<ConfigInfo> machineInfo;
	std::unique_ptr<RealTimeInfo> realTimeInfo;
	std::unique_ptr<SoftwareInfoTopic> softwareInfoTopic;
	std::unique_ptr<TclCallbackMessages> tclCallbackMessages;

	// Locking rules for activeBoard access:
	//  - main thread can always access activeBoard without taking a lock
	//  - changing activeBoard handle can only be done in the main thread
	//    and needs to take the mbMutex lock
	//  - non-main thread can only access activeBoard via specific
	//    member functions (atm only via enterMainLoop()), it needs to take
	//    the mbMutex lock
	Boards boards; // unordered
	Boards garbageBoards;
	MSXMotherBoard* activeBoard = nullptr; // either nullptr or a board inside 'boards'

	int blockedCounter = 0;
	bool paused = false;

	/**
	 * True iff the Reactor should keep running.
	 * When this is set to false, the Reactor will end the main loop after
	 * finishing the pending request(s).
	 */
	bool running = true;

	bool isInit = false; // has the init() method been run successfully

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
