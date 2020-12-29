#include "Reactor.hh"
#include "CommandLineParser.hh"
#include "RTScheduler.hh"
#include "EventDistributor.hh"
#include "GlobalCommandController.hh"
#include "InputEventGenerator.hh"
#include "InputEvents.hh"
#include "DiskFactory.hh"
#include "DiskManipulator.hh"
#include "DiskChanger.hh"
#include "FilePool.hh"
#include "UserSettings.hh"
#include "RomDatabase.hh"
#include "RomInfo.hh"
#include "TclCallbackMessages.hh"
#include "MSXMotherBoard.hh"
#include "StateChangeDistributor.hh"
#include "Command.hh"
#include "AfterCommand.hh"
#include "MessageCommand.hh"
#include "CommandException.hh"
#include "GlobalCliComm.hh"
#include "InfoTopic.hh"
#include "Display.hh"
#include "Mixer.hh"
#include "AviRecorder.hh"
#include "GlobalSettings.hh"
#include "BooleanSetting.hh"
#include "EnumSetting.hh"
#include "TclObject.hh"
#include "HardwareConfig.hh"
#include "XMLElement.hh"
#include "XMLException.hh"
#include "FileContext.hh"
#include "FileException.hh"
#include "FileOperations.hh"
#include "foreach_file.hh"
#include "Thread.hh"
#include "Timer.hh"
#include "serialize.hh"
#include "checked_cast.hh"
#include "ranges.hh"
#include "statp.hh"
#include "stl.hh"
#include "StringOp.hh"
#include "unreachable.hh"
#include "view.hh"
#include "build-info.hh"
#include <cassert>
#include <memory>

using std::make_shared;
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
	void execute(span<const TclObject> tokens, TclObject& result) override;
	[[nodiscard]] string help(const vector<string>& tokens) const override;
private:
	EventDistributor& distributor;
};

class MachineCommand final : public Command
{
public:
	MachineCommand(CommandController& commandController, Reactor& reactor);
	void execute(span<const TclObject> tokens, TclObject& result) override;
	[[nodiscard]] string help(const vector<string>& tokens) const override;
	void tabCompletion(vector<string>& tokens) const override;
private:
	Reactor& reactor;
};

class TestMachineCommand final : public Command
{
public:
	TestMachineCommand(CommandController& commandController, Reactor& reactor);
	void execute(span<const TclObject> tokens, TclObject& result) override;
	[[nodiscard]] string help(const vector<string>& tokens) const override;
	void tabCompletion(vector<string>& tokens) const override;
private:
	Reactor& reactor;
};

class CreateMachineCommand final : public Command
{
public:
	CreateMachineCommand(CommandController& commandController, Reactor& reactor);
	void execute(span<const TclObject> tokens, TclObject& result) override;
	[[nodiscard]] string help(const vector<string>& tokens) const override;
private:
	Reactor& reactor;
};

class DeleteMachineCommand final : public Command
{
public:
	DeleteMachineCommand(CommandController& commandController, Reactor& reactor);
	void execute(span<const TclObject> tokens, TclObject& result) override;
	[[nodiscard]] string help(const vector<string>& tokens) const override;
	void tabCompletion(vector<string>& tokens) const override;
private:
	Reactor& reactor;
};

class ListMachinesCommand final : public Command
{
public:
	ListMachinesCommand(CommandController& commandController, Reactor& reactor);
	void execute(span<const TclObject> tokens, TclObject& result) override;
	[[nodiscard]] string help(const vector<string>& tokens) const override;
private:
	Reactor& reactor;
};

class ActivateMachineCommand final : public Command
{
public:
	ActivateMachineCommand(CommandController& commandController, Reactor& reactor);
	void execute(span<const TclObject> tokens, TclObject& result) override;
	[[nodiscard]] string help(const vector<string>& tokens) const override;
	void tabCompletion(vector<string>& tokens) const override;
private:
	Reactor& reactor;
};

class StoreMachineCommand final : public Command
{
public:
	StoreMachineCommand(CommandController& commandController, Reactor& reactor);
	void execute(span<const TclObject> tokens, TclObject& result) override;
	[[nodiscard]] string help(const vector<string>& tokens) const override;
	void tabCompletion(vector<string>& tokens) const override;
private:
	Reactor& reactor;
};

class RestoreMachineCommand final : public Command
{
public:
	RestoreMachineCommand(CommandController& commandController, Reactor& reactor);
	void execute(span<const TclObject> tokens, TclObject& result) override;
	[[nodiscard]] string help(const vector<string>& tokens) const override;
	void tabCompletion(vector<string>& tokens) const override;
private:
	Reactor& reactor;
};

