// $Id$

#include "MSXMotherBoard.hh"
#include "MSXDevice.hh"
#include "CommandController.hh"
#include "Scheduler.hh"
#include "MSXCPUInterface.hh"
#include "MSXCPU.hh"
#include "EmuTime.hh"
#include "PluggingController.hh"
#include "Mixer.hh"
#include "HardwareConfig.hh"
#include "DeviceFactory.hh"
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

MSXMotherBoard::MSXMotherBoard()
	: paused(false), powered(false)
	, needReset(false), needPowerDown(false), needPowerUp(false)
	, blockedCounter(1) // power off
	, emulationRunning(true)
        , pauseSetting(GlobalSettings::instance().getPauseSetting())
	, powerSetting(GlobalSettings::instance().getPowerSetting())
	, output(CliComm::instance())
	, quitCommand(*this)
	, resetCommand(*this)
{
	MSXCPU::instance().setMotherboard(this);

	pauseSetting.addListener(this);
	powerSetting.addListener(this);

	EventDistributor::instance().registerEventListener(
		OPENMSX_QUIT_EVENT, *this, EventDistributor::NATIVE);

	CommandController::instance().registerCommand(&quitCommand, "quit");
	CommandController::instance().registerCommand(&quitCommand, "exit");
	CommandController::instance().registerCommand(&resetCommand, "reset");
}

MSXMotherBoard::~MSXMotherBoard()
{
	CommandController::instance().unregisterCommand(&resetCommand, "reset");
	CommandController::instance().unregisterCommand(&quitCommand, "exit");
	CommandController::instance().unregisterCommand(&quitCommand, "quit");

	EventDistributor::instance().unregisterEventListener(
		OPENMSX_QUIT_EVENT, *this, EventDistributor::NATIVE);

	powerSetting.removeListener(this);
	pauseSetting.removeListener(this);

	// Destroy emulated MSX machine.
	for (Devices::iterator it = availableDevices.begin();
	     it != availableDevices.end(); ++it) {
		delete *it;
	}
	availableDevices.clear();
}

void MSXMotherBoard::addDevice(std::auto_ptr<MSXDevice> device)
{
	availableDevices.push_back(device.release());
}


void MSXMotherBoard::resetMSX()
{
	const EmuTime& time = Scheduler::instance().getCurrentTime();
	MSXCPUInterface::instance().reset();
	for (Devices::iterator it = availableDevices.begin();
	     it != availableDevices.end(); ++it) {
		(*it)->reset(time);
	}
	MSXCPU::instance().reset(time);
}

void MSXMotherBoard::powerDownMSX()
{
	const EmuTime& time = Scheduler::instance().getCurrentTime();
	for (Devices::iterator it = availableDevices.begin();
	     it != availableDevices.end(); ++it) {
		(*it)->powerDown(time);
	}
}

void MSXMotherBoard::powerUpMSX()
{
	const EmuTime& time = Scheduler::instance().getCurrentTime();
	MSXCPUInterface::instance().reset();
	for (Devices::iterator it = availableDevices.begin();
	     it != availableDevices.end(); ++it) {
		(*it)->powerUp(time);
	}
	MSXCPU::instance().reset(time);
}

void MSXMotherBoard::createDevices(const XMLElement& elem)
{
	const XMLElement::Children& children = elem.getChildren();
	for (XMLElement::Children::const_iterator it = children.begin();
	     it != children.end(); ++it) {
		const XMLElement& sub = **it;
		const string& name = sub.getName();
		if ((name == "primary") || (name == "secondary")) {
			createDevices(sub);
		} else {
			PRT_DEBUG("Instantiating: " << name);
			addDevice(DeviceFactory::create(sub, EmuTime::zero));
		}
	}
}

void MSXMotherBoard::run(bool power)
{
	// Initialise devices.
	//PRT_DEBUG(HardwareConfig::instance().getChild("devices").dump());
	createDevices(HardwareConfig::instance().getChild("devices"));
	
	// First execute auto commands.
	CommandController::instance().autoCommands();

	// Initialize.
	MSXCPUInterface::instance().reset();

	// Run.
	if (power) {
		powerOn();
		powerUpMSX();
	}
	
	while (emulationRunning) {
		if (blockedCounter > 0) {
			if (needPowerDown) {
				needPowerDown = false;
				powerDownMSX();
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
				powerUpMSX();
			}
			if (needReset) {
				needReset = false;
				resetMSX();
			}
			MSXCPU::instance().execute();
		}
	}

	powerOff();
	powerDownMSX();
}

void MSXMotherBoard::unpause()
{
	if (paused) {
		paused = false;
		output.update(CliComm::STATUS, "paused", "false");
		--blockedCounter;
	}
}

void MSXMotherBoard::pause()
{
	if (!paused) {
		paused = true;
		output.update(CliComm::STATUS, "paused", "true");
		++blockedCounter;
		MSXCPU::instance().exitCPULoop();
	}
}

void MSXMotherBoard::powerOn()
{
	if (!powered) {
		powered = true;
		powerSetting.setValue(true);
		EventDistributor::instance().distributeEvent(
			new LedEvent(LedEvent::POWER, true));
		--blockedCounter;
	}
}

void MSXMotherBoard::powerOff()
{
	if (powered) {
		powered = false;
		powerSetting.setValue(false);
		EventDistributor::instance().distributeEvent(
			new LedEvent(LedEvent::POWER, false));
		++blockedCounter;
		MSXCPU::instance().exitCPULoop();
	}
}

// SettingListener
void MSXMotherBoard::update(const Setting* setting)
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
bool MSXMotherBoard::signalEvent(const Event& /*event*/)
{
	emulationRunning = false;
	MSXCPU::instance().exitCPULoop();
	return true;
}


// class QuitCommand

MSXMotherBoard::QuitCommand::QuitCommand(MSXMotherBoard& parent_)
	: parent(parent_)
{
}

string MSXMotherBoard::QuitCommand::execute(const vector<string>& /*tokens*/)
{
	parent.emulationRunning = false;
	MSXCPU::instance().exitCPULoop();
	return "";
}

string MSXMotherBoard::QuitCommand::help(const vector<string>& /*tokens*/) const
{
	return "Use this command to stop the emulator\n";
}


// class ResetCmd

MSXMotherBoard::ResetCmd::ResetCmd(MSXMotherBoard& parent_)
	: parent(parent_)
{
}

string MSXMotherBoard::ResetCmd::execute(const vector<string>& /*tokens*/)
{
	parent.needReset = true;
	MSXCPU::instance().exitCPULoop();
	return "";
}
string MSXMotherBoard::ResetCmd::help(const vector<string>& /*tokens*/) const
{
	return "Resets the MSX.\n";
}


} // namespace openmsx



