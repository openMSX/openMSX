// $Id$

#include "Reactor.hh"
#include "MSXMotherBoard.hh"
#include "CommandLineParser.hh"
#include "CommandController.hh"
#include "Scheduler.hh"
#include "MSXCPU.hh"
#include "CliComm.hh"
#include "EventDistributor.hh"
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

Reactor::Reactor(MSXMotherBoard& motherBoard_)
	: paused(false)
	, blockedCounter(0)
	, running(true)
	, motherBoard(motherBoard_)
	, pauseSetting(motherBoard.getCommandController().getGlobalSettings().
	                   getPauseSetting())
	, cliComm(motherBoard.getCliComm())
	, quitCommand(motherBoard.getCommandController(), *this)
{
	pauseSetting.attach(*this);

	motherBoard.getEventDistributor().registerEventListener(
		OPENMSX_QUIT_EVENT, *this, EventDistributor::DETACHED);
}

Reactor::~Reactor()
{
	motherBoard.getEventDistributor().unregisterEventListener(
		OPENMSX_QUIT_EVENT, *this, EventDistributor::DETACHED);

	pauseSetting.detach(*this);
}

void Reactor::run(CommandLineParser& parser)
{
	CommandController& commandController(motherBoard.getCommandController());
	Display& display(motherBoard.getDisplay());
	Scheduler& scheduler(motherBoard.getScheduler());

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
		//commandController.executeCommand("set power on");
		motherBoard.powerUp();
	}
	while (running) {
		bool blocked = blockedCounter > 0;
		if (!blocked) blocked = !motherBoard.execute();
		if (blocked) {
			display.repaint();
			Timer::sleep(100 * 1000);
			motherBoard.getScheduler().doPoll();
			// TODO: Make Scheduler only responsible for events inside the MSX.
			//       All other events should be handled by the Reactor.
			scheduler.schedule(scheduler.getCurrentTime());
		}
	}
	motherBoard.doPowerDown(scheduler.getCurrentTime());
}

void Reactor::unpause()
{
	if (paused) {
		paused = false;
		cliComm.update(CliComm::STATUS, "paused", "false");
		unblock();
	}
}

void Reactor::pause()
{
	if (!paused) {
		paused = true;
		cliComm.update(CliComm::STATUS, "paused", "true");
		block();
	}
}

void Reactor::block()
{
	++blockedCounter;
	motherBoard.getCPU().exitCPULoop();
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
		running = false;
		motherBoard.getCPU().exitCPULoop();
	} else {
		assert(false);
	}
}


// class QuitCommand
// TODO: Unify QuitCommand and OPENMSX_QUIT_EVENT.

Reactor::QuitCommand::QuitCommand(CommandController& commandController,
                                  Reactor& reactor_)
	: SimpleCommand(commandController, "exit")
	, reactor(reactor_)
{
}

string Reactor::QuitCommand::execute(const vector<string>& /*tokens*/)
{
	reactor.running = false;
	reactor.motherBoard.getCPU().exitCPULoop();
	return "";
}

string Reactor::QuitCommand::help(const vector<string>& /*tokens*/) const
{
	return "Use this command to stop the emulator\n";
}

} // namespace openmsx
