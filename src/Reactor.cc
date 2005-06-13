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
#include "InputEventGenerator.hh"
#include "GlobalSettings.hh"
#include "BooleanSetting.hh"
#include "Interpreter.hh"
#include <cassert>

using std::string;
using std::vector;

namespace openmsx {

Reactor::Reactor()
	: paused(false)
	, blockedCounter(0)
	, running(true)
	, pauseSetting(GlobalSettings::instance().getPauseSetting())
	, output(CliComm::instance())
	, quitCommand(*this)
{
	pauseSetting.addListener(this);

	EventDistributor::instance().registerEventListener(
		OPENMSX_QUIT_EVENT, *this, EventDistributor::NATIVE);

	CommandController::instance().registerCommand(&quitCommand, "quit");
	CommandController::instance().registerCommand(&quitCommand, "exit");
}

Reactor::~Reactor()
{
	CommandController::instance().unregisterCommand(&quitCommand, "exit");
	CommandController::instance().unregisterCommand(&quitCommand, "quit");

	EventDistributor::instance().unregisterEventListener(
		OPENMSX_QUIT_EVENT, *this, EventDistributor::NATIVE);

	pauseSetting.removeListener(this);
}

void Reactor::run(bool autoRun)
{
	CommandController& commandController = CommandController::instance();
	Display& display = Display::instance();
	InputEventGenerator& inputEventGenerator = InputEventGenerator::instance();
	Interpreter& interpreter = Interpreter::instance();
	Scheduler& scheduler = Scheduler::instance();

	// First execute auto commands.
	commandController.autoCommands();

	// Initialize MSX machine.
	// TODO: Make this into a command.
	motherboard.reset(new MSXMotherBoard());

	// Run.
	if (autoRun) {
		commandController.executeCommand("set power on");
	}
	while (running) {
		bool blocked = blockedCounter > 0;
		if (!blocked) blocked = !motherboard->execute();
		if (blocked) {
			display.repaint();
			Timer::sleep(100 * 1000);
			inputEventGenerator.poll();
			interpreter.poll();
			// TODO: Make Scheduler only responsible for events inside the MSX.
			//       All other events should be handled by the Reactor.
			scheduler.schedule(scheduler.getCurrentTime());
		}
	}

	motherboard->powerDownMSX();
}

void Reactor::unpause()
{
	if (paused) {
		paused = false;
		output.update(CliComm::STATUS, "paused", "false");
		unblock();
	}
}

void Reactor::pause()
{
	if (!paused) {
		paused = true;
		output.update(CliComm::STATUS, "paused", "true");
		block();
	}
}

void Reactor::block()
{
	++blockedCounter;
	MSXCPU::instance().exitCPULoop();
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
bool Reactor::signalEvent(const Event& event)
{
	if (event.getType() == OPENMSX_QUIT_EVENT) {
		running = false;
		MSXCPU::instance().exitCPULoop();
	} else {
		assert(false);
	}
	return true;
}


// class QuitCommand
// TODO: Unify QuitCommand and OPENMSX_QUIT_EVENT.

Reactor::QuitCommand::QuitCommand(Reactor& parent_)
	: parent(parent_)
{
}

string Reactor::QuitCommand::execute(const vector<string>& /*tokens*/)
{
	parent.running = false;
	MSXCPU::instance().exitCPULoop();
	return "";
}

string Reactor::QuitCommand::help(const vector<string>& /*tokens*/) const
{
	return "Use this command to stop the emulator\n";
}

} // namespace openmsx



