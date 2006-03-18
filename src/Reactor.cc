// $Id$

#include "Reactor.hh"
#include "CommandLineParser.hh"
#include "EventDistributor.hh"
#include "CommandController.hh"
#include "CommandConsole.hh"
#include "InputEventGenerator.hh"
#include "FileManipulator.hh"
#include "FilePool.hh"
#include "MSXMotherBoard.hh"
#include "Command.hh"
#include "CliComm.hh"
#include "Display.hh"
#include "IconStatus.hh"
#include "Timer.hh"
#include "Alarm.hh"
#include "GlobalSettings.hh"
#include "BooleanSetting.hh"
#include "FileContext.hh"
#include "FileException.hh"
#include "FileOperations.hh"
#include "ReadDir.hh"
#include <cassert>

using std::string;
using std::vector;

namespace openmsx {

class QuitCommand : public SimpleCommand
{
public:
	QuitCommand(CommandController& commandController, Reactor& reactor);
	virtual std::string execute(const std::vector<std::string>& tokens);
	virtual std::string help(const std::vector<std::string>& tokens) const;
private:
	Reactor& reactor;
};

class PollEventGenerator : private Alarm
{
public:
	PollEventGenerator(EventDistributor& eventDistributor);
	~PollEventGenerator();
private:
	virtual bool alarm();
	EventDistributor& eventDistributor;
};


Reactor::Reactor()
	: paused(false)
	, blockedCounter(0)
	, motherBoard(NULL)
	, pauseSetting(getCommandController().getGlobalSettings().
	                   getPauseSetting())
	, quitCommand(new QuitCommand(getCommandController(), *this))
{
	createMachineSetting();

	pauseSetting.attach(*this);
	getMachineSetting().attach(*this);

	getEventDistributor().registerEventListener(
		OPENMSX_QUIT_EVENT, *this);
}

Reactor::~Reactor()
{
	deleteMotherBoard();

	getEventDistributor().unregisterEventListener(
		OPENMSX_QUIT_EVENT, *this);

	getMachineSetting().detach(*this);
	pauseSetting.detach(*this);
}

EventDistributor& Reactor::getEventDistributor()
{
	if (!eventDistributor.get()) {
		eventDistributor.reset(new EventDistributor(*this));
	}
	return *eventDistributor;
}

CommandController& Reactor::getCommandController()
{
	if (!commandController.get()) {
		commandController.reset(new CommandController(getEventDistributor()));
	}
	return *commandController;
}

CliComm& Reactor::getCliComm()
{
	if (!cliComm.get()) {
		cliComm.reset(new CliComm(getCommandController(),
		                          getEventDistributor()));
	}
	return *cliComm;
}

InputEventGenerator& Reactor::getInputEventGenerator()
{
	if (!inputEventGenerator.get()) {
		inputEventGenerator.reset(new InputEventGenerator(
			getCommandController(), getEventDistributor()));
	}
	return *inputEventGenerator;
}

CommandConsole& Reactor::getCommandConsole()
{
	if (!commandConsole.get()) {
		commandConsole.reset(new CommandConsole(
			getCommandController(), getEventDistributor()));
	}
	return *commandConsole;
}

Display& Reactor::getDisplay()
{
	if (!display.get()) {
		display.reset(new Display(*this));
		display->createVideoSystem();
	}
	return *display;
}

IconStatus& Reactor::getIconStatus()
{
	if (!iconStatus.get()) {
		iconStatus.reset(new IconStatus(getEventDistributor()));
	}
	return *iconStatus;
}

FileManipulator& Reactor::getFileManipulator()
{
	if (!fileManipulator.get()) {
		fileManipulator.reset(new FileManipulator(
			getCommandController()));
	}
	return *fileManipulator;
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

static int select(const string& basepath, const struct dirent* d)
{
	// entry must be a directory and must contain the file "hardwareconfig.xml"
	string name = basepath + '/' + d->d_name;
	return FileOperations::isDirectory(name) &&
	       FileOperations::isRegularFile(name + "/hardwareconfig.xml");
}

static void searchMachines(const string& basepath, EnumSetting<int>::Map& machines)
{
	static int unique = 1;
	ReadDir dir(basepath);
	while (dirent* d = dir.getEntry()) {
		if (select(basepath, d)) {
			machines[d->d_name] = unique++; // dummy value
		}
	}
}

void Reactor::createMachineSetting()
{
	EnumSetting<int>::Map machines;

	SystemFileContext context;
	const vector<string>& paths = context.getPaths();
	for (vector<string>::const_iterator it = paths.begin();
	     it != paths.end(); ++it) {
		searchMachines(*it + "machines", machines);
	}

	machines["C-BIOS_MSX2+"] = 0; // default machine

	machineSetting.reset(new EnumSetting<int>(
		getCommandController(), "machine",
		"default machine (takes effect next time openMSX is started)",
		0, machines));
}

MSXMotherBoard& Reactor::createMotherBoard(const std::string& machine)
{
	assert(!motherBoard);
	motherBoard = new MSXMotherBoard(*this);
	motherBoard->loadMachine(machine);
	return *motherBoard;
}

MSXMotherBoard* Reactor::getMotherBoard() const
{
	return motherBoard;
}

void Reactor::deleteMotherBoard()
{
	// TODO properly fix this
	MSXMotherBoard* tmp = motherBoard; // reduce chance of race conditions
	motherBoard = NULL;
	delete tmp;
}

void Reactor::doSwitchMachine()
{
	deleteMotherBoard();
	try {
		createMotherBoard(getMachineSetting().getValueString());
	} catch (MSXException& e) {
		// TODO
	}
}

void Reactor::run(CommandLineParser& parser)
{
	Display& display = getDisplay();

	// execute init.tcl
	try {
		SystemFileContext context(true); // only in system dir
		getCommandController().source(context.resolve("init.tcl"));
	} catch (FileException& e) {
		// no init.tcl, ignore
	}

	// execute startup scripts
	const CommandLineParser::Scripts& scripts = parser.getStartupScripts();
	for (CommandLineParser::Scripts::const_iterator it = scripts.begin();
	     it != scripts.end(); ++it) {
		try {
			UserFileContext context(getCommandController());
			getCommandController().source(context.resolve(*it));
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
		//getCommandController().executeCommand("set power on");
		MSXMotherBoard* motherboard = getMotherBoard();
		if (motherboard) {
			motherboard->powerUp();
		}
	}

	PollEventGenerator pollEventGenerator(getEventDistributor());

	running = true;
	switchMachineFlag = false;
	while (running) {
		getEventDistributor().deliverEvents();
		MSXMotherBoard* motherboard = getMotherBoard();
		bool blocked = (blockedCounter > 0) || !motherboard;
		if (!blocked) blocked = !motherboard->execute();
		if (blocked) {
			display.repaint();
			Timer::sleep(100 * 1000);
		}
		if (switchMachineFlag) {
			switchMachineFlag = false;
			doSwitchMachine();
		}
	}
}

void Reactor::enterMainLoop()
{
	// Note: this method can get called from different threads
	//  TODO investigate why test on 'running' is needed
	//   there seem to be some race condition between when main thread is
	//   deleting MSXMotherBoard while another threat still calls this
	//   method.
	MSXMotherBoard* motherboard = getMotherBoard();
	if (motherboard && running) {
		motherBoard->exitCPULoop();
	}
}

void Reactor::unpause()
{
	if (paused) {
		paused = false;
		getCliComm().update(CliComm::STATUS, "paused", "false");
		unblock();
	}
}

void Reactor::pause()
{
	if (!paused) {
		paused = true;
		getCliComm().update(CliComm::STATUS, "paused", "true");
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
	} else if (&setting == machineSetting.get()) {
		switchMachineFlag = true;
		enterMainLoop();
	}
}

// EventListener
void Reactor::signalEvent(const Event& event)
{
	if (event.getType() == OPENMSX_QUIT_EVENT) {
		enterMainLoop();
		running = false;
	} else {
		assert(false);
	}
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

} // namespace openmsx
