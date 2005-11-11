// $Id$

#include "Reactor.hh"
#include "MSXMotherBoard.hh"
#include "CommandController.hh"
#include "Scheduler.hh"
#include "MSXCPUInterface.hh"
#include "MSXCPU.hh"
#include "CliComm.hh"
#include "EventDistributor.hh"
#include "Display.hh"
#include "Timer.hh"
#include "GlobalSettings.hh"
#include "BooleanSetting.hh"
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

void Reactor::run(bool autoRun)
{
	CommandController& commandController(motherBoard.getCommandController());
	Display& display(motherBoard.getDisplay());
	Scheduler& scheduler(motherBoard.getScheduler());

	// First execute auto commands.
	commandController.autoCommands();

	// Run.
	if (autoRun) {
		commandController.executeCommand("set power on");
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


// SettingListener
void Reactor::update(const Setting* setting)
{
	if (setting == &pauseSetting) {
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



