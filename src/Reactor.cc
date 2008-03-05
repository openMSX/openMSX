// $Id$

#include "Reactor.hh"
#include "CommandLineParser.hh"
#include "EventDistributor.hh"
#include "GlobalCommandController.hh"
#include "CommandConsole.hh"
#include "InputEventGenerator.hh"
#include "DiskManipulator.hh"
#include "FilePool.hh"
#include "MSXMotherBoard.hh"
#include "Command.hh"
#include "GlobalCliComm.hh"
#include "InfoTopic.hh"
#include "Display.hh"
#include "Mixer.hh"
#include "IconStatus.hh"
#include "AviRecorder.hh"
#include "Alarm.hh"
#include "GlobalSettings.hh"
#include "BooleanSetting.hh"
#include "EnumSetting.hh"
#include "HardwareConfig.hh"
#include "FileContext.hh"
#include "FileException.hh"
#include "FileOperations.hh"
#include "ReadDir.hh"
#include "Thread.hh"
#include "Timer.hh"
#include <cassert>
#include <memory>

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
	: mbSem(1)
	, pauseSetting(getGlobalSettings().getPauseSetting())
	, quitCommand(new QuitCommand(getCommandController(), *this))
	, machineCommand(new MachineCommand(getCommandController(), *this))
	, testMachineCommand(new TestMachineCommand(getCommandController(), *this))
	, createMachineCommand(new CreateMachineCommand(getCommandController(), *this))
	, deleteMachineCommand(new DeleteMachineCommand(getCommandController(), *this))
	, listMachinesCommand(new ListMachinesCommand(getCommandController(), *this))
	, activateMachineCommand(new ActivateMachineCommand(getCommandController(), *this))
	, aviRecordCommand(new AviRecorder(*this))
	, extensionInfo(new ConfigInfo(
	       getGlobalCommandController().getOpenMSXInfoCommand(),
	       "extensions"))
	, machineInfo(new ConfigInfo(
	       getGlobalCommandController().getOpenMSXInfoCommand(),
	       "machines"))
	, realTimeInfo(new RealTimeInfo(
	       getGlobalCommandController().getOpenMSXInfoCommand()))
	, needSwitch(false)
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
	deleteMotherBoard();

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
			getGlobalCommandController(), getEventDistributor(),
			getDisplay()));
	}
	return *commandConsole;
}

IconStatus& Reactor::getIconStatus()
{
	if (!iconStatus.get()) {
		iconStatus.reset(new IconStatus(getEventDistributor()));
	}
	return *iconStatus;
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
			getGlobalCommandController().getSettingsConfig()));
	}
	return *filePool;
}

EnumSetting<int>& Reactor::getMachineSetting()
{
	return *machineSetting;
}

GlobalSettings& Reactor::getGlobalSettings()
{
	return getGlobalCommandController().getGlobalSettings();
}

CommandController& Reactor::getCommandController()
{
	return getGlobalCommandController();
}

