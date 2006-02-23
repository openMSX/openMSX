// $Id$

#include "Reactor.hh"
#include "CommandLineParser.hh"
#include "CommandController.hh"
#include "EventDistributor.hh"
#include "CommandConsole.hh"
#include "FileManipulator.hh"
#include "FilePool.hh"
#include "MSXMotherBoard.hh"
#include "Command.hh"
#include "Scheduler.hh"
#include "Schedulable.hh"
#include "MSXCPU.hh"
#include "CliComm.hh"
#include "Display.hh"
#include "Timer.hh"
#include "GlobalSettings.hh"
#include "BooleanSetting.hh"
#include "FileContext.hh"
#include "FileException.hh"
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

class ExitCPULoopSchedulable : public Schedulable
{
public:
	ExitCPULoopSchedulable(MSXMotherBoard& motherboard);
	void schedule();
private:
	virtual void executeUntil(const EmuTime& time, int userData);
	virtual const std::string& schedName() const;
	MSXMotherBoard& motherBoard;
};


Reactor::Reactor()
	: paused(false)
	, blockedCounter(0)
	, running(true)
	, pauseSetting(getCommandController().getGlobalSettings().
	                   getPauseSetting())
	, quitCommand(new QuitCommand(getCommandController(), *this))
{
	pauseSetting.attach(*this);

	getEventDistributor().registerEventListener(
		OPENMSX_QUIT_EVENT, *this);
}

Reactor::~Reactor()
{
	getEventDistributor().unregisterEventListener(
		OPENMSX_QUIT_EVENT, *this);

	pauseSetting.detach(*this);
}

CommandController& Reactor::getCommandController()
{
	if (!commandController.get()) {
		commandController.reset(new CommandController());
	}
	return *commandController;
}

EventDistributor& Reactor::getEventDistributor()
{
	if (!eventDistributor.get()) {
		eventDistributor.reset(new EventDistributor(*this));
	}
	return *eventDistributor;
}

CliComm& Reactor::getCliComm()
{
	if (!cliComm.get()) {
		cliComm.reset(new CliComm(getCommandController(),
		                          getEventDistributor()));
	}
	return *cliComm;
}

CommandConsole& Reactor::getCommandConsole()
{
	if (!commandConsole.get()) {
		commandConsole.reset(new CommandConsole(
			getCommandController(),
			getEventDistributor()));
	}
	return *commandConsole;
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

MSXMotherBoard& Reactor::getMotherBoard()
{
	if (!motherBoard.get()) {
		motherBoard.reset(new MSXMotherBoard(*this));
	}
	return *motherBoard;
}

void Reactor::run(CommandLineParser& parser)
{
	Display& display(getMotherBoard().getDisplay());

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
		getMotherBoard().powerUp();
	}
	
	Scheduler& scheduler(getMotherBoard().getScheduler());
	while (running) {
		getEventDistributor().deliverEvents();
		bool blocked = blockedCounter > 0;
		if (!blocked) blocked = !getMotherBoard().execute();
		if (blocked) {
			display.repaint();
			Timer::sleep(100 * 1000);
			scheduler.doPoll(); // TODO remove in future
			// TODO: Make Scheduler only responsible for events inside the MSX.
			//       All other events should be handled by the Reactor.
			scheduler.schedule(scheduler.getCurrentTime());
		}
	}
	getMotherBoard().doPowerDown(scheduler.getCurrentTime());

	// TODO clean this up, see comment in enterMainLoop
	schedulable.reset(); 
}

void Reactor::enterMainLoop()
{
	// this method can get called from different threads, so use Scheduler
	// to call MSXCPU::exitCPULoop()
	
	// TODO check for running can be removed if we cleanly destroy motherBoard
	//      before exiting
	if (motherBoard.get() && running) {
		if (!schedulable.get()) {
			schedulable.reset(new ExitCPULoopSchedulable(*motherBoard));
		}
		schedulable->schedule();
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
	} else {
		assert(false);
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


// class ExitCPULoopSchedulable

ExitCPULoopSchedulable::ExitCPULoopSchedulable(MSXMotherBoard& motherBoard_)
	: Schedulable(motherBoard_.getScheduler())
	, motherBoard(motherBoard_)
{
}

void ExitCPULoopSchedulable::schedule()
{
	setSyncPoint(Scheduler::ASAP);
}

void ExitCPULoopSchedulable::executeUntil(const EmuTime& /*time*/, int /*userData*/)
{
	motherBoard.getCPU().exitCPULoop();
}

const std::string& ExitCPULoopSchedulable::schedName() const
{
	static const std::string name = "ExitCPULoopSchedulable";
	return name;
}

} // namespace openmsx