class GetClipboardCommand final : public Command
{
public:
	GetClipboardCommand(CommandController& commandController);
	void execute(span<const TclObject> tokens, TclObject& result) override;
	[[nodiscard]] string help(const vector<string>& tokens) const override;
};

class SetClipboardCommand final : public Command
{
public:
	SetClipboardCommand(CommandController& commandController);
	void execute(span<const TclObject> tokens, TclObject& result) override;
	[[nodiscard]] string help(const vector<string>& tokens) const override;
};

class ConfigInfo final : public InfoTopic
{
public:
	ConfigInfo(InfoCommand& openMSXInfoCommand, const string& configName);
	void execute(span<const TclObject> tokens,
	             TclObject& result) const override;
	[[nodiscard]] string help(const vector<string>& tokens) const override;
	void tabCompletion(vector<string>& tokens) const override;
private:
	const string configName;
};

class RealTimeInfo final : public InfoTopic
{
public:
	explicit RealTimeInfo(InfoCommand& openMSXInfoCommand);
	void execute(span<const TclObject> tokens,
	             TclObject& result) const override;
	[[nodiscard]] string help(const vector<string>& tokens) const override;
private:
	const uint64_t reference;
};

class SoftwareInfoTopic final : InfoTopic
{
public:
	SoftwareInfoTopic(InfoCommand& openMSXInfoCommand, Reactor& reactor);
	void execute(span<const TclObject> tokens,
	             TclObject& result) const override;
	[[nodiscard]] std::string help(const std::vector<std::string>& tokens) const override;
private:
	Reactor& reactor;
};


Reactor::Reactor() = default;

void Reactor::init()
{
	rtScheduler = make_unique<RTScheduler>();
	eventDistributor = make_unique<EventDistributor>(*this);
	globalCliComm = make_unique<GlobalCliComm>();
	globalCommandController = make_unique<GlobalCommandController>(
		*eventDistributor, *globalCliComm, *this);
	globalSettings = make_unique<GlobalSettings>(
		*globalCommandController);
	inputEventGenerator = make_unique<InputEventGenerator>(
		*globalCommandController, *eventDistributor, *globalSettings);
	diskFactory = make_unique<DiskFactory>(
		*this);
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
		*globalCommandController);
	setClipboardCommand = make_unique<SetClipboardCommand>(
		*globalCommandController);
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

	eventDistributor->registerEventListener(OPENMSX_QUIT_EVENT, *this);
#if PLATFORM_ANDROID
	eventDistributor->registerEventListener(OPENMSX_FOCUS_EVENT, *this);
#endif
	eventDistributor->registerEventListener(OPENMSX_DELETE_BOARDS, *this);
	isInit = true;
}

