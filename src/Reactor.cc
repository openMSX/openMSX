#include "Reactor.hh"

#include "AfterCommand.hh"
#include "AviRecorder.hh"
#include "BooleanSetting.hh"
#include "Command.hh"
#include "CommandException.hh"
#include "CommandLineParser.hh"
#include "DiskChanger.hh"
#include "DiskFactory.hh"
#include "DiskManipulator.hh"
#include "Display.hh"
#include "EnumSetting.hh"
#include "Event.hh"
#include "EventDistributor.hh"
#include "FileContext.hh"
#include "FileException.hh"
#include "FilePool.hh"
#include "GlobalCliComm.hh"
#include "GlobalCommandController.hh"
#include "GlobalSettings.hh"
#include "HardwareConfig.hh"
#include "ImGuiManager.hh"
#include "InfoTopic.hh"
#include "InputEventGenerator.hh"
#include "Keyboard.hh"
#include "MSXMotherBoard.hh"
#include "MessageCommand.hh"
#include "Mixer.hh"
#include "MsxChar2Unicode.hh"
#include "RTScheduler.hh"
#include "RomDatabase.hh"
#include "RomInfo.hh"
#include "StateChangeDistributor.hh"
#include "SymbolManager.hh"
#include "TclCallbackMessages.hh"
#include "TclObject.hh"
#include "UserSettings.hh"
#include "VideoSystem.hh"
#include "XMLElement.hh"
#include "XMLException.hh"

#include "FileOperations.hh"
#include "foreach_file.hh"
#include "Thread.hh"
#include "Timer.hh"

#include "narrow.hh"
#include "ranges.hh"
#include "serialize.hh"
#include "stl.hh"
#include "unreachable.hh"
#include "build-info.hh"

#include <array>
#include <cassert>
#include <memory>

using std::make_unique;
using std::string;
using std::string_view;
using std::vector;

