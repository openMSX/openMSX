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
#include "Display.hh"
#include "Mixer.hh"
#include "IconStatus.hh"
#include "AviRecorder.hh"
#include "Timer.hh"
#include "Alarm.hh"
#include "GlobalSettings.hh"
#include "BooleanSetting.hh"
#include "HardwareConfig.hh"
#include "FileContext.hh"
#include "FileException.hh"
#include "FileOperations.hh"
#include "ReadDir.hh"
#include "Thread.hh"
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
	virtual string help   (const vector<string>& tokens) const;
	virtual void tabCompletion(vector<string>& tokens) const;
private:
	const string configName;
};


Reactor::Reactor()
	: paused(false)
	, blockedCounter(0)
	, running(true)
	, mbSem(1)
	, pauseSetting(getGlobalSettings().getPauseSetting())
	, quitCommand(new QuitCommand(getCommandController(), *this))
	, machineCommand(new MachineCommand(getCommandController(), *this))
	, testMachineCommand(new TestMachineCommand(getCommandController(), *this))
	, aviRecordCommand(new AviRecorder(*this))
	, extensionInfo(new ConfigInfo(
	       getGlobalCommandController().getOpenMSXInfoCommand(),
	       "extensions"))
	, machineInfo(new ConfigInfo(
	       getGlobalCommandController().getOpenMSXInfoCommand(),
	       "machines"))
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

void Reactor::createMotherBoard(const string& machine)
{
	assert(!motherBoard.get());
	prepareMotherBoard(machine);
	switchMotherBoard(newMotherBoard);
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
	std::auto_ptr<MSXMotherBoard> tmp(new MSXMotherBoard(*this));
	tmp->loadMachine(machine);
	newMotherBoard = tmp;
	enterMainLoop();
	return *newMotherBoard;
}

void Reactor::switchMotherBoard(std::auto_ptr<MSXMotherBoard> mb)
{
	assert(Thread::isMainThread());
	ScopedLock lock(mbSem);
	motherBoard = mb;
	getEventDistributor().distributeEvent(
		new SimpleEvent<OPENMSX_MACHINE_LOADED_EVENT>());
	getGlobalCliComm().update(
		CliComm::HARDWARE, motherBoard->getMachineID(), "select");
}

MSXMotherBoard* Reactor::getMotherBoard() const
{
	assert(Thread::isMainThread());
	return motherBoard.get();
}

void Reactor::deleteMotherBoard()
{
	assert(Thread::isMainThread());
	ScopedLock lock(mbSem);
	motherBoard.reset();
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
	if (motherBoard.get()) {
		motherBoard->exitCPULoopAsync();
	}
}

void Reactor::run(CommandLineParser& parser)
{
	Display& display = getDisplay();
	GlobalCommandController& commandController = getGlobalCommandController();

	// select initial machine before executing scripts
	if (newMotherBoard.get()) {
		switchMotherBoard(newMotherBoard);
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
		if (newMotherBoard.get()) {
			switchMotherBoard(newMotherBoard);
			assert(motherBoard.get());
			assert(!newMotherBoard.get());
		}
		getEventDistributor().deliverEvents();
		MSXMotherBoard* motherboard = getMotherBoard();
		bool blocked = (blockedCounter > 0) || !motherboard;
		if (!blocked) blocked = !motherboard->execute();
		if (blocked) {
			display.repaint();
			Timer::sleep(100 * 1000);
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
//
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
				configName, tokens[2]->getString());
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

} // namespace openmsx
