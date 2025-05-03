#ifndef REACTOR_HH
#define REACTOR_HH

#include "EnumSetting.hh"
#include "EventListener.hh"
#include "Observer.hh"

#include <cassert>
#include <memory>
#include <mutex>
#include <ranges>
#include <string>
#include <string_view>
#include <vector>

namespace openmsx {

class ActivateMachineCommand;
class AfterCommand;
class AviRecorder;
class CliComm;
class CommandController;
class CommandLineParser;
class ConfigInfo;
class CreateMachineCommand;
class DeleteMachineCommand;
class DiskChanger;
class DiskFactory;
class DiskManipulator;
class Display;
class EventDistributor;
class ExitCommand;
class FilePool;
class GetClipboardCommand;
class GlobalCliComm;
class GlobalCommandController;
class GlobalSettings;
class HotKey;
class ImGuiManager;
class InfoCommand;
class InputEventGenerator;
class Interpreter;
class ListMachinesCommand;
class SetupCommand;
class MSXMotherBoard;
class MachineCommand;
class MessageCommand;
class Mixer;
class MsxChar2Unicode;
class RTScheduler;
class RealTimeInfo;
class RestoreMachineCommand;
class RomDatabase;
class SetClipboardCommand;
class Setting;
class Shortcuts;
class SoftwareInfoTopic;
class StoreMachineCommand;
class SymbolManager;
class TclCallbackMessages;
class TestMachineCommand;
class UserSettings;

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

	void runStartupScripts(const CommandLineParser& parser);
	void powerOn();
	void run();

	void enterMainLoop();

	[[nodiscard]] Shortcuts& getShortcuts() { return *shortcuts; }
	[[nodiscard]] RTScheduler& getRTScheduler() { return *rtScheduler; }
	[[nodiscard]] EventDistributor& getEventDistributor() { return *eventDistributor; }
	[[nodiscard]] GlobalCliComm& getGlobalCliComm() { return *globalCliComm; }
	[[nodiscard]] GlobalCommandController& getGlobalCommandController() { return *globalCommandController; }
	[[nodiscard]] InputEventGenerator& getInputEventGenerator() { return *inputEventGenerator; }
	[[nodiscard]] Display& getDisplay() { assert(display); return *display; }
	[[nodiscard]] Mixer& getMixer();
	[[nodiscard]] DiskFactory& getDiskFactory() { return *diskFactory; }
	[[nodiscard]] DiskManipulator& getDiskManipulator() { return *diskManipulator; }
	[[nodiscard]] EnumSetting<int>& getDefaultMachineSetting() { return *defaultMachineSetting; }
	[[nodiscard]] FilePool& getFilePool() { return *filePool; }
	[[nodiscard]] ImGuiManager& getImGuiManager() { return *imGuiManager; }
	[[nodiscard]] const HotKey& getHotKey() const;
	[[nodiscard]] SymbolManager& getSymbolManager() const { return *symbolManager; }
	[[nodiscard]] AviRecorder& getRecorder() const { return *aviRecordCommand; }

	[[nodiscard]] RomDatabase& getSoftwareDatabase();

	void switchMachine(const std::string& machine);
	void switchMachineFromSetup(const std::string& filename);
	[[nodiscard]] MSXMotherBoard* getMotherBoard() const;

	[[nodiscard]] static std::vector<std::string> getHwConfigs(std::string_view type);

	[[nodiscard]] const MsxChar2Unicode& getMsxChar2Unicode() const;

	void block();
	void unblock();

	// convenience methods
	[[nodiscard]] GlobalSettings& getGlobalSettings() { return *globalSettings; }
	[[nodiscard]] InfoCommand& getOpenMSXInfoCommand();
	[[nodiscard]] CommandController& getCommandController();
	[[nodiscard]] CliComm& getCliComm();
	[[nodiscard]] Interpreter& getInterpreter();
	[[nodiscard]] std::string_view getMachineID() const;

	using Board = std::shared_ptr<MSXMotherBoard>;
	[[nodiscard]] Board createEmptyMotherBoard();
	void replaceBoard(MSXMotherBoard& oldBoard, Board newBoard); // for reverse
	[[nodiscard]] Board getMachine(std::string_view machineID) const;

	[[nodiscard]] bool isFullyStarted() const { return fullyStarted; }

	[[nodiscard]] auto getMachineIDs() const {
		return std::views::transform(boards,
			[](auto& b) -> std::string_view { return b->getMachineID(); });
	}
private:
	void createDefaultMachineAndSetupSettings();
	void switchBoard(Board newBoard);
	void deleteBoard(Board board);

	// Observer<Setting>
	void update(const Setting& setting) noexcept override;

	// EventListener
	bool signalEvent(const Event& event) override;

	void unpause();
	void pause();

private:
	std::mutex mbMutex; // this should come first, because it's still used by
	                    // the destructors of the unique_ptr below

	// note: order of unique_ptr's is important
	std::unique_ptr<Shortcuts> shortcuts; // before globalCommandController
	std::unique_ptr<RTScheduler> rtScheduler;
	std::unique_ptr<EventDistributor> eventDistributor;
	std::unique_ptr<GlobalCliComm> globalCliComm;
	std::unique_ptr<GlobalCommandController> globalCommandController;
	std::unique_ptr<GlobalSettings> globalSettings;
	std::unique_ptr<InputEventGenerator> inputEventGenerator;
	std::unique_ptr<SymbolManager> symbolManager; // before imGuiManager
	std::unique_ptr<ImGuiManager> imGuiManager; // before display
	std::unique_ptr<Display> display;
	std::unique_ptr<Mixer> mixer; // lazy initialized
	std::unique_ptr<DiskFactory> diskFactory;
	std::unique_ptr<DiskManipulator> diskManipulator;
	std::unique_ptr<DiskChanger> virtualDrive;
	std::unique_ptr<FilePool> filePool;

	std::unique_ptr<EnumSetting<int>> defaultMachineSetting;
	std::unique_ptr<UserSettings> userSettings;
	std::unique_ptr<RomDatabase> softwareDatabase; // lazy initialized

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
	std::unique_ptr<SetupCommand> setupCommand;
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
	std::vector<Board> boards; // unordered
	Board activeBoard; // either nullptr or a board inside 'boards'

	int blockedCounter = 0;
	bool paused = false;
	bool fullyStarted = false; // all start up actions completed

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
	friend class SetupCommand;
};

} // namespace openmsx

#endif // REACTOR_HH
