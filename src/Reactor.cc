// $Id$

#include "Reactor.hh"
#include "MSXMotherBoard.hh"
#include "CommandController.hh"
#include "Scheduler.hh"
#include "MSXCPUInterface.hh"
#include "MSXCPU.hh"
#include "LedEvent.hh"
#include "CliComm.hh"
#include "EventDistributor.hh"
#include "Display.hh"
#include "Timer.hh"
#include "InputEventGenerator.hh"
#include "GlobalSettings.hh"
#include "BooleanSetting.hh"
#include "Interpreter.hh"

using std::string;
using std::vector;

namespace openmsx {

Reactor::Reactor()
	: paused(false), powered(false)
	, needReset(false), needPowerDown(false), needPowerUp(false)
	, blockedCounter(1) // power off
	, emulationRunning(true)
	, pauseSetting(GlobalSettings::instance().getPauseSetting())
	, powerSetting(GlobalSettings::instance().getPowerSetting())
	, output(CliComm::instance())
	, motherboard()
	, quitCommand(*this)
	, resetCommand(*this)
{
	pauseSetting.addListener(this);
	powerSetting.addListener(this);

	EventDistributor::instance().registerEventListener(
		OPENMSX_QUIT_EVENT, *this, EventDistributor::NATIVE);

	CommandController::instance().registerCommand(&quitCommand, "quit");
	CommandController::instance().registerCommand(&quitCommand, "exit");
	CommandController::instance().registerCommand(&resetCommand, "reset");
}

Reactor::~Reactor()
{
	CommandController::instance().unregisterCommand(&resetCommand, "reset");
	CommandController::instance().unregisterCommand(&quitCommand, "exit");
	CommandController::instance().unregisterCommand(&quitCommand, "quit");

	EventDistributor::instance().unregisterEventListener(
		OPENMSX_QUIT_EVENT, *this, EventDistributor::NATIVE);

	powerSetting.removeListener(this);
	pauseSetting.removeListener(this);
}

void Reactor::run(bool power)
{
	// First execute auto commands.
	CommandController::instance().autoCommands();

	// Initialize.
	MSXCPUInterface::instance().reset();

	// Run.
	if (power) {
		powerOn();
		needPowerUp = true;
	}

	while (emulationRunning) {
		if (blockedCounter > 0) {
			if (needPowerDown) {
				needPowerDown = false;
				motherboard.powerDownMSX();
			}
			Display::instance().repaint();
			Timer::sleep(100 * 1000);
			InputEventGenerator::instance().poll();
			Interpreter::instance().poll();
			Scheduler& scheduler = Scheduler::instance();
			scheduler.schedule(scheduler.getCurrentTime());
		} else {
			if (needPowerUp) {
				needPowerUp = false;
				motherboard.powerUpMSX();
			}
			if (needReset) {
				needReset = false;
				motherboard.resetMSX();
			}
			motherboard.execute();
		}
	}

	powerOff();
	motherboard.powerDownMSX();
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

void Reactor::powerOn()
{
	if (!powered) {
		powered = true;
		powerSetting.setValue(true);
		EventDistributor::instance().distributeEvent(
			new LedEvent(LedEvent::POWER, true));
		unblock();
	}
}

void Reactor::powerOff()
{
	if (powered) {
		powered = false;
		powerSetting.setValue(false);
		EventDistributor::instance().distributeEvent(
			new LedEvent(LedEvent::POWER, false));
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
}


// SettingListener
void Reactor::update(const Setting* setting)
{
	if (setting == &powerSetting) {
		if (powerSetting.getValue()) {
			powerOn();
			needPowerUp = true;
		} else {
			powerOff();
			needPowerDown = true;
		}
	} else if (setting == &pauseSetting) {
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
bool Reactor::signalEvent(const Event& /*event*/)
{
	emulationRunning = false;
	MSXCPU::instance().exitCPULoop();
	return true;
}


// class QuitCommand

Reactor::QuitCommand::QuitCommand(Reactor& parent_)
	: parent(parent_)
{
}

string Reactor::QuitCommand::execute(const vector<string>& /*tokens*/)
{
	parent.emulationRunning = false;
	MSXCPU::instance().exitCPULoop();
	return "";
}

string Reactor::QuitCommand::help(const vector<string>& /*tokens*/) const
{
	return "Use this command to stop the emulator\n";
}


// class ResetCmd

Reactor::ResetCmd::ResetCmd(Reactor& parent_)
	: parent(parent_)
{
}

string Reactor::ResetCmd::execute(const vector<string>& /*tokens*/)
{
	parent.needReset = true;
	MSXCPU::instance().exitCPULoop();
	return "";
}
string Reactor::ResetCmd::help(const vector<string>& /*tokens*/) const
{
	return "Resets the MSX.\n";
}


} // namespace openmsx