namespace openmsx {

// global variable to communicate the exit-code from the 'exit' command to main()
int exitCode = 0;

class ExitCommand final : public Command
{
public:
	ExitCommand(CommandController& commandController, EventDistributor& distributor);
	void execute(std::span<const TclObject> tokens, TclObject& result) override;
	[[nodiscard]] string help(std::span<const TclObject> tokens) const override;
private:
	EventDistributor& distributor;
};

class MachineCommand final : public Command
{
public:
	MachineCommand(CommandController& commandController, Reactor& reactor);
	void execute(std::span<const TclObject> tokens, TclObject& result) override;
	[[nodiscard]] string help(std::span<const TclObject> tokens) const override;
	void tabCompletion(vector<string>& tokens) const override;
private:
	Reactor& reactor;
};

class TestMachineCommand final : public Command
{
public:
	TestMachineCommand(CommandController& commandController, Reactor& reactor);
	void execute(std::span<const TclObject> tokens, TclObject& result) override;
	[[nodiscard]] string help(std::span<const TclObject> tokens) const override;
	void tabCompletion(vector<string>& tokens) const override;
private:
	Reactor& reactor;
};

class CreateMachineCommand final : public Command
{
public:
	CreateMachineCommand(CommandController& commandController, Reactor& reactor);
	void execute(std::span<const TclObject> tokens, TclObject& result) override;
	[[nodiscard]] string help(std::span<const TclObject> tokens) const override;
private:
	Reactor& reactor;
};

class DeleteMachineCommand final : public Command
{
public:
	DeleteMachineCommand(CommandController& commandController, Reactor& reactor);
	void execute(std::span<const TclObject> tokens, TclObject& result) override;
	[[nodiscard]] string help(std::span<const TclObject> tokens) const override;
	void tabCompletion(vector<string>& tokens) const override;
private:
	Reactor& reactor;
};

class ListMachinesCommand final : public Command
{
public:
	ListMachinesCommand(CommandController& commandController, Reactor& reactor);
	void execute(std::span<const TclObject> tokens, TclObject& result) override;
	[[nodiscard]] string help(std::span<const TclObject> tokens) const override;
private:
	Reactor& reactor;
};

class ActivateMachineCommand final : public Command
{
public:
	ActivateMachineCommand(CommandController& commandController, Reactor& reactor);
	void execute(std::span<const TclObject> tokens, TclObject& result) override;
	[[nodiscard]] string help(std::span<const TclObject> tokens) const override;
	void tabCompletion(vector<string>& tokens) const override;
private:
	Reactor& reactor;
};

class StoreMachineCommand final : public Command
{
public:
	StoreMachineCommand(CommandController& commandController, Reactor& reactor);
	void execute(std::span<const TclObject> tokens, TclObject& result) override;
	[[nodiscard]] string help(std::span<const TclObject> tokens) const override;
	void tabCompletion(vector<string>& tokens) const override;
private:
	Reactor& reactor;
};

class RestoreMachineCommand final : public Command
{
public:
	RestoreMachineCommand(CommandController& commandController, Reactor& reactor);
	void execute(std::span<const TclObject> tokens, TclObject& result) override;
	[[nodiscard]] string help(std::span<const TclObject> tokens) const override;
	void tabCompletion(vector<string>& tokens) const override;
private:
	Reactor& reactor;
};

class GetClipboardCommand final : public Command
{
public:
	GetClipboardCommand(CommandController& commandController, Reactor& reactor);
	void execute(std::span<const TclObject> tokens, TclObject& result) override;
	[[nodiscard]] string help(std::span<const TclObject> tokens) const override;
private:
	Reactor& reactor;
};

class SetClipboardCommand final : public Command
{
public:
	SetClipboardCommand(CommandController& commandController, Reactor& reactor);
	void execute(std::span<const TclObject> tokens, TclObject& result) override;
	[[nodiscard]] string help(std::span<const TclObject> tokens) const override;
private:
	Reactor& reactor;
};

class ConfigInfo final : public InfoTopic
{
public:
	ConfigInfo(InfoCommand& openMSXInfoCommand, const string& configName);
	void execute(std::span<const TclObject> tokens,
	             TclObject& result) const override;
	[[nodiscard]] string help(std::span<const TclObject> tokens) const override;
	void tabCompletion(vector<string>& tokens) const override;
private:
	const string configName;
};

class RealTimeInfo final : public InfoTopic
{
public:
	explicit RealTimeInfo(InfoCommand& openMSXInfoCommand);
	void execute(std::span<const TclObject> tokens,
	             TclObject& result) const override;
	[[nodiscard]] string help(std::span<const TclObject> tokens) const override;
private:
	const uint64_t reference;
};

class SoftwareInfoTopic final : public InfoTopic
{
public:
	SoftwareInfoTopic(InfoCommand& openMSXInfoCommand, Reactor& reactor);
	void execute(std::span<const TclObject> tokens,
	             TclObject& result) const override;
	[[nodiscard]] std::string help(std::span<const TclObject> tokens) const override;
private:
	Reactor& reactor;
};


Reactor::Reactor() = default;

void Reactor::init()
{
	shortcuts = make_unique<Shortcuts>();
	rtScheduler = make_unique<RTScheduler>();
	eventDistributor = make_unique<EventDistributor>(*this);
	globalCliComm = make_unique<GlobalCliComm>();
	globalCommandController = make_unique<GlobalCommandController>(
		*eventDistributor, *globalCliComm, *this);
	globalSettings = make_unique<GlobalSettings>(
		*globalCommandController);
	inputEventGenerator = make_unique<InputEventGenerator>(
		*globalCommandController, *eventDistributor, *globalSettings);
	symbolManager = make_unique<SymbolManager>(
		*globalCommandController);
	imGuiManager = make_unique<ImGuiManager>(*this);
	diskFactory = make_unique<DiskFactory>(*this);
	diskManipulator = make_unique<DiskManipulator>(
		*globalCommandController, *this);
	virtualDrive = make_unique<DiskChanger>(
		*this, "virtual_drive");
	filePool = make_unique<FilePool>(*globalCommandController, *this);
	userSettings = make_unique<UserSettings>(
		*globalCommandController);
	afterCommand = make_unique<AfterCommand>(
		*this, *eventDistributor, *globalCommandController);
	exitCommand = make_unique<ExitCommand>(
		*globalCommandController, *eventDistributor);
	messageCommand = make_unique<MessageCommand>(
		*globalCommandController);
	machineCommand = make_unique<MachineCommand>(
		*globalCommandController, *this);
	testMachineCommand = make_unique<TestMachineCommand>(
		*globalCommandController, *this);
	createMachineCommand = make_unique<CreateMachineCommand>(
		*globalCommandController, *this);
	deleteMachineCommand = make_unique<DeleteMachineCommand>(
		*globalCommandController, *this);
	listMachinesCommand = make_unique<ListMachinesCommand>(
		*globalCommandController, *this);
	activateMachineCommand = make_unique<ActivateMachineCommand>(
		*globalCommandController, *this);
	storeMachineCommand = make_unique<StoreMachineCommand>(
		*globalCommandController, *this);
	restoreMachineCommand = make_unique<RestoreMachineCommand>(
		*globalCommandController, *this);
	getClipboardCommand = make_unique<GetClipboardCommand>(
		*globalCommandController, *this);
	setClipboardCommand = make_unique<SetClipboardCommand>(
		*globalCommandController, *this);
	aviRecordCommand = make_unique<AviRecorder>(*this);
	extensionInfo = make_unique<ConfigInfo>(
		getOpenMSXInfoCommand(), "extensions");
	machineInfo   = make_unique<ConfigInfo>(
		getOpenMSXInfoCommand(), "machines");
	realTimeInfo = make_unique<RealTimeInfo>(
		getOpenMSXInfoCommand());
	softwareInfoTopic = make_unique<SoftwareInfoTopic>(
		getOpenMSXInfoCommand(), *this);
	tclCallbackMessages = make_unique<TclCallbackMessages>(
		*globalCliComm, *globalCommandController);

	createMachineSetting();

	getGlobalSettings().getPauseSetting().attach(*this);

	eventDistributor->registerEventListener(EventType::QUIT, *this);
#if PLATFORM_ANDROID
	eventDistributor->registerEventListener(EventType::WINDOW, *this);
#endif
	isInit = true;
}

Reactor::~Reactor()
{
	if (!isInit) return;
	deleteBoard(activeBoard);

	eventDistributor->unregisterEventListener(EventType::QUIT, *this);
#if PLATFORM_ANDROID
	eventDistributor->unregisterEventListener(EventType::WINDOW, *this);
#endif

	getGlobalSettings().getPauseSetting().detach(*this);
}

Mixer& Reactor::getMixer()
{
	if (!mixer) {
		mixer = make_unique<Mixer>(*this, *globalCommandController);
	}
	return *mixer;
}

RomDatabase& Reactor::getSoftwareDatabase()
{
	if (!softwareDatabase) {
		softwareDatabase = make_unique<RomDatabase>(*globalCliComm);
	}
	return *softwareDatabase;
}

CliComm& Reactor::getCliComm()
{
	return *globalCliComm;
}

Interpreter& Reactor::getInterpreter()
{
	return getGlobalCommandController().getInterpreter();
}

CommandController& Reactor::getCommandController()
{
	return *globalCommandController;
}

InfoCommand& Reactor::getOpenMSXInfoCommand()
{
	return globalCommandController->getOpenMSXInfoCommand();
}

const HotKey& Reactor::getHotKey() const
{
	return globalCommandController->getHotKey();
}

vector<string> Reactor::getHwConfigs(string_view type)
{
	vector<string> result;
	for (const auto& p : systemFileContext().getPaths()) {
		auto fileAction = [&](const std::string& /*path*/, std::string_view name) {
			if (name.ends_with(".xml")) {
				name.remove_suffix(4);
				result.emplace_back(name);
			}
		};
		auto dirAction = [&](std::string& path, std::string_view name) {
			auto size = path.size();
			path += "/hardwareconfig.xml";
			if (FileOperations::isRegularFile(path)) {
				result.emplace_back(name);
			}
			path.resize(size);
		};
		foreach_file_and_directory(FileOperations::join(p, type), fileAction, dirAction);
	}
	// remove duplicates
	ranges::sort(result);
	result.erase(ranges::unique(result), end(result));
	return result;
}

const MsxChar2Unicode& Reactor::getMsxChar2Unicode() const
{
	// TODO cleanup this code. It should be easier to get a hold of the
	// 'MsxChar2Unicode' object. Probably the 'Keyboard' class is not the
	// right location to store it.
	try {
		if (const auto* board = getMotherBoard()) {
			if (const auto* keyb = board->getKeyboard()) {
				return keyb->getMsxChar2Unicode();
			}
		}
	} catch (MSXException&) {
		// ignore
	}
	static const MsxChar2Unicode defaultMsxChars("MSXVID.TXT");
	return defaultMsxChars;
}


void Reactor::createMachineSetting()
{
	auto names = getHwConfigs("machines");
	EnumSetting<int>::Map machines; // int's are unique dummy values
	machines.reserve(names.size() + 1);
	int count = 1;
	append(machines, view::transform(names,
		[&](auto& name) { return EnumSettingBase::MapEntry(std::move(name), count++); }));
	machines.emplace_back("C-BIOS_MSX2+", 0); // default machine

	machineSetting = make_unique<EnumSetting<int>>(
		*globalCommandController, "default_machine",
		"default machine (takes effect next time openMSX is started)",
		0, std::move(machines));
}

MSXMotherBoard* Reactor::getMotherBoard() const
{
	assert(Thread::isMainThread());
	return activeBoard.get();
}

string_view Reactor::getMachineID() const
{
	return activeBoard ? activeBoard->getMachineID() : string_view{};
}

Reactor::Board Reactor::getMachine(string_view machineID) const
{
	if (auto it = ranges::find(boards, machineID, &MSXMotherBoard::getMachineID);
	    it != boards.end()) {
		return *it;
	}
	throw CommandException("No machine with ID: ", machineID);
}

Reactor::Board Reactor::createEmptyMotherBoard()
{
	return std::make_shared<MSXMotherBoard>(*this);
}

void Reactor::replaceBoard(MSXMotherBoard& oldBoard_, Board newBoard)
{
	assert(Thread::isMainThread());

	// Add new board.
	boards.push_back(newBoard);

	// Lookup old board (it must be present).
	auto it = find_unguarded(boards, &oldBoard_,
	                         [](auto& b) { return b.get(); });

	// If the old board was the active board, then activate the new board
	if (*it == activeBoard) {
		switchBoard(newBoard);
	}

	// Remove the old board.
	move_pop_back(boards, it);
}

void Reactor::switchMachine(const string& machine)
{
	if (!display) {
		display = make_unique<Display>(*this);
		// TODO: Currently it is not possible to move this call into the
		//       constructor of Display because the call to createVideoSystem()
		//       indirectly calls Reactor.getDisplay().
		display->createVideoSystem();
	}

	// create+load new machine
	// switch to new machine
	// delete old active machine

	assert(Thread::isMainThread());
	// Note: loadMachine can throw an exception and in that case the
	//       motherboard must be considered as not created at all.
	auto newBoard = createEmptyMotherBoard();
	newBoard->loadMachine(machine);
	boards.push_back(newBoard);

	auto oldBoard = activeBoard;
	switchBoard(newBoard);
	deleteBoard(oldBoard);
}

void Reactor::switchBoard(Board newBoard)
{
	assert(Thread::isMainThread());
	assert(!newBoard || contains(boards, newBoard));
	assert(!activeBoard || contains(boards, activeBoard));
	if (activeBoard) {
		activeBoard->activate(false);
	}
	{
		// Don't hold the lock for longer than the actual switch.
		// In the past we had a potential for deadlocks here, because
		// (indirectly) the code below still acquires other locks.
		std::scoped_lock lock(mbMutex);
		activeBoard = newBoard;
	}
	eventDistributor->distributeEvent(MachineLoadedEvent());
	globalCliComm->update(CliComm::UpdateType::HARDWARE, getMachineID(), "select");
	if (activeBoard) {
		activeBoard->activate(true);
	}
}

void Reactor::deleteBoard(Board board)
{
	// Note: pass 'board' by-value to keep the parameter from changing
	// after the call to switchBoard(). switchBoard() changes the
	// 'activeBoard' member variable, so the 'board' parameter would change
	// if it were passed by reference to this method (AFAICS this only
	// happens in ~Reactor()).
	assert(Thread::isMainThread());
	if (!board) return;

	if (board == activeBoard) {
		// delete active board -> there is no active board anymore
		switchBoard(nullptr);
	}
	auto it = rfind_unguarded(boards, board);
	move_pop_back(boards, it);
}

void Reactor::enterMainLoop()
{
	// Note: this method can get called from different threads
	if (Thread::isMainThread()) {
		// Don't take lock in main thread to avoid recursive locking.
		if (activeBoard) {
			activeBoard->exitCPULoopSync();
		}
	} else {
		std::scoped_lock lock(mbMutex);
		if (activeBoard) {
			activeBoard->exitCPULoopAsync();
		}
	}
}

void Reactor::runStartupScripts(const CommandLineParser& parser)
{
	auto& commandController = *globalCommandController;

	// execute init.tcl
	try {
		commandController.source(
			preferSystemFileContext().resolve("init.tcl"));
	} catch (FileException&) {
		// no init.tcl, ignore
	}

	// execute startup scripts
	for (const auto& s : parser.getStartupScripts()) {
		try {
			commandController.source(userFileContext().resolve(s));
		} catch (FileException& e) {
			throw FatalError("Couldn't execute script: ",
			                 e.getMessage());
		}
	}
	for (const auto& cmd : parser.getStartupCommands()) {
		try {
			commandController.executeCommand(cmd);
		} catch (CommandException& e) {
			throw FatalError("Couldn't execute command: ", cmd,
			                 '\n', e.getMessage());
		}
	}

	fullyStarted = true;

	// At this point openmsx is fully started, it's OK now to start
	// accepting external commands
	getGlobalCliComm().setAllowExternalCommands();

	// ...and re-emit any postponed message callbacks now that the scripts
	// are loaded
	tclCallbackMessages->redoPostponedCallbacks();
}

void Reactor::powerOn()
{
	// don't use Tcl to power up the machine, we cannot pass
	// exceptions through Tcl and ADVRAM might throw in its
	// powerUp() method. Solution is to implement dependencies
	// between devices so ADVRAM can check the error condition
	// in its constructor
	//commandController.executeCommand("set power on");
	if (activeBoard) {
		activeBoard->powerUp();
	}
}

void Reactor::run()
{
	while (running) {
		eventDistributor->deliverEvents();
		bool blocked = (blockedCounter > 0) || !activeBoard;
		if (!blocked) {
			// copy shared_ptr to keep Board alive (e.g. in case of Tcl
			// callbacks)
			auto copy = activeBoard;
			blocked = !copy->execute();
		}
		if (blocked) {
			// At first sight a better alternative is to use the
			// SDL_WaitEvent() function. Though when inspecting
			// the implementation of that function, it turns out
			// to also use a sleep/poll loop, with even shorter
			// sleep periods as we use here. Maybe in future
			// SDL implementations this will be improved.
			eventDistributor->sleep(20 * 1000);
		}
	}
}

void Reactor::unpause()
{
	if (paused) {
		paused = false;
		globalCliComm->update(CliComm::UpdateType::STATUS, "paused", "false");
		unblock();
	}
}

void Reactor::pause()
{
	if (!paused) {
		paused = true;
		globalCliComm->update(CliComm::UpdateType::STATUS, "paused", "true");
		block();
	}
}

void Reactor::block()
{
	++blockedCounter;
	enterMainLoop();
	getMixer().mute();
}

void Reactor::unblock()
{
	--blockedCounter;
	assert(blockedCounter >= 0);
	getMixer().unmute();
}


// Observer<Setting>
void Reactor::update(const Setting& setting) noexcept
{
	const auto& pauseSetting = getGlobalSettings().getPauseSetting();
	if (&setting == &pauseSetting) {
		if (pauseSetting.getBoolean()) {
			pause();
		} else {
			unpause();
		}
	}
}

// EventListener
bool Reactor::signalEvent(const Event& event)
{
	std::visit(overloaded{
		[&](const QuitEvent& /*e*/) {
			enterMainLoop();
			running = false;
		},
		[&](const WindowEvent& e) {
			(void)e;
#if PLATFORM_ANDROID
			if (e.isMainWindow()) {
				// Android SDL port sends a (un)focus event when an app is put in background
				// by the OS for whatever reason (like an incoming phone call) and all screen
				// resources are taken away from the app.
				// In such case the app is supposed to behave as a good citizen
				// and minimize its resource usage and related battery drain.
				// The SDL Android port already takes care of halting the Java
				// part of the sound processing. The Display class makes sure that it wont try
				// to render anything to the (temporary missing) graphics resources but the
				// main emulation should also be temporary stopped, in order to minimize CPU usage
				if (e.getSdlWindowEvent().type == SDL_WINDOWEVENT_FOCUS_GAINED) {
					unblock();
				} else if (e.getSdlWindowEvent().type == SDL_WINDOWEVENT_FOCUS_LOST) {
					block();
				}
			}
#endif
		},
		[](const EventBase /*e*/) {
			UNREACHABLE; // we didn't subscribe to this event...
		}
	}, event);
	return false;
}


// class ExitCommand

ExitCommand::ExitCommand(CommandController& commandController_,
                         EventDistributor& distributor_)
	: Command(commandController_, "exit")
	, distributor(distributor_)
{
}

void ExitCommand::execute(std::span<const TclObject> tokens, TclObject& /*result*/)
{
	checkNumArgs(tokens, Between{1, 2}, Prefix{1}, "?exitcode?");
	switch (tokens.size()) {
	case 1:
		exitCode = 0;
		break;
	case 2:
		exitCode = tokens[1].getInt(getInterpreter());
		break;
	}
	distributor.distributeEvent(QuitEvent());
}

string ExitCommand::help(std::span<const TclObject> /*tokens*/) const
{
	return "Use this command to stop the emulator.\n"
	       "Optionally you can pass an exit-code.\n";
}


// class MachineCommand

MachineCommand::MachineCommand(CommandController& commandController_,
                               Reactor& reactor_)
	: Command(commandController_, "machine")
	, reactor(reactor_)
{
}

void MachineCommand::execute(std::span<const TclObject> tokens, TclObject& result)
{
	checkNumArgs(tokens, Between{1, 2}, Prefix{1}, "?machinetype?");
	switch (tokens.size()) {
	case 1: // get current machine
		// nothing
		break;
	case 2:
		try {
			reactor.switchMachine(string(tokens[1].getString()));
		} catch (MSXException& e) {
			throw CommandException("Machine switching failed: ",
			                       e.getMessage());
		}
		break;
	}
	// Always return machineID (of current or of new machine).
	result = reactor.getMachineID();
}

string MachineCommand::help(std::span<const TclObject> /*tokens*/) const
{
	return "Switch to a different MSX machine.";
}

void MachineCommand::tabCompletion(vector<string>& tokens) const
{
	completeString(tokens, Reactor::getHwConfigs("machines"));
}


// class TestMachineCommand

TestMachineCommand::TestMachineCommand(CommandController& commandController_,
                                       Reactor& reactor_)
	: Command(commandController_, "test_machine")
	, reactor(reactor_)
{
}

void TestMachineCommand::execute(std::span<const TclObject> tokens,
                                 TclObject& result)
{
	checkNumArgs(tokens, 2, "machinetype");
	try {
		MSXMotherBoard mb(reactor);
		mb.loadMachine(string(tokens[1].getString()));
	} catch (MSXException& e) {
		result = e.getMessage(); // error
	}
}

string TestMachineCommand::help(std::span<const TclObject> /*tokens*/) const
{
	return "Test the configuration for the given machine. "
	       "Returns an error message explaining why the configuration is "
	       "invalid or an empty string in case of success.";
}

void TestMachineCommand::tabCompletion(vector<string>& tokens) const
{
	completeString(tokens, Reactor::getHwConfigs("machines"));
}


// class CreateMachineCommand

CreateMachineCommand::CreateMachineCommand(
	CommandController& commandController_, Reactor& reactor_)
	: Command(commandController_, "create_machine")
	, reactor(reactor_)
{
}

void CreateMachineCommand::execute(std::span<const TclObject> tokens, TclObject& result)
{
	checkNumArgs(tokens, 1, Prefix{1}, nullptr);
	auto newBoard = reactor.createEmptyMotherBoard();
	result = newBoard->getMachineID();
	reactor.boards.push_back(std::move(newBoard));
}

string CreateMachineCommand::help(std::span<const TclObject> /*tokens*/) const
{
	return "Creates a new (empty) MSX machine. Returns the ID for the new "
	       "machine.\n"
	       "Use 'load_machine' to actually load a machine configuration "
	       "into this new machine.\n"
	       "The main reason create_machine and load_machine are two "
	       "separate commands is that sometimes you already want to know "
	       "the ID of the machine before load_machine starts emitting "
	       "events for this machine.";
}


// class DeleteMachineCommand

DeleteMachineCommand::DeleteMachineCommand(
	CommandController& commandController_, Reactor& reactor_)
	: Command(commandController_, "delete_machine")
	, reactor(reactor_)
{
}

void DeleteMachineCommand::execute(std::span<const TclObject> tokens,
                                   TclObject& /*result*/)
{
	checkNumArgs(tokens, 2, "id");
	reactor.deleteBoard(reactor.getMachine(tokens[1].getString()));
}

string DeleteMachineCommand::help(std::span<const TclObject> /*tokens*/) const
{
	return "Deletes the given MSX machine.";
}

void DeleteMachineCommand::tabCompletion(vector<string>& tokens) const
{
	completeString(tokens, reactor.getMachineIDs());
}


// class ListMachinesCommand

ListMachinesCommand::ListMachinesCommand(
	CommandController& commandController_, Reactor& reactor_)
	: Command(commandController_, "list_machines")
	, reactor(reactor_)
{
}

void ListMachinesCommand::execute(std::span<const TclObject> /*tokens*/,
                                  TclObject& result)
{
	result.addListElements(reactor.getMachineIDs());
}

string ListMachinesCommand::help(std::span<const TclObject> /*tokens*/) const
{
	return "Returns a list of all machine IDs.";
}


// class ActivateMachineCommand

ActivateMachineCommand::ActivateMachineCommand(
	CommandController& commandController_, Reactor& reactor_)
	: Command(commandController_, "activate_machine")
	, reactor(reactor_)
{
}

void ActivateMachineCommand::execute(std::span<const TclObject> tokens,
                                     TclObject& result)
{
	checkNumArgs(tokens, Between{1, 2}, Prefix{1}, "id");
	switch (tokens.size()) {
	case 1:
		break;
	case 2:
		reactor.switchBoard(reactor.getMachine(tokens[1].getString()));
		break;
	}
	result = reactor.getMachineID();
}

string ActivateMachineCommand::help(std::span<const TclObject> /*tokens*/) const
{
	return "Make another machine the active msx machine.\n"
	       "Or when invoked without arguments, query the ID of the "
	       "active msx machine.";
}

void ActivateMachineCommand::tabCompletion(vector<string>& tokens) const
{
	completeString(tokens, reactor.getMachineIDs());
}


// class StoreMachineCommand

StoreMachineCommand::StoreMachineCommand(
	CommandController& commandController_, Reactor& reactor_)
	: Command(commandController_, "store_machine")
	, reactor(reactor_)
{
}

void StoreMachineCommand::execute(std::span<const TclObject> tokens, TclObject& result)
{
	checkNumArgs(tokens, 3, Prefix{1}, "id filename");
	const auto& machineID = tokens[1].getString();
	const auto& filename = tokens[2].getString();

	const auto& board = *reactor.getMachine(machineID);

	XmlOutputArchive out(filename);
	out.serialize("machine", board);
	out.close();
	result = filename;
}

string StoreMachineCommand::help(std::span<const TclObject> /*tokens*/) const
{
	return
		"store_machine machineID <filename>  Save state of machine \"machineID\" to indicated file\n"
		"\n"
		"This is a low-level command, the 'savestate' script is easier to use.";
}

void StoreMachineCommand::tabCompletion(vector<string>& tokens) const
{
	completeString(tokens, reactor.getMachineIDs());
}


// class RestoreMachineCommand

RestoreMachineCommand::RestoreMachineCommand(
	CommandController& commandController_, Reactor& reactor_)
	: Command(commandController_, "restore_machine")
	, reactor(reactor_)
{
}

void RestoreMachineCommand::execute(std::span<const TclObject> tokens,
                                    TclObject& result)
{
	checkNumArgs(tokens, 2, Prefix{1}, "filename");
	auto newBoard = reactor.createEmptyMotherBoard();

	const auto filename = FileOperations::expandTilde(string(tokens[1].getString()));

	try {
		XmlInputArchive in(filename);
		in.serialize("machine", *newBoard);
	} catch (XMLException& e) {
		throw CommandException("Cannot load state, bad file format: ",
		                       e.getMessage());
	} catch (MSXException& e) {
		throw CommandException("Cannot load state: ", e.getMessage());
	}

	// Savestate also contains stuff like the keyboard state at the moment
	// the snapshot was created (this is required for reverse/replay). But
	// now we want the MSX to see the actual host keyboard state.
	newBoard->getStateChangeDistributor().stopReplay(newBoard->getCurrentTime());

	result = newBoard->getMachineID();
	reactor.boards.push_back(std::move(newBoard));
}

string RestoreMachineCommand::help(std::span<const TclObject> /*tokens*/) const
{
	return "restore_machine                       Load state from last saved state in default directory\n"
	       "restore_machine <filename>            Load state from indicated file\n"
	       "\n"
	       "This is a low-level command, the 'loadstate' script is easier to use.";
}

void RestoreMachineCommand::tabCompletion(vector<string>& tokens) const
{
	completeFileName(tokens, userFileContext());
}


// class GetClipboardCommand

GetClipboardCommand::GetClipboardCommand(
	CommandController& commandController_, Reactor& reactor_)
	: Command(commandController_, "get_clipboard_text")
	, reactor(reactor_)
{
	// Note: cannot yet call getReactor().getDisplay() (e.g. to cache it)
	//   display may not yet be initialized.
}

void GetClipboardCommand::execute(std::span<const TclObject> tokens, TclObject& result)
{
	checkNumArgs(tokens, 1, Prefix{1}, nullptr);
	result = reactor.getDisplay().getVideoSystem().getClipboardText();
}

string GetClipboardCommand::help(std::span<const TclObject> /*tokens*/) const
{
	return "Returns the (text) content of the clipboard as a string.";
}


// class SetClipboardCommand

SetClipboardCommand::SetClipboardCommand(
	CommandController& commandController_, Reactor& reactor_)
	: Command(commandController_, "set_clipboard_text")
	, reactor(reactor_)
{
}

void SetClipboardCommand::execute(std::span<const TclObject> tokens, TclObject& /*result*/)
{
	checkNumArgs(tokens, 2, "text");
	reactor.getDisplay().getVideoSystem().setClipboardText(tokens[1].getString());
}

string SetClipboardCommand::help(std::span<const TclObject> /*tokens*/) const
{
	return "Send the given string to the clipboard.";
}


// class ConfigInfo

ConfigInfo::ConfigInfo(InfoCommand& openMSXInfoCommand,
	               const string& configName_)
	: InfoTopic(openMSXInfoCommand, configName_)
	, configName(configName_)
{
}

void ConfigInfo::execute(std::span<const TclObject> tokens, TclObject& result) const
{
	// TODO make meta info available through this info topic
	switch (tokens.size()) {
	case 2: {
		result.addListElements(Reactor::getHwConfigs(configName));
		break;
	}
	case 3: {
		try {
			std::array<char, 8192> allocBuffer; // tweak
			XMLDocument doc{allocBuffer.data(), sizeof(allocBuffer)};
			HardwareConfig::loadConfig(
				doc, configName, tokens[2].getString());
			if (const auto* info = doc.getRoot()->findChild("info")) {
				for (const auto& c : info->getChildren()) {
					result.addDictKeyValue(c.getName(), c.getData());
				}
			}
		} catch (MSXException& e) {
			throw CommandException(
				"Couldn't get config info: ", e.getMessage());
		}
		break;
	}
	default:
		throw CommandException("Too many parameters");
	}
}

string ConfigInfo::help(std::span<const TclObject> /*tokens*/) const
{
	return strCat("Shows a list of available ", configName, ", "
	              "or get meta information about the selected item.\n");
}

void ConfigInfo::tabCompletion(vector<string>& tokens) const
{
	completeString(tokens, Reactor::getHwConfigs(configName));
}


// class RealTimeInfo

RealTimeInfo::RealTimeInfo(InfoCommand& openMSXInfoCommand)
	: InfoTopic(openMSXInfoCommand, "realtime")
	, reference(Timer::getTime())
{
}

void RealTimeInfo::execute(std::span<const TclObject> /*tokens*/,
                           TclObject& result) const
{
	auto delta = Timer::getTime() - reference;
	result = narrow_cast<double>(delta) * (1.0 / 1000000.0);
}

string RealTimeInfo::help(std::span<const TclObject> /*tokens*/) const
{
	return "Returns the time in seconds since openMSX was started.";
}


// SoftwareInfoTopic

SoftwareInfoTopic::SoftwareInfoTopic(InfoCommand& openMSXInfoCommand, Reactor& reactor_)
	: InfoTopic(openMSXInfoCommand, "software")
	, reactor(reactor_)
{
}

void SoftwareInfoTopic::execute(
	std::span<const TclObject> tokens, TclObject& result) const
{
	if (tokens.size() != 3) {
		throw CommandException("Wrong number of parameters");
	}

	Sha1Sum sha1sum(tokens[2].getString());
	const auto& romDatabase = reactor.getSoftwareDatabase();
	const RomInfo* romInfo = romDatabase.fetchRomInfo(sha1sum);
	if (!romInfo) {
		// no match found
		throw CommandException(
			"Software with sha1sum ", sha1sum.toString(), " not found");
	}

	const char* bufStart = romDatabase.getBufferStart();
	result.addDictKeyValues("title",            romInfo->getTitle(bufStart),
	                        "year",             romInfo->getYear(bufStart),
	                        "company",          romInfo->getCompany(bufStart),
	                        "country",          romInfo->getCountry(bufStart),
	                        "orig_type",        romInfo->getOrigType(bufStart),
	                        "remark",           romInfo->getRemark(bufStart),
	                        "original",         romInfo->getOriginal(),
	                        "mapper_type_name", RomInfo::romTypeToName(romInfo->getRomType()),
	                        "genmsxid",         romInfo->getGenMSXid());
}

string SoftwareInfoTopic::help(std::span<const TclObject> /*tokens*/) const
{
	return "Returns information about the software "
	       "given its sha1sum, in a paired list.";
}

} // namespace openmsx