void Reactor::getHwConfigs(const string& type, set<string>& result)
{
	SystemFileContext context;
	const vector<string>& paths = context.getPaths();
	for (vector<string>::const_iterator it = paths.begin();
	     it != paths.end(); ++it) {
		string path = *it + type;
		ReadDir configsDir(path);
		while (dirent* d = configsDir.getEntry()) {
			string name = d->d_name;
			string dir = path + '/' + name;
			string config = dir + "/hardwareconfig.xml";
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

// used by CommandLineParser
void Reactor::createMotherBoard(const string& machine)
{
	assert(!activeBoard.get());
	prepareMotherBoard(machine);
	switchMotherBoard();
}

MSXMotherBoard& Reactor::prepareMotherBoard(const string& machine)
{
	// Locking rules for MSXMotherBoard object access:
	//  - main thread can always access motherBoard without taking a lock
	//  - changing motherBoard handle can only be done in the main thread
	//    and needs to take the mbSem lock
	//  - non-main thread can only access motherBoard via specific
	//    member functions (atm only via enterMainLoop()), it needs to take
	//    the mbSem lock

	assert(Thread::isMainThread());
	ScopedLock lock(mbSem);
	// Note: loadMachine can throw an exception and in that case the
	//       motherboard must be considered as not created at all.
	shared_ptr<MSXMotherBoard> tmp(new MSXMotherBoard(*this));
	tmp->loadMachine(machine);
	boards.push_back(tmp);

	if (activeBoard.get()) {
		Boards::iterator it = find(boards.begin(), boards.end(),
		                           activeBoard);
		assert(it != boards.end());
		boards.erase(it); // only gets deleted after switch
	}

	prepareSwitch(tmp);
	return *tmp;
}

void Reactor::prepareSwitch(shared_ptr<MSXMotherBoard> board)
{
	assert(Thread::isMainThread());
	switchBoard = board;
	needSwitch = true;
	enterMainLoop();
}

void Reactor::switchMotherBoard()
{
	assert(Thread::isMainThread());
	assert(needSwitch);
	assert(!switchBoard.get() ||
	       find(boards.begin(), boards.end(), switchBoard) != boards.end());
	ScopedLock lock(mbSem);
	activeBoard = switchBoard;
	switchBoard.reset();
	needSwitch = false;
	getEventDistributor().distributeEvent(
		new SimpleEvent<OPENMSX_MACHINE_LOADED_EVENT>());
	string machineID = activeBoard.get() ? activeBoard->getMachineID()
	                                     : "";
	getGlobalCliComm().update(CliComm::HARDWARE, machineID, "select");
}

MSXMotherBoard* Reactor::getMotherBoard() const
{
	assert(Thread::isMainThread());
	return activeBoard.get();
}

void Reactor::deleteMotherBoard()
{
	assert(Thread::isMainThread());
	ScopedLock lock(mbSem);
	if (activeBoard.get()) {
		Boards::iterator it = find(boards.begin(), boards.end(),
		                           activeBoard);
		assert(it != boards.end());
		boards.erase(it);
		activeBoard.reset(); // also deletes board
	}
}

void Reactor::enterMainLoop()
{
	// Note: this method can get called from different threads
	ScopedLock lock;
	if (!Thread::isMainThread()) {
		// Don't take lock in main thread to avoid recursive locking.
		// This method gets called from within createMotherBoard().
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

	// select initial machine before executing scripts
	if (needSwitch) {
		switchMotherBoard();
	}

	// execute init.tcl
	try {
		SystemFileContext context(true); // only in system dir
		commandController.source(context.resolve("init.tcl"));
	} catch (FileException& e) {
		// no init.tcl, ignore
	}

	// execute startup scripts
	const CommandLineParser::Scripts& scripts = parser.getStartupScripts();
	for (CommandLineParser::Scripts::const_iterator it = scripts.begin();
	     it != scripts.end(); ++it) {
		try {
			UserFileContext context(commandController);
			commandController.source(context.resolve(*it));
		} catch (FileException& e) {
			throw FatalError("Couldn't execute script: " +
			                 e.getMessage());
		}
	}

	// Run
	if (parser.getParseStatus() == CommandLineParser::RUN) {
		// don't use TCL to power up the machine, we cannot pass
		// exceptions through TCL and ADVRAM might throw in its
		// powerUp() method. Solution is to implement dependencies
		// between devices so ADVRAM can check the error condition
		// in its constructor
		//getGlobalCommandController().executeCommand("set power on");
		MSXMotherBoard* motherboard = getMotherBoard();
		if (motherboard) {
			motherboard->powerUp();
		}
	}

	PollEventGenerator pollEventGenerator(getEventDistributor());

	while (running) {
		if (needSwitch) {
			switchMotherBoard();
			assert(!needSwitch);
		}
		getEventDistributor().deliverEvents();
		MSXMotherBoard* motherboard = getMotherBoard();
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
		getGlobalCommandController().executeCommand("exit");
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
		if (MSXMotherBoard* motherBoard = reactor.getMotherBoard()) {
			return motherBoard->getMachineID();
		} else {
			return "";
		}
	case 2:
		try {
			MSXMotherBoard& motherBoard =
				reactor.prepareMotherBoard(tokens[1]);
			return motherBoard.getMachineID();
		} catch (MSXException& e) {
			throw CommandException("Machine switching failed: " +
			                       e.getMessage());
		}
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

string CreateMachineCommand::execute(const vector<string>& /*tokens*/)
{
	shared_ptr<MSXMotherBoard> mb(new MSXMotherBoard(reactor));
	reactor.boards.push_back(mb);
	return mb->getMachineID();
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
	for (Reactor::Boards::iterator it = reactor.boards.begin();
	     it != reactor.boards.end(); ++it) {
		if ((*it)->getMachineID() == tokens[1]) {
			shared_ptr<MSXMotherBoard> mb = *it;
			if (mb == reactor.activeBoard) {
				// if this was the active board, the actual
				// delete happens after switch ...
				reactor.prepareSwitch(
					shared_ptr<MSXMotherBoard>(NULL));
			}
			// ... otherwise the delete already happens here
			reactor.boards.erase(it);
			return "";
		}
	}
	throw CommandException("No machine with ID: " + tokens[1]);
}

string DeleteMachineCommand::help(const vector<string>& /*tokens*/) const
{
	return "Deletes the given MSX machine.";
}

void DeleteMachineCommand::tabCompletion(vector<string>& tokens) const
{
	set<string> ids;
	for (Reactor::Boards::const_iterator it = reactor.boards.begin();
	     it != reactor.boards.end(); ++it) {
		ids.insert((*it)->getMachineID());
	}
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
	for (Reactor::Boards::const_iterator it = reactor.boards.begin();
	     it != reactor.boards.end(); ++it) {
		result.addListElement((*it)->getMachineID());
	}
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
		return reactor.activeBoard.get()
		     ? reactor.activeBoard->getMachineID()
		     : "";
	case 2:
		for (Reactor::Boards::iterator it = reactor.boards.begin();
		     it != reactor.boards.end(); ++it) {
			if ((*it)->getMachineID() == tokens[1]) {
				reactor.prepareSwitch(*it);
				return tokens[1];
			}
		}
		throw CommandException("No machine with ID: " + tokens[1]);
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
	// TODO same as DeleteMachineCommand
	set<string> ids;
	for (Reactor::Boards::const_iterator it = reactor.boards.begin();
	     it != reactor.boards.end(); ++it) {
		ids.insert((*it)->getMachineID());
	}
	completeString(tokens, ids);
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

void RealTimeInfo::execute(const vector<TclObject*>& tokens,
                           TclObject& result) const
{
	unsigned long long delta = Timer::getTime() - reference;
	result.setDouble(delta / 1000000.0);
}

string RealTimeInfo::help(const vector<string>& tokens) const
{
	return "Returns the time in seconds since openMSX was started.";
}

} // namespace openmsx
