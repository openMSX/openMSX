// $Id$

#include "Reactor.hh"
#include "CommandLineParser.hh"
#include "EventDistributor.hh"
#include "GlobalCommandController.hh"
#include "CommandConsole.hh"
#include "InputEventGenerator.hh"
#include "DiskManipulator.hh"
#include "FilePool.hh"
#include "UserSettings.hh"
#include "MSXMotherBoard.hh"
#include "Command.hh"
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
#include <cassert>
#include <memory>
#include <sys/stat.h>
#include <iostream>

using std::string;
using std::vector;
using std::set;

namespace openmsx {

class QuitCommand : public SimpleCommand
{
public:
	QuitCommand(CommandController& commandController, Reactor& reactor);
	virtual string execute(const vector<string>& tokens);
	virtual string help(const vector<string>& tokens) const;
private:
	Reactor& reactor;
};

class MachineCommand : public SimpleCommand
{
public:
	MachineCommand(CommandController& commandController, Reactor& reactor);
	virtual string execute(const vector<string>& tokens);
	virtual string help(const vector<string>& tokens) const;
	virtual void tabCompletion(vector<string>& tokens) const;
private:
	Reactor& reactor;
};

class TestMachineCommand : public SimpleCommand
{
public:
	TestMachineCommand(CommandController& commandController, Reactor& reactor);
	virtual string execute(const vector<string>& tokens);
	virtual string help(const vector<string>& tokens) const;
	virtual void tabCompletion(vector<string>& tokens) const;
private:
	Reactor& reactor;
};

class CreateMachineCommand : public SimpleCommand
{
public:
	CreateMachineCommand(CommandController& commandController, Reactor& reactor);
	virtual string execute(const vector<string>& tokens);
	virtual string help(const vector<string>& tokens) const;
private:
	Reactor& reactor;
};

class DeleteMachineCommand : public SimpleCommand
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

class ActivateMachineCommand : public SimpleCommand
{
public:
	ActivateMachineCommand(CommandController& commandController, Reactor& reactor);
	virtual string execute(const vector<string>& tokens);
	virtual string help(const vector<string>& tokens) const;
	virtual void tabCompletion(vector<string>& tokens) const;
private:
	Reactor& reactor;
};

class StoreMachineCommand : public SimpleCommand
{
public:
	StoreMachineCommand(CommandController& commandController, Reactor& reactor);
	virtual string execute(const vector<string>& tokens);
	virtual string help(const vector<string>& tokens) const;
	virtual void tabCompletion(vector<string>& tokens) const;
private:
	Reactor& reactor;
};

class RestoreMachineCommand : public SimpleCommand
{
public:
	RestoreMachineCommand(CommandController& commandController, Reactor& reactor);
	virtual string execute(const vector<string>& tokens);
	virtual string help(const vector<string>& tokens) const;
	virtual void tabCompletion(vector<string>& tokens) const;
private:
	Reactor& reactor;
};

class StoreMachineMemCommand : public SimpleCommand
{
public:
	StoreMachineMemCommand(CommandController& commandController, Reactor& reactor);
	virtual string execute(const vector<string>& tokens);
	virtual string help(const vector<string>& tokens) const;
	virtual void tabCompletion(vector<string>& tokens) const;
private:
	Reactor& reactor;
};

class RestoreMachineMemCommand : public SimpleCommand
{
public:
	RestoreMachineMemCommand(CommandController& commandController, Reactor& reactor);
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
	~PollEventGenerator();
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
	: pauseSetting(getGlobalSettings().getPauseSetting())
	, userSettings(new UserSettings(getCommandController()))
	, quitCommand(new QuitCommand(getCommandController(), *this))
	, machineCommand(new MachineCommand(getCommandController(), *this))
	, testMachineCommand(new TestMachineCommand(getCommandController(), *this))
	, createMachineCommand(new CreateMachineCommand(getCommandController(), *this))
	, deleteMachineCommand(new DeleteMachineCommand(getCommandController(), *this))
	, listMachinesCommand(new ListMachinesCommand(getCommandController(), *this))
	, activateMachineCommand(new ActivateMachineCommand(getCommandController(), *this))
	, storeMachineCommand(new StoreMachineCommand(getCommandController(), *this))
	, restoreMachineCommand(new RestoreMachineCommand(getCommandController(), *this))
	, storeMachineMemCommand(new StoreMachineMemCommand(getCommandController(), *this))
	, restoreMachineMemCommand(new RestoreMachineMemCommand(getCommandController(), *this))
	, aviRecordCommand(new AviRecorder(*this))
	, extensionInfo(new ConfigInfo(getOpenMSXInfoCommand(), "extensions"))
	, machineInfo  (new ConfigInfo(getOpenMSXInfoCommand(), "machines"))
	, realTimeInfo(new RealTimeInfo(getOpenMSXInfoCommand()))
	, mbSem(1)
	, blockedCounter(0)
	, paused(false)
	, running(true)
{
	createMachineSetting();

	pauseSetting.attach(*this);

	getEventDistributor().registerEventListener(
		OPENMSX_QUIT_EVENT, *this);
}

Reactor::~Reactor()
{
	deleteBoard(activeBoard);

	getEventDistributor().unregisterEventListener(
		OPENMSX_QUIT_EVENT, *this);

	pauseSetting.detach(*this);
}

EventDistributor& Reactor::getEventDistributor()
{
	if (!eventDistributor.get()) {
		eventDistributor.reset(new EventDistributor(*this));
	}
	return *eventDistributor;
}

GlobalCommandController& Reactor::getGlobalCommandController()
{
	if (!globalCommandController.get()) {
		globalCommandController.reset(new GlobalCommandController(
			getEventDistributor(), *this));
	}
	return *globalCommandController;
}

GlobalCliComm& Reactor::getGlobalCliComm()
{
	if (!globalCliComm.get()) {
		globalCliComm.reset(new GlobalCliComm(
			getGlobalCommandController(), getEventDistributor()));
	}
	return *globalCliComm;
}

CliComm& Reactor::getCliComm()
{
	return getGlobalCliComm();
}

InputEventGenerator& Reactor::getInputEventGenerator()
{
	if (!inputEventGenerator.get()) {
		inputEventGenerator.reset(new InputEventGenerator(
			getCommandController(), getEventDistributor()));
	}
	return *inputEventGenerator;
}

Display& Reactor::getDisplay()
{
	if (!display.get()) {
		display.reset(new Display(*this));
		display->createVideoSystem();
	}
	return *display;
}

Mixer& Reactor::getMixer()
{
	if (!mixer.get()) {
		mixer.reset(new Mixer(getCommandController()));
	}
	return *mixer;
}

CommandConsole& Reactor::getCommandConsole()
{
	if (!commandConsole.get()) {
		commandConsole.reset(new CommandConsole(
			getCommandController(), getEventDistributor(),
			getDisplay()));
	}
	return *commandConsole;
}

DiskManipulator& Reactor::getDiskManipulator()
{
	if (!diskManipulator.get()) {
		diskManipulator.reset(new DiskManipulator(
			getCommandController()));
	}
	return *diskManipulator;
}

FilePool& Reactor::getFilePool()
{
	if (!filePool.get()) {
		filePool.reset(new FilePool(
			getCommandController().getSettingsConfig()));
	}
	return *filePool;
}

EnumSetting<int>& Reactor::getMachineSetting()
{
	return *machineSetting;
}

GlobalSettings& Reactor::getGlobalSettings()
{
	return getCommandController().getGlobalSettings();
}

CommandController& Reactor::getCommandController()
{
	return getGlobalCommandController();
}

InfoCommand& Reactor::getOpenMSXInfoCommand()
{
	return getGlobalCommandController().getOpenMSXInfoCommand();
}

void Reactor::getHwConfigs(const string& type, set<string>& result)
{
	SystemFileContext context;
	CommandController* controller = NULL; // ok for SystemFileContext
	vector<string> paths = context.getPaths(*controller);
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
		getCommandController(), "default_machine",
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

void Reactor::switchMachine(const string& machine)
{
	// create+load new machine
	// switch to new machine
	// delete old active machine

	assert(Thread::isMainThread());
	// Note: loadMachine can throw an exception and in that case the
	//       motherboard must be considered as not created at all.
	Board newBoard(new MSXMotherBoard(*this));
	newBoard->loadMachine(machine);
	boards.push_back(newBoard);

	Board oldBoard = activeBoard;
	switchBoard(newBoard);
	deleteBoard(oldBoard);
}

void Reactor::switchBoard(Board newBoard)
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
	getEventDistributor().distributeEvent(
		new SimpleEvent<OPENMSX_MACHINE_LOADED_EVENT>());
	getGlobalCliComm().update(CliComm::HARDWARE, getMachineID(), "select");
	if (activeBoard.get()) {
		activeBoard->activate(true);
	}
}

void Reactor::deleteBoard(Board board)
{
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
	Display& display = getDisplay();
	GlobalCommandController& commandController = getGlobalCommandController();
	getDiskManipulator(); // make sure it gets instantiated
	                      // (also on machines without disk drive)

	// execute init.tcl
	try {
		PreferSystemFileContext context;
		commandController.source(
			context.resolve(commandController, "init.tcl"));
	} catch (FileException& e) {
		// no init.tcl, ignore
	}

	// execute startup scripts
	const CommandLineParser::Scripts& scripts = parser.getStartupScripts();
	for (CommandLineParser::Scripts::const_iterator it = scripts.begin();
	     it != scripts.end(); ++it) {
		try {
			UserFileContext context;
			commandController.source(
				context.resolve(commandController, *it));
		} catch (FileException& e) {
			throw FatalError("Couldn't execute script: " +
			                 e.getMessage());
		}
	}

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

	PollEventGenerator pollEventGenerator(getEventDistributor());

	while (running) {
		garbageBoards.clear(); // see deleteBoard()
		getEventDistributor().deliverEvents();
		MSXMotherBoard* motherboard = activeBoard.get();
		bool blocked = (blockedCounter > 0) || !motherboard;
		if (!blocked) blocked = !motherboard->execute();
		if (blocked) {
			display.repaint();
			getEventDistributor().sleep(100 * 1000);
		}
	}
}

void Reactor::unpause()
{
	if (paused) {
		paused = false;
		getGlobalCliComm().update(CliComm::STATUS, "paused", "false");
		unblock();
	}
}

void Reactor::pause()
{
	if (!paused) {
		paused = true;
		getGlobalCliComm().update(CliComm::STATUS, "paused", "true");
		block();
	}
}

void Reactor::block()
{
	++blockedCounter;
	enterMainLoop();
}

void Reactor::unblock()
{
	--blockedCounter;
	assert(blockedCounter >= 0);
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
bool Reactor::signalEvent(shared_ptr<const Event> event)
{
	if (event->getType() == OPENMSX_QUIT_EVENT) {
		getCommandController().executeCommand("exit");
	} else {
		assert(false);
	}
	return true;
}


// class QuitCommand
// TODO: Unify QuitCommand and OPENMSX_QUIT_EVENT.

QuitCommand::QuitCommand(CommandController& commandController,
                         Reactor& reactor_)
	: SimpleCommand(commandController, "exit")
	, reactor(reactor_)
{
}

string QuitCommand::execute(const vector<string>& /*tokens*/)
{
	reactor.enterMainLoop();
	reactor.running = false;
	return "";
}

string QuitCommand::help(const vector<string>& /*tokens*/) const
{
	return "Use this command to stop the emulator\n";
}


// class MachineCommand

MachineCommand::MachineCommand(CommandController& commandController,
                               Reactor& reactor_)
	: SimpleCommand(commandController, "machine")
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
	: SimpleCommand(commandController, "test_machine")
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
	: SimpleCommand(commandController, "create_machine")
	, reactor(reactor_)
{
}

string CreateMachineCommand::execute(const vector<string>& tokens)
{
	if (tokens.size() != 1) {
		throw SyntaxError();
	}
	Reactor::Board newBoard(new MSXMotherBoard(reactor));
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
	       "seperate commands is that sometimes you already want to know "
	       "the ID of the machine before load_machine starts emitting "
	       "events for this machine.";
}


// class DeleteMachineCommand

DeleteMachineCommand::DeleteMachineCommand(
	CommandController& commandController, Reactor& reactor_)
	: SimpleCommand(commandController, "delete_machine")
	, reactor(reactor_)
{
}

string DeleteMachineCommand::execute(const vector<string>& tokens)
{
	if (tokens.size() != 2) {
		throw SyntaxError();
	}
	Reactor::Board board = reactor.getMachine(tokens[1]);
	reactor.deleteBoard(board);
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
	: SimpleCommand(commandController, "activate_machine")
	, reactor(reactor_)
{
}

string ActivateMachineCommand::execute(const vector<string>& tokens)
{
	switch (tokens.size()) {
	case 1:
		return reactor.getMachineID();
	case 2: {
		Reactor::Board board = reactor.getMachine(tokens[1]);
		reactor.switchBoard(board);
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
	: SimpleCommand(commandController, "store_machine")
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
	: SimpleCommand(commandController, "restore_machine")
	, reactor(reactor_)
{
}

string RestoreMachineCommand::execute(const vector<string>& tokens)
{
	Reactor::Board newBoard(new MSXMotherBoard(reactor));

	string filename;
	string machineID;
	switch (tokens.size()) {
	case 1: { // add an extra scope to avoid case label jump problems
		// load last saved entry
		struct stat st;
		string dirName = FileOperations::getUserOpenMSXDir() + "/" + "savestates" + "/";
		string lastEntry = "";
		time_t lastTime = 0;
		ReadDir dir(dirName.c_str());
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
	completeFileName(getCommandController(), tokens, context, defaults);
}


// class StoreMachineMemCommand   TODO temp code

StoreMachineMemCommand::StoreMachineMemCommand(
	CommandController& commandController, Reactor& reactor_)
	: SimpleCommand(commandController, "store_machine_mem")
	, reactor(reactor_)
{
}

string StoreMachineMemCommand::execute(const vector<string>& tokens)
{
	if (tokens.size() != 2) {
		throw SyntaxError();
	}
	Reactor::Board board = reactor.getMachine(tokens[1]);

	reactor.snapshot.clear();
	MemOutputArchive out(reactor.snapshot);
	out.serialize("machine", *board);
	return StringOp::toString(reactor.snapshot.size()); // size in bytes
}

string StoreMachineMemCommand::help(const vector<string>& /*tokens*/) const
{
	return "TODO";
}

void StoreMachineMemCommand::tabCompletion(vector<string>& tokens) const
{
	set<string> ids;
	reactor.getMachineIDs(ids);
	completeString(tokens, ids);
}


// class RestoreMachineMemCommand   TODO temp code

RestoreMachineMemCommand::RestoreMachineMemCommand(
	CommandController& commandController, Reactor& reactor_)
	: SimpleCommand(commandController, "restore_machine_mem")
	, reactor(reactor_)
{
}

string RestoreMachineMemCommand::execute(const vector<string>& /*tokens*/)
{
	Reactor::Board newBoard(new MSXMotherBoard(reactor));
	MemInputArchive in(reactor.snapshot);
	in.serialize("machine", *newBoard);
	reactor.boards.push_back(newBoard);
	return newBoard->getMachineID();
}

string RestoreMachineMemCommand::help(const vector<string>& /*tokens*/) const
{
	return "TODO";
}

void RestoreMachineMemCommand::tabCompletion(vector<string>& /*tokens*/) const
{
	// TODO
}


// class PollEventGenerator

PollEventGenerator::PollEventGenerator(EventDistributor& eventDistributor_)
	: eventDistributor(eventDistributor_)
{
	schedule(20 * 1000); // 50 times per second
}

PollEventGenerator::~PollEventGenerator()
{
	cancel();
}

bool PollEventGenerator::alarm()
{
	eventDistributor.distributeEvent(new SimpleEvent<OPENMSX_POLL_EVENT>());
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
			std::auto_ptr<XMLElement> config = HardwareConfig::loadConfig(
				configName, tokens[2]->getString(), "dummy");
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