Reactor::~Reactor()
{
	if (!isInit) return;
	deleteBoard(activeBoard);

	eventDistributor->unregisterEventListener(OPENMSX_QUIT_EVENT, *this);
#if PLATFORM_ANDROID
	eventDistributor->unregisterEventListener(OPENMSX_FOCUS_EVENT, *this);
#endif
	eventDistributor->unregisterEventListener(OPENMSX_DELETE_BOARDS, *this);

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

vector<string> Reactor::getHwConfigs(string_view type)
{
	vector<string> result;
	for (auto& p : systemFileContext().getPaths()) {
		auto fileAction = [&](const std::string& /*path*/, std::string_view name) {
			if (StringOp::endsWith(name, ".xml")) {
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

void Reactor::createMachineSetting()
{
	auto names = getHwConfigs("machines");
	EnumSetting<int>::Map machines; // int's are unique dummy values
	machines.reserve(names.size() + 1);
	int count = 1;
	append(machines, view::transform(names,
		[&](auto& name) { return std::pair(std::move(name), count++); }));
	machines.emplace_back("C-BIOS_MSX2+", 0); // default machine

	machineSetting = make_unique<EnumSetting<int>>(
		*globalCommandController, "default_machine",
		"default machine (takes effect next time openMSX is started)",
		0, std::move(machines));
}

MSXMotherBoard* Reactor::getMotherBoard() const
{
	assert(Thread::isMainThread());
	return activeBoard;
}

string_view Reactor::getMachineID() const
{
	return activeBoard ? activeBoard->getMachineID() : string_view{};
}

vector<string_view> Reactor::getMachineIDs() const
{
	return to_vector(view::transform(
		boards, [](auto& b) { return b->getMachineID(); }));
}

MSXMotherBoard& Reactor::getMachine(string_view machineID) const
{
	for (auto& b : boards) {
		if (b->getMachineID() == machineID) {
			return *b;
		}
	}
	throw CommandException("No machine with ID: ", machineID);
}

Reactor::Board Reactor::createEmptyMotherBoard()
{
	return make_unique<MSXMotherBoard>(*this);
}

void Reactor::replaceBoard(MSXMotherBoard& oldBoard_, Board newBoard_)
{
	assert(Thread::isMainThread());

	// Add new board.
	auto* newBoard = newBoard_.get();
	boards.push_back(move(newBoard_));

	// Lookup old board (it must be present).
	auto it = find_if_unguarded(boards,
		[&](auto& b) { return b.get() == &oldBoard_; });

	// If the old board was the active board, then activate the new board
	if (it->get() == activeBoard) {
		switchBoard(newBoard);
	}

	// Remove (=delete) the old board.
	// Note that we don't use the 'garbageBoards' mechanism as used in
	// deleteBoard(). This means oldBoard cannot be used  anymore right
	// after this method returns.
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
	auto newBoard_ = createEmptyMotherBoard();
	auto* newBoard = newBoard_.get();
	newBoard->loadMachine(machine);
	boards.push_back(move(newBoard_));

	auto* oldBoard = activeBoard;
	switchBoard(newBoard);
	deleteBoard(oldBoard);
}

void Reactor::switchBoard(MSXMotherBoard* newBoard)
{
	assert(Thread::isMainThread());
	assert(!newBoard ||
	       (ranges::any_of(boards, [&](auto& b) { return b.get() == newBoard; })));
	assert(!activeBoard ||
	       (ranges::any_of(boards, [&](auto& b) { return b.get() == activeBoard; })));
	if (activeBoard) {
		activeBoard->activate(false);
	}
	{
		// Don't hold the lock for longer than the actual switch.
		// In the past we had a potential for deadlocks here, because
		// (indirectly) the code below still acquires other locks.
		std::lock_guard<std::mutex> lock(mbMutex);
		activeBoard = newBoard;
	}
	eventDistributor->distributeEvent(
		make_shared<SimpleEvent>(OPENMSX_MACHINE_LOADED_EVENT));
	globalCliComm->update(CliComm::HARDWARE, getMachineID(), "select");
	if (activeBoard) {
		activeBoard->activate(true);
	}
}

void Reactor::deleteBoard(MSXMotherBoard* board)
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
	auto it = rfind_if_unguarded(boards,
		[&](auto& b) { return b.get() == board; });
	auto board_ = move(*it);
	move_pop_back(boards, it);
	// Don't immediately delete old boards because it's possible this
	// routine is called via a code path that goes through the old
	// board. Instead remember this board and delete it at a safe moment
	// in time.
	garbageBoards.push_back(move(board_));
	eventDistributor->distributeEvent(
		make_shared<SimpleEvent>(OPENMSX_DELETE_BOARDS));
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
		std::lock_guard<std::mutex> lock(mbMutex);
		if (activeBoard) {
			activeBoard->exitCPULoopAsync();
		}
	}
}

void Reactor::run(CommandLineParser& parser)
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

	// At this point openmsx is fully started, it's OK now to start
	// accepting external commands
	getGlobalCliComm().setAllowExternalCommands();

	// Run
	if (parser.getParseStatus() == CommandLineParser::RUN) {
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

	while (doOneIteration()) {
		// nothing
	}
}

bool Reactor::doOneIteration()
{
	eventDistributor->deliverEvents();
	assert(garbageBoards.empty());
	bool blocked = (blockedCounter > 0) || !activeBoard;
	if (!blocked) blocked = !activeBoard->execute();
	if (blocked) {
		// At first sight a better alternative is to use the
		// SDL_WaitEvent() function. Though when inspecting
		// the implementation of that function, it turns out
		// to also use a sleep/poll loop, with even shorter
		// sleep periods as we use here. Maybe in future
		// SDL implementations this will be improved.
		eventDistributor->sleep(20 * 1000);
	}
	return running;
}

void Reactor::unpause()
{
	if (paused) {
		paused = false;
		globalCliComm->update(CliComm::STATUS, "paused", "false");
		unblock();
	}
}

void Reactor::pause()
{
	if (!paused) {
		paused = true;
		globalCliComm->update(CliComm::STATUS, "paused", "true");
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
void Reactor::update(const Setting& setting)
{
	auto& pauseSetting = getGlobalSettings().getPauseSetting();
	if (&setting == &pauseSetting) {
		if (pauseSetting.getBoolean()) {
			pause();
		} else {
			unpause();
		}
	}
}

// EventListener
int Reactor::signalEvent(const std::shared_ptr<const Event>& event)
{
	auto type = event->getType();
	if (type == OPENMSX_QUIT_EVENT) {
		enterMainLoop();
		running = false;
#if PLATFORM_ANDROID
	} else if (type == OPENMSX_FOCUS_EVENT) {
		// Android SDL port sends a (un)focus event when an app is put in background
		// by the OS for whatever reason (like an incoming phone call) and all screen
		// resources are taken away from the app.
		// In such case the app is supposed to behave as a good citizen
		// and minimize its resource usage and related battery drain.
		// The SDL Android port already takes care of halting the Java
		// part of the sound processing. The Display class makes sure that it wont try
		// to render anything to the (temporary missing) graphics resources but the
		// main emulation should also be temporary stopped, in order to minimize CPU usage
		auto& focusEvent = checked_cast<const FocusEvent&>(*event);
		if (focusEvent.getGain()) {
			unblock();
		} else {
			block();
		}
#endif
	} else if (type == OPENMSX_DELETE_BOARDS) {
		// Doesn't really matter which one we delete, just that we do
		// one per event.
		assert(!garbageBoards.empty());
		garbageBoards.pop_back();
	} else {
		UNREACHABLE; // we didn't subscribe to this event...
	}
	return 0;
}


// class ExitCommand

ExitCommand::ExitCommand(CommandController& commandController_,
                         EventDistributor& distributor_)
	: Command(commandController_, "exit")
	, distributor(distributor_)
{
}

void ExitCommand::execute(span<const TclObject> tokens, TclObject& /*result*/)
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
	distributor.distributeEvent(make_shared<QuitEvent>());
}

string ExitCommand::help(const vector<string>& /*tokens*/) const
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

void MachineCommand::execute(span<const TclObject> tokens, TclObject& result)
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

string MachineCommand::help(const vector<string>& /*tokens*/) const
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

void TestMachineCommand::execute(span<const TclObject> tokens,
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

string TestMachineCommand::help(const vector<string>& /*tokens*/) const
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

void CreateMachineCommand::execute(span<const TclObject> tokens, TclObject& result)
{
	checkNumArgs(tokens, 1, Prefix{1}, nullptr);
	auto newBoard = reactor.createEmptyMotherBoard();
	result = newBoard->getMachineID();
	reactor.boards.push_back(move(newBoard));
}

string CreateMachineCommand::help(const vector<string>& /*tokens*/) const
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

void DeleteMachineCommand::execute(span<const TclObject> tokens,
                                   TclObject& /*result*/)
{
	checkNumArgs(tokens, 2, "id");
	reactor.deleteBoard(&reactor.getMachine(tokens[1].getString()));
}

string DeleteMachineCommand::help(const vector<string>& /*tokens*/) const
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

void ListMachinesCommand::execute(span<const TclObject> /*tokens*/,
                                  TclObject& result)
{
	result.addListElements(reactor.getMachineIDs());
}

string ListMachinesCommand::help(const vector<string>& /*tokens*/) const
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

void ActivateMachineCommand::execute(span<const TclObject> tokens,
                                     TclObject& result)
{
	checkNumArgs(tokens, Between{1, 2}, Prefix{1}, "id");
	switch (tokens.size()) {
	case 1:
		break;
	case 2:
		reactor.switchBoard(&reactor.getMachine(tokens[1].getString()));
		break;
	}
	result = reactor.getMachineID();
}

string ActivateMachineCommand::help(const vector<string>& /*tokens*/) const
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

void StoreMachineCommand::execute(span<const TclObject> tokens, TclObject& result)
{
	checkNumArgs(tokens, Between{1, 3}, Prefix{1}, "?id? ?filename?");
	string filename;
	string_view machineID;
	switch (tokens.size()) {
	case 1:
		machineID = reactor.getMachineID();
		filename = FileOperations::getNextNumberedFileName("savestates", "openmsxstate", ".xml.gz");
		break;
	case 2:
		machineID = tokens[1].getString();
		filename = FileOperations::getNextNumberedFileName("savestates", "openmsxstate", ".xml.gz");
		break;
	case 3:
		machineID = tokens[1].getString();
		filename = tokens[2].getString();
		break;
	}

	auto& board = reactor.getMachine(machineID);

	XmlOutputArchive out(filename);
	out.serialize("machine", board);
	out.close();
	result = filename;
}

string StoreMachineCommand::help(const vector<string>& /*tokens*/) const
{
	return
		"store_machine                       Save state of current machine to file \"openmsxNNNN.xml.gz\"\n"
		"store_machine machineID             Save state of machine \"machineID\" to file \"openmsxNNNN.xml.gz\"\n"
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

void RestoreMachineCommand::execute(span<const TclObject> tokens,
                                    TclObject& result)
{
	checkNumArgs(tokens, Between{1, 2}, Prefix{1}, "?filename?");
	auto newBoard = reactor.createEmptyMotherBoard();

	string filename;
	switch (tokens.size()) {
	case 1: {
		// load last saved entry
		string lastEntry;
		time_t lastTime = 0;
		foreach_file(
			FileOperations::getUserOpenMSXDir() + "/savestates",
			[&](const string& path, const FileOperations::Stat& st) {
				time_t modTime = st.st_mtime;
				if (modTime > lastTime) {
					filename = path;
					lastTime = modTime;
				}
			});
		if (filename.empty()) {
			throw CommandException("Can't find last saved state.");
		}
		break;
	}
	case 2:
		filename = FileOperations::expandTilde(string(tokens[1].getString()));
		break;
	}

	//std::cerr << "Loading " << filename << '\n';
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
	reactor.boards.push_back(move(newBoard));
}

string RestoreMachineCommand::help(const vector<string>& /*tokens*/) const
{
	return "restore_machine                       Load state from last saved state in default directory\n"
	       "restore_machine <filename>            Load state from indicated file\n"
	       "\n"
	       "This is a low-level command, the 'loadstate' script is easier to use.";
}

void RestoreMachineCommand::tabCompletion(vector<string>& tokens) const
{
	// TODO: add the default files (state files in user's savestates dir)
	completeFileName(tokens, userFileContext());
}


// class GetClipboardCommand

GetClipboardCommand::GetClipboardCommand(CommandController& commandController_)
	: Command(commandController_, "get_clipboard_text")
{
}

void GetClipboardCommand::execute(span<const TclObject> tokens, TclObject& result)
{
	checkNumArgs(tokens, 1, Prefix{1}, nullptr);
	if (char* text = SDL_GetClipboardText()) {
		result = text;
		SDL_free(text);
	}
}

string GetClipboardCommand::help(const vector<string>& /*tokens*/) const
{
	return "Returns the (text) content of the clipboard as a string.";
}


// class SetClipboardCommand

SetClipboardCommand::SetClipboardCommand(CommandController& commandController_)
	: Command(commandController_, "set_clipboard_text")
{
}

void SetClipboardCommand::execute(span<const TclObject> tokens, TclObject& /*result*/)
{
	checkNumArgs(tokens, 2, "text");
	auto text = tokens[1].getString();
	if (SDL_SetClipboardText(text.c_str()) != 0) {
		const char* err = SDL_GetError();
		SDL_ClearError();
		throw CommandException(err);
	}
}

string SetClipboardCommand::help(const vector<string>& /*tokens*/) const
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

void ConfigInfo::execute(span<const TclObject> tokens, TclObject& result) const
{
	// TODO make meta info available through this info topic
	switch (tokens.size()) {
	case 2: {
		result.addListElements(Reactor::getHwConfigs(configName));
		break;
	}
	case 3: {
		try {
			auto config = HardwareConfig::loadConfig(
				configName, tokens[2].getString());
			if (auto* info = config.findChild("info")) {
				for (const auto& i : info->getChildren()) {
					result.addDictKeyValue(i.getName(), i.getData());
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

string ConfigInfo::help(const vector<string>& /*tokens*/) const
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

void RealTimeInfo::execute(span<const TclObject> /*tokens*/,
                           TclObject& result) const
{
	auto delta = Timer::getTime() - reference;
	result = delta / 1000000.0;
}

string RealTimeInfo::help(const vector<string>& /*tokens*/) const
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
	span<const TclObject> tokens, TclObject& result) const
{
	if (tokens.size() != 3) {
		throw CommandException("Wrong number of parameters");
	}

	Sha1Sum sha1sum(tokens[2].getString());
	auto& romDatabase = reactor.getSoftwareDatabase();
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

string SoftwareInfoTopic::help(const vector<string>& /*tokens*/) const
{
	return "Returns information about the software "
	       "given its sha1sum, in a paired list.";
}

} // namespace openmsx
