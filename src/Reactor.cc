// $Id$

#include "Reactor.hh"
#include "CommandLineParser.hh"
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
#include "TclCallbackMessages.hh"
#include "MSXMotherBoard.hh"
#include "StateChangeDistributor.hh"
#include "Command.hh"
#include "AfterCommand.hh"
#include "CommandException.hh"
#include "GlobalCliComm.hh"
#include "InfoTopic.hh"
#include "Display.hh"
#include "Mixer.hh"
#include "AviRecorder.hh"
#include "Alarm.hh"
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
#include "ReadDir.hh"
#include "Thread.hh"
#include "Timer.hh"
#include "serialize.hh"
#include "openmsx.hh"
#include "checked_cast.hh"
#include "statp.hh"
#include "unreachable.hh"
#include <cassert>
#include <memory>
#include <iostream>

using std::string;
using std::vector;
using std::set;

namespace openmsx {

class QuitCommand : public Command
{
public:
	QuitCommand(CommandController& commandController, EventDistributor& distributor);
	virtual string execute(const vector<string>& tokens);
	virtual string help(const vector<string>& tokens) const;
private:
	EventDistributor& distributor;
};

class MachineCommand : public Command
{
public:
	MachineCommand(CommandController& commandController, Reactor& reactor);
	virtual string execute(const vector<string>& tokens);
	virtual string help(const vector<string>& tokens) const;
	virtual void tabCompletion(vector<string>& tokens) const;
private:
	Reactor& reactor;
};

class TestMachineCommand : public Command
{
public:
	TestMachineCommand(CommandController& commandController, Reactor& reactor);
	virtual string execute(const vector<string>& tokens);
	virtual string help(const vector<string>& tokens) const;
	virtual void tabCompletion(vector<string>& tokens) const;
private:
	Reactor& reactor;
};

class CreateMachineCommand : public Command
{
public:
	CreateMachineCommand(CommandController& commandController, Reactor& reactor);
	virtual string execute(const vector<string>& tokens);
	virtual string help(const vector<string>& tokens) const;
private:
	Reactor& reactor;
};

class DeleteMachineCommand : public Command
{
public:
	DeleteMachineCommand(CommandController& commandController, Reactor& reactor);
	virtual string execute(const vector<string>& tokens);
	virtual string help(const vector<string>& tokens) const;
	virtual void tabCompletion(vector<string>& tokens) const;
private:
	Reactor& reactor;
};

class ListMachinesCommand : public Command
{
public:
	ListMachinesCommand(CommandController& commandController, Reactor& reactor);
	virtual void execute(const vector<TclObject*>& tokens,
	                     TclObject& result);
	virtual string help(const vector<string>& tokens) const;
private:
	Reactor& reactor;
};

class ActivateMachineCommand : public Command
{
public:
	ActivateMachineCommand(CommandController& commandController, Reactor& reactor);
	virtual string execute(const vector<string>& tokens);
	virtual string help(const vector<string>& tokens) const;
	virtual void tabCompletion(vector<string>& tokens) const;
private:
	Reactor& reactor;
};

class StoreMachineCommand : public Command
{
public:
	StoreMachineCommand(CommandController& commandController, Reactor& reactor);
	virtual string execute(const vector<string>& tokens);
	virtual string help(const vector<string>& tokens) const;
	virtual void tabCompletion(vector<string>& tokens) const;
private:
	Reactor& reactor;
};

class RestoreMachineCommand : public Command
{
public:
	RestoreMachineCommand(CommandController& commandController, Reactor& reactor);
	virtual string execute(const vector<string>& tokens);
	virtual string help(const vector<string>& tokens) const;
	virtual void tabCompletion(vector<string>& tokens) const;
private:
	Reactor& reactor;
};

class PollEventGenerator : private Alarm
{
public:
	explicit PollEventGenerator(EventDistributor& eventDistributor);
	virtual ~PollEventGenerator();
private:
	virtual bool alarm();
	EventDistributor& eventDistributor;
};

class ConfigInfo : public InfoTopic
{
public:
	ConfigInfo(InfoCommand& openMSXInfoCommand, const string& configName);
	virtual void execute(const vector<TclObject*>& tokens,
	                     TclObject& result) const;
	virtual string help(const vector<string>& tokens) const;
	virtual void tabCompletion(vector<string>& tokens) const;
private:
	const string configName;
};

class RealTimeInfo : public InfoTopic
{
public:
	RealTimeInfo(InfoCommand& openMSXInfoCommand);
	virtual void execute(const vector<TclObject*>& tokens,
	                     TclObject& result) const;
	virtual string help(const vector<string>& tokens) const;
private:
	const unsigned long long reference;
};


Reactor::Reactor()
	: mbSem(1)
	, eventDistributor(new EventDistributor(*this))
	, globalCliComm(new GlobalCliComm())
	, globalCommandController(new GlobalCommandController(
		*eventDistributor, *globalCliComm, *this))
	, globalSettings(new GlobalSettings(*globalCommandController))
	, inputEventGenerator(new InputEventGenerator(
		*globalCommandController, *eventDistributor))
	, mixer(new Mixer(*this, *globalCommandController))
	, diskFactory(new DiskFactory(*this))
	, diskManipulator(new DiskManipulator(*globalCommandController, *this))
	, virtualDrive(new DiskChanger("virtual_drive", *globalCommandController,
	                               *diskFactory, *diskManipulator, true))
	, filePool(new FilePool(*globalCommandController, *eventDistributor))
	, pauseSetting(getGlobalSettings().getPauseSetting())
	, pauseOnLostFocusSetting(getGlobalSettings().getPauseOnLostFocusSetting())
	, userSettings(new UserSettings(*globalCommandController))
	, softwareDatabase(new RomDatabase(*globalCommandController, *globalCliComm))
	, afterCommand(new AfterCommand(*this, *eventDistributor,
	                                *globalCommandController))
	, quitCommand(new QuitCommand(*globalCommandController, *eventDistributor))
	, machineCommand(new MachineCommand(*globalCommandController, *this))
	, testMachineCommand(new TestMachineCommand(*globalCommandController, *this))
	, createMachineCommand(new CreateMachineCommand(*globalCommandController, *this))
	, deleteMachineCommand(new DeleteMachineCommand(*globalCommandController, *this))
	, listMachinesCommand(new ListMachinesCommand(*globalCommandController, *this))
	, activateMachineCommand(new ActivateMachineCommand(*globalCommandController, *this))
	, storeMachineCommand(new StoreMachineCommand(*globalCommandController, *this))
	, restoreMachineCommand(new RestoreMachineCommand(*globalCommandController, *this))
	, aviRecordCommand(new AviRecorder(*this))
	, extensionInfo(new ConfigInfo(getOpenMSXInfoCommand(), "extensions"))
	, machineInfo  (new ConfigInfo(getOpenMSXInfoCommand(), "machines"))
	, realTimeInfo(new RealTimeInfo(getOpenMSXInfoCommand()))
	, tclCallbackMessages(new TclCallbackMessages(*globalCliComm,
	                                              *globalCommandController))
	, blockedCounter(0)
	, paused(false)
	, running(true)
{
	createMachineSetting();

	pauseSetting.attach(*this);

	eventDistributor->registerEventListener(OPENMSX_QUIT_EVENT, *this);
	eventDistributor->registerEventListener(OPENMSX_FOCUS_EVENT, *this);
	eventDistributor->registerEventListener(OPENMSX_DELETE_BOARDS, *this);
}

Reactor::~Reactor()
{
	deleteBoard(activeBoard);

	eventDistributor->unregisterEventListener(OPENMSX_QUIT_EVENT, *this);
	eventDistributor->unregisterEventListener(OPENMSX_FOCUS_EVENT, *this);
	eventDistributor->unregisterEventListener(OPENMSX_DELETE_BOARDS, *this);

	pauseSetting.detach(*this);
}

EventDistributor& Reactor::getEventDistributor()
{
	return *eventDistributor;
}

GlobalCommandController& Reactor::getGlobalCommandController()
{
	return *globalCommandController;
}

GlobalCliComm& Reactor::getGlobalCliComm()
{
	return *globalCliComm;
}

CliComm& Reactor::getCliComm()
{
	return *globalCliComm;
}

InputEventGenerator& Reactor::getInputEventGenerator()
{
	return *inputEventGenerator;
}

Display& Reactor::getDisplay()
{
	assert(display.get());
	return *display;
}

RomDatabase& Reactor::getSoftwareDatabase()
{
	return *softwareDatabase;
}

Mixer& Reactor::getMixer()
{
	return *mixer;
}

DiskFactory& Reactor::getDiskFactory()
{
	return *diskFactory;
}

FilePool& Reactor::getFilePool()
{
	return *filePool;
}

DiskManipulator& Reactor::getDiskManipulator()
{
	return *diskManipulator;
}

EnumSetting<int>& Reactor::getMachineSetting()
{
	return *machineSetting;
}

GlobalSettings& Reactor::getGlobalSettings()
{
	return *globalSettings;
}

CommandController& Reactor::getCommandController()
{
	return *globalCommandController;
}

InfoCommand& Reactor::getOpenMSXInfoCommand()
{
	return globalCommandController->getOpenMSXInfoCommand();
}

void Reactor::getHwConfigs(const string& type, set<string>& result)
{
	SystemFileContext context;
	vector<string> paths = context.getPaths();
	for (vector<string>::const_iterator it = paths.begin();
	     it != paths.end(); ++it) {
		string path = FileOperations::join(*it, type);
		ReadDir configsDir(path);
		while (dirent* d = configsDir.getEntry()) {
			string name = d->d_name;
			string dir = FileOperations::join(path, name);
			string config = FileOperations::join(dir, "hardwareconfig.xml");
			if (FileOperations::isDirectory(dir) &&
			    FileOperations::isRegularFile(config)) {
				result.insert(name);
			}
		}
	}
}

void Reactor::createMachineSetting()
{
	EnumSetting<int>::Map machines; // int's are unique dummy values
	int count = 1;
	set<string> names;
	getHwConfigs("machines", names);
	for (set<string>::const_iterator it = names.begin();
	     it != names.end(); ++it) {
		machines[*it] = count++;
	}
	machines["C-BIOS_MSX2+"] = 0; // default machine

	machineSetting.reset(new EnumSetting<int>(
		*globalCommandController, "default_machine",
		"default machine (takes effect next time openMSX is started)",
		0, machines));
}

MSXMotherBoard* Reactor::getMotherBoard() const
{
	assert(Thread::isMainThread());
	return activeBoard.get();
}

string Reactor::getMachineID() const
{
	return activeBoard.get() ? activeBoard->getMachineID() : "";
}

void Reactor::getMachineIDs(set<string>& result) const
{
	for (Reactor::Boards::const_iterator it = boards.begin();
	     it != boards.end(); ++it) {
		result.insert((*it)->getMachineID());
	}
}

Reactor::Board Reactor::getMachine(const string& machineID) const
{
	for (Boards::const_iterator it = boards.begin();
	     it != boards.end(); ++it) {
		if ((*it)->getMachineID() == machineID) {
			return *it;
		}
	}
	throw CommandException("No machine with ID: " + machineID);
}

Reactor::Board Reactor::createEmptyMotherBoard()
{
	return Board(new MSXMotherBoard(*this));
}

void Reactor::replaceBoard(MSXMotherBoard& oldBoard_, const Board& newBoard)
{
	assert(Thread::isMainThread());

	// Add new board.
	boards.push_back(newBoard);

	// Lookup old board (it must be present).
	Boards::iterator it = boards.begin();
	while (it->get() != &oldBoard_) {
		++it;
		assert(it != boards.end());
	}

	// If the old board was the active board, then activate the new board
	if (*it == activeBoard) {
		switchBoard(newBoard);
	}

	// Remove (=delete) the old board.
	// Note that we don't use the 'garbageBoards' mechanism as used in
	// deleteBoard(). This means oldBoard cannot be used  anymore right
	// after this method returns.
	boards.erase(it);
}

void Reactor::switchMachine(const string& machine)
{
	if (!display.get()) {
		display.reset(new Display(*this));
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
	Board newBoard = createEmptyMotherBoard();
	newBoard->loadMachine(machine);
	boards.push_back(newBoard);

	Board oldBoard = activeBoard;
	switchBoard(newBoard);
	deleteBoard(oldBoard);
}

void Reactor::switchBoard(const Board& newBoard)
{
	assert(Thread::isMainThread());
	assert(!newBoard.get() ||
	       (find(boards.begin(), boards.end(), newBoard) != boards.end()));
	assert(!activeBoard.get() ||
	       (find(boards.begin(), boards.end(), activeBoard) != boards.end()));
	if (activeBoard.get()) {
		activeBoard->activate(false);
	}
	{
		// Don't hold the lock for longer than the actual switch.
		// In the past we had a potential for deadlocks here, because
		// (indirectly) the code below still acquires other locks.
		ScopedLock lock(mbSem);
		activeBoard = newBoard;
	}
	eventDistributor->distributeEvent(
		new SimpleEvent(OPENMSX_MACHINE_LOADED_EVENT));
	globalCliComm->update(CliComm::HARDWARE, getMachineID(), "select");
	if (activeBoard.get()) {
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
	if (!board.get()) return;

	if (board == activeBoard) {
		// delete active board -> there is no active board anymore
		switchBoard(Reactor::Board(NULL));
	}
	Boards::iterator it = find(boards.begin(), boards.end(), board);
	assert(it != boards.end());
	boards.erase(it);
	// Don't immediately delete old boards because it's possible this
	// routine is called via a code path that goes through the old
	// board. Instead remember this board and delete it at a safe moment
	// in time.
	garbageBoards.push_back(board);
	eventDistributor->distributeEvent(
		new SimpleEvent(OPENMSX_DELETE_BOARDS));
}

void Reactor::enterMainLoop()
{
	// Note: this method can get called from different threads
	ScopedLock lock;
	if (!Thread::isMainThread()) {
		// Don't take lock in main thread to avoid recursive locking.
		lock.take(mbSem);
	}
	if (activeBoard.get()) {
		activeBoard->exitCPULoopAsync();
	}
}

void Reactor::run(CommandLineParser& parser)
{
	GlobalCommandController& commandController = *globalCommandController;

	PRT_DEBUG("Reactor::run Trying to execute init.tcl...");

	// execute init.tcl
	try {
		PreferSystemFileContext context;
		commandController.source(context.resolve("init.tcl"));
	} catch (FileException&) {
		// no init.tcl, ignore
	}

	PRT_DEBUG("Reactor::run Executing startup scripts...");
	// execute startup scripts
	const CommandLineParser::Scripts& scripts = parser.getStartupScripts();
	for (CommandLineParser::Scripts::const_iterator it = scripts.begin();
	     it != scripts.end(); ++it) {
		PRT_DEBUG("Reactor::run Executing startup script..." << *it);
		try {
			UserFileContext context;
			commandController.source(context.resolve(*it));
		} catch (FileException& e) {
			throw FatalError("Couldn't execute script: " +
			                 e.getMessage());
		}
	}

	PRT_DEBUG("Reactor::run Powering up active board...");
	// Run
	if (parser.getParseStatus() == CommandLineParser::RUN) {
		// don't use Tcl to power up the machine, we cannot pass
		// exceptions through Tcl and ADVRAM might throw in its
		// powerUp() method. Solution is to implement dependencies
		// between devices so ADVRAM can check the error condition
		// in its constructor
		//commandController.executeCommand("set power on");
		if (activeBoard.get()) {
			activeBoard->powerUp();
		}
	}

	PRT_DEBUG("Reactor::run Instantiate poll event generator...");
	PollEventGenerator pollEventGenerator(*eventDistributor);

	while (running) {
		eventDistributor->deliverEvents();
		assert(garbageBoards.empty());
		MSXMotherBoard* motherboard = activeBoard.get();
		bool blocked = (blockedCounter > 0) || !motherboard;
		if (!blocked) blocked = !motherboard->execute();
		if (blocked) {
			// At first sight a better alternative is to use the
			// SDL_WaitEvent() function. Though when inspecting
			// the implementation of that function, it turns out
			// to also use a sleep/poll loop, with even shorter
			// sleep periods as we use here. Maybe in future
			// SDL implementations this will be improved.
			eventDistributor->sleep(100 * 1000);
		}
	}
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
	mixer->mute();
}

void Reactor::unblock()
{
	--blockedCounter;
	assert(blockedCounter >= 0);
	mixer->unmute();
}


// Observer<Setting>
void Reactor::update(const Setting& setting)
{
	if (&setting == &pauseSetting) {
		if (pauseSetting.getValue()) {
			pause();
		} else {
			unpause();
		}
	}
}

// EventListener
int Reactor::signalEvent(const shared_ptr<const Event>& event)
{
	EventType type = event->getType();
	if (type == OPENMSX_QUIT_EVENT) {
		enterMainLoop();
		running = false;
	} else if (type == OPENMSX_FOCUS_EVENT) {
		if (!pauseOnLostFocusSetting.getValue()) return 0;
		const FocusEvent& focusEvent = checked_cast<const FocusEvent&>(*event);
		if (focusEvent.getGain()) {
			// gained focus
			if (!pauseSetting.getValue()) {
				unpause();
			}
		} else {
			// lost focus
			pause();
		}
	} else if (type == OPENMSX_DELETE_BOARDS) {
		assert(!garbageBoards.empty());
		garbageBoards.erase(garbageBoards.begin());
	} else {
		UNREACHABLE; // we didn't subscribe to this event...
	}
	return 0;
}


// class QuitCommand

QuitCommand::QuitCommand(CommandController& commandController,
                         EventDistributor& distributor_)
	: Command(commandController, "exit")
	, distributor(distributor_)
{
}

string QuitCommand::execute(const vector<string>& /*tokens*/)
{
	distributor.distributeEvent(new QuitEvent());
	return "";
}

string QuitCommand::help(const vector<string>& /*tokens*/) const
{
	return "Use this command to stop the emulator\n";
}


// class MachineCommand

MachineCommand::MachineCommand(CommandController& commandController,
                               Reactor& reactor_)
	: Command(commandController, "machine")
	, reactor(reactor_)
{
}

string MachineCommand::execute(const vector<string>& tokens)
{
	switch (tokens.size()) {
	case 1: // get current machine
		return reactor.getMachineID();
	case 2:
		try {
			reactor.switchMachine(tokens[1]);
		} catch (MSXException& e) {
			throw CommandException("Machine switching failed: " +
			                       e.getMessage());
		}
		return reactor.getMachineID();
	default:
		throw SyntaxError();
	}
}

string MachineCommand::help(const vector<string>& /*tokens*/) const
{
	return "Switch to a different MSX machine.";
}

void MachineCommand::tabCompletion(vector<string>& tokens) const
{
	set<string> machines;
	Reactor::getHwConfigs("machines", machines);
	completeString(tokens, machines);
}


// class TestMachineCommand

TestMachineCommand::TestMachineCommand(CommandController& commandController,
                                       Reactor& reactor_)
	: Command(commandController, "test_machine")
	, reactor(reactor_)
{
}

string TestMachineCommand::execute(const vector<string>& tokens)
{
	if (tokens.size() != 2) {
		throw SyntaxError();
	}
	try {
		MSXMotherBoard mb(reactor);
		mb.loadMachine(tokens[1]);
		return ""; // success
	} catch (MSXException& e) {
		return e.getMessage(); // error
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
	set<string> machines;
	Reactor::getHwConfigs("machines", machines);
	completeString(tokens, machines);
}


// class CreateMachineCommand

CreateMachineCommand::CreateMachineCommand(
	CommandController& commandController, Reactor& reactor_)
	: Command(commandController, "create_machine")
	, reactor(reactor_)
{
}

string CreateMachineCommand::execute(const vector<string>& tokens)
{
	if (tokens.size() != 1) {
		throw SyntaxError();
	}
	Reactor::Board newBoard = reactor.createEmptyMotherBoard();
	reactor.boards.push_back(newBoard);
	return newBoard->getMachineID();
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
	CommandController& commandController, Reactor& reactor_)
	: Command(commandController, "delete_machine")
	, reactor(reactor_)
{
}

string DeleteMachineCommand::execute(const vector<string>& tokens)
{
	if (tokens.size() != 2) {
		throw SyntaxError();
	}
	reactor.deleteBoard(reactor.getMachine(tokens[1]));
	return "";
}

string DeleteMachineCommand::help(const vector<string>& /*tokens*/) const
{
	return "Deletes the given MSX machine.";
}

void DeleteMachineCommand::tabCompletion(vector<string>& tokens) const
{
	set<string> ids;
	reactor.getMachineIDs(ids);
	completeString(tokens, ids);
}


// class ListMachinesCommand

ListMachinesCommand::ListMachinesCommand(
	CommandController& commandController, Reactor& reactor_)
	: Command(commandController, "list_machines")
	, reactor(reactor_)
{
}

void ListMachinesCommand::execute(const vector<TclObject*>& /*tokens*/,
                                  TclObject& result)
{
	set<string> ids;
	reactor.getMachineIDs(ids);
	result.addListElements(ids.begin(), ids.end());
}

string ListMachinesCommand::help(const vector<string>& /*tokens*/) const
{
	return "Returns a list of all machine IDs.";
}


// class ActivateMachineCommand

ActivateMachineCommand::ActivateMachineCommand(
	CommandController& commandController, Reactor& reactor_)
	: Command(commandController, "activate_machine")
	, reactor(reactor_)
{
}

string ActivateMachineCommand::execute(const vector<string>& tokens)
{
	switch (tokens.size()) {
	case 1:
		return reactor.getMachineID();
	case 2: {
		reactor.switchBoard(reactor.getMachine(tokens[1]));
		return reactor.getMachineID();
	}
	default:
		throw SyntaxError();
	}
}

string ActivateMachineCommand::help(const vector<string>& /*tokens*/) const
{
	return "Make another machine the active msx machine.\n"
	       "Or when invoked without arguments, query the ID of the "
	       "active msx machine.";
}

void ActivateMachineCommand::tabCompletion(vector<string>& tokens) const
{
	set<string> ids;
	reactor.getMachineIDs(ids);
	completeString(tokens, ids);
}


// class StoreMachineCommand

StoreMachineCommand::StoreMachineCommand(
	CommandController& commandController, Reactor& reactor_)
	: Command(commandController, "store_machine")
	, reactor(reactor_)
{
}

string StoreMachineCommand::execute(const vector<string>& tokens)
{
	string filename;
	string machineID;
	switch (tokens.size()) {
	case 1:
		machineID = reactor.getMachineID();
		filename = FileOperations::getNextNumberedFileName("savestates", "openmsxstate", ".xml.gz");
		break;
	case 2:
		machineID = tokens[1];
		filename = FileOperations::getNextNumberedFileName("savestates", "openmsxstate", ".xml.gz");
		break;
	case 3:
		machineID = tokens[1];
		filename = tokens[2];
		break;
	default:
		throw SyntaxError();
	}

	Reactor::Board board = reactor.getMachine(machineID);

	XmlOutputArchive out(filename);
	out.serialize("machine", *board);
	return filename;
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
	set<string> ids;
	reactor.getMachineIDs(ids);
	completeString(tokens, ids);
}


// class RestoreMachineCommand

RestoreMachineCommand::RestoreMachineCommand(
	CommandController& commandController, Reactor& reactor_)
	: Command(commandController, "restore_machine")
	, reactor(reactor_)
{
}

string RestoreMachineCommand::execute(const vector<string>& tokens)
{
	Reactor::Board newBoard = reactor.createEmptyMotherBoard();

	string filename;
	string machineID;
	switch (tokens.size()) {
	case 1: { // add an extra scope to avoid case label jump problems
		// load last saved entry
		struct stat st;
		string dirName = FileOperations::getUserOpenMSXDir() + "/savestates/";
		string lastEntry = "";
		time_t lastTime = 0;
		ReadDir dir(dirName);
		while (dirent* d = dir.getEntry()) {
			int res = stat((dirName + string(d->d_name)).c_str(), &st);
			if ((res == 0) && S_ISREG(st.st_mode)) {
				time_t modTime = st.st_mtime;
				if (modTime > lastTime) {
					lastEntry = string(d->d_name);
					lastTime = modTime;
				}
			}
		}
		if (lastEntry == "") {
			throw CommandException("Can't find last saved state.");
		}
		filename = dirName + lastEntry;
		}
		break;
	case 2:
		filename = tokens[1];
		break;
	default:
		throw SyntaxError();
	}

	//std::cerr << "Loading " << filename << std::endl;
	try {
		XmlInputArchive in(filename);
		in.serialize("machine", *newBoard);
	} catch (XMLException& e) {
		throw CommandException("Cannot load state, bad file format: " + e.getMessage());
	} catch (MSXException& e) {
		throw CommandException("Cannot load state: " + e.getMessage());
	}

	// Savestate also contains stuff like the keyboard state at the moment
	// the snapshot was created (this is required for reverse/replay). But
	// now we want the MSX to see the actual host keyboard state.
	newBoard->getStateChangeDistributor().stopReplay(newBoard->getCurrentTime());

	reactor.boards.push_back(newBoard);
	return newBoard->getMachineID();
}

string RestoreMachineCommand::help(const vector<string>& /*tokens*/) const
{
	return
		"restore_machine                       Load state from last saved state in default directory\n"
		"restore_machine <filename>            Load state from indicated file\n"
		"\n"
		"This is a low-level command, the 'loadstate' script is easier to use.";
}

void RestoreMachineCommand::tabCompletion(vector<string>& tokens) const
{
	set<string> defaults;
	// TODO: put the default files in defaults (state files in user's savestates dir)
	UserFileContext context;
	completeFileName(tokens, context, defaults);
}


// class PollEventGenerator

PollEventGenerator::PollEventGenerator(EventDistributor& eventDistributor_)
	: eventDistributor(eventDistributor_)
{
	schedule(20 * 1000); // 50 times per second
}

PollEventGenerator::~PollEventGenerator()
{
	prepareDelete();
}

bool PollEventGenerator::alarm()
{
	eventDistributor.distributeEvent(new SimpleEvent(OPENMSX_POLL_EVENT));
	return true; // reschedule
}


// class ConfigInfo

ConfigInfo::ConfigInfo(InfoCommand& openMSXInfoCommand,
	               const string& configName_)
	: InfoTopic(openMSXInfoCommand, configName_)
	, configName(configName_)
{
}

void ConfigInfo::execute(const vector<TclObject*>& tokens,
                         TclObject& result) const
{
	// TODO make meta info available through this info topic
	switch (tokens.size()) {
	case 2: {
		set<string> configs;
		Reactor::getHwConfigs(configName, configs);
		result.addListElements(configs.begin(), configs.end());
		break;
	}
	case 3: {
		try {
			string filename = SystemFileContext().resolve(
				FileOperations::join(
					configName, tokens[2]->getString().str(),
					"hardwareconfig.xml"));
			std::auto_ptr<XMLElement> config =
				HardwareConfig::loadConfig(filename);
			if (XMLElement* info = config->findChild("info")) {
				const XMLElement::Children& children =
					info->getChildren();
				for (XMLElement::Children::const_iterator it =
					children.begin();
				     it != children.end(); ++it) {
					result.addListElement((*it)->getName());
					result.addListElement((*it)->getData());
				}
			}
		} catch (MSXException& e) {
			throw CommandException(
				"Couldn't get config info: " + e.getMessage());
		}
		break;
	}
	default:
		throw CommandException("Too many parameters");
	}
}

string ConfigInfo::help(const vector<string>& /*tokens*/) const
{
	return "Shows a list of available " + configName + ", "
	       "or get meta information about the selected item.\n";
}

void ConfigInfo::tabCompletion(vector<string>& tokens) const
{
	set<string> configs;
	Reactor::getHwConfigs(configName, configs);
	completeString(tokens, configs);
}


// class RealTimeInfo

RealTimeInfo::RealTimeInfo(InfoCommand& openMSXInfoCommand)
	: InfoTopic(openMSXInfoCommand, "realtime")
	, reference(Timer::getTime())
{
}

void RealTimeInfo::execute(const vector<TclObject*>& /*tokens*/,
                           TclObject& result) const
{
	unsigned long long delta = Timer::getTime() - reference;
	result.setDouble(delta / 1000000.0);
}

string RealTimeInfo::help(const vector<string>& /*tokens*/) const
{
	return "Returns the time in seconds since openMSX was started.";
}

} // namespace openmsx
