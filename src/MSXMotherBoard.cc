// $Id$

#include "MSXMotherBoard.hh"
#include "MSXDevice.hh"
#include "Scheduler.hh"
#include "CartridgeSlotManager.hh"
#include "PluggingController.hh"
#include "Debugger.hh"
#include "Mixer.hh"
#include "DummyDevice.hh"
#include "MSXCPUInterface.hh"
#include "MSXCPU.hh"
#include "PanasonicMemory.hh"
#include "MSXDeviceSwitch.hh"
#include "CassettePort.hh"
#include "RenShaTurbo.hh"
#include "CommandConsole.hh"
#include "Display.hh"
#include "RenderSettings.hh"
#include "EmuTime.hh"
#include "HardwareConfig.hh"
#include "DeviceFactory.hh"
#include "LedEvent.hh"
#include "CliComm.hh"
#include "EventDistributor.hh"
#include "UserInputEventDistributor.hh"
#include "GlobalSettings.hh"
#include "BooleanSetting.hh"
#include "FileContext.hh"
#include <cassert>

using std::string;
using std::vector;

namespace openmsx {

MSXMotherBoard::MSXMotherBoard()
	: powered(false)
	, blockedCounter(0)
	, powerSetting(GlobalSettings::instance().getPowerSetting())
	, output(CliComm::instance())
	, debugger(new Debugger())
	, msxCpu(new MSXCPU(getDebugger()))
	, resetCommand(*this)
{
	getCPU().setMotherboard(this);
	getMixer().mute(); // powered down

	powerSetting.addListener(this);

	CommandController::instance().registerCommand(&resetCommand, "reset");
}

MSXMotherBoard::~MSXMotherBoard()
{
	CommandController::instance().unregisterCommand(&resetCommand, "reset");

	powerSetting.removeListener(this);

	// Destroy emulated MSX machine.
	for (Devices::iterator it = availableDevices.begin();
	     it != availableDevices.end(); ++it) {
		delete *it;
	}
	availableDevices.clear();
}

CartridgeSlotManager& MSXMotherBoard::getSlotManager()
{
	if (!slotManager.get()) {
		slotManager.reset(new CartridgeSlotManager(*this));
	}
	return *slotManager;
}

UserInputEventDistributor& MSXMotherBoard::getUserInputEventDistributor()
{
	if (!userInputEventDistributor.get()) {
		userInputEventDistributor.reset(
			new UserInputEventDistributor(EventDistributor::instance()));
	}
	return *userInputEventDistributor;
}


PluggingController& MSXMotherBoard::getPluggingController()
{
	if (!pluggingController.get()) {
		pluggingController.reset(new PluggingController(*this));
	}
	return *pluggingController;
}

Debugger& MSXMotherBoard::getDebugger()
{
	return *debugger;
}

Mixer& MSXMotherBoard::getMixer()
{
	if (!mixer.get()) {
		mixer.reset(new Mixer());
	}
	return *mixer;
}

DummyDevice& MSXMotherBoard::getDummyDevice()
{
	if (!dummyDevice.get()) {
		dummyDeviceConfig.reset(new XMLElement("Dummy"));
		dummyDeviceConfig->addAttribute("id", "empty");
		dummyDeviceConfig->setFileContext(std::auto_ptr<FileContext>(
			new SystemFileContext()));
		dummyDevice.reset(
			new DummyDevice(*this, *dummyDeviceConfig, EmuTime::zero));
	}
	return *dummyDevice;
}

MSXCPUInterface& MSXMotherBoard::getCPUInterface()
{
	if (!msxCpuInterface.get()) {
		// TODO assert hw config already loaded
		msxCpuInterface = MSXCPUInterface::create(
			*this, HardwareConfig::instance());
	}
	return *msxCpuInterface; 
}

PanasonicMemory& MSXMotherBoard::getPanasonicMemory()
{
	if (!panasonicMemory.get()) {
		panasonicMemory.reset(new PanasonicMemory(*this));
	}
	return *panasonicMemory;
}

MSXDeviceSwitch& MSXMotherBoard::getDeviceSwitch()
{
	if (!deviceSwitch.get()) {
		devSwitchConfig.reset(new XMLElement("DeviceSwitch"));
		devSwitchConfig->addAttribute("id", "DeviceSwitch");
		devSwitchConfig->setFileContext(std::auto_ptr<FileContext>(
			new SystemFileContext()));
		deviceSwitch.reset(
			new MSXDeviceSwitch(*this, *devSwitchConfig, EmuTime::zero));
	}
	return *deviceSwitch;
}

CassettePortInterface& MSXMotherBoard::getCassettePort()
{
	if (!cassettePort.get()) {
		if (HardwareConfig::instance().findChild("CassettePort")) {
			cassettePort.reset(new CassettePort(*this));
		} else {
			cassettePort.reset(new DummyCassettePort());
		}
	}
	return *cassettePort;
}

RenShaTurbo& MSXMotherBoard::getRenShaTurbo()
{
	if (!renShaTurbo.get()) {
		renShaTurbo.reset(new RenShaTurbo());
	}
	return *renShaTurbo;
}

CommandConsole& MSXMotherBoard::getCommandConsole()
{
	if (!commandConsole.get()) {
		commandConsole.reset(new CommandConsole());
	}
	return *commandConsole;
}

RenderSettings& MSXMotherBoard::getRenderSettings()
{
	if (!renderSettings.get()) {
		renderSettings.reset(new RenderSettings());
	}
	return *renderSettings;
}

Display& MSXMotherBoard::getDisplay()
{
	if (!display.get()) {
		display.reset(new Display(getUserInputEventDistributor(),
		                          getRenderSettings(),
		                          getCommandConsole()));
	}
	return *display;
}

void MSXMotherBoard::readConfig()
{
	getSlotManager().readConfig();
	createDevices(HardwareConfig::instance().getChild("devices"));
}

bool MSXMotherBoard::execute()
{
	if (!powered || blockedCounter) {
		return false;
	}

	getCPU().execute();
	return true;
}

void MSXMotherBoard::block()
{
	++blockedCounter;
	getCPU().exitCPULoop();
	getMixer().mute();
}

void MSXMotherBoard::unblock()
{
	--blockedCounter;
	assert(blockedCounter >= 0);
	getMixer().unmute();
}

void MSXMotherBoard::pause()
{
	getCPU().setPaused(true);
	getMixer().mute();
}

void MSXMotherBoard::unpause()
{
	getCPU().setPaused(false);
	getMixer().unmute();
}

void MSXMotherBoard::addDevice(std::auto_ptr<MSXDevice> device)
{
	availableDevices.push_back(device.release());
}

void MSXMotherBoard::resetMSX()
{
	const EmuTime& time = Scheduler::instance().getCurrentTime();
	getCPUInterface().reset();
	for (Devices::iterator it = availableDevices.begin();
	     it != availableDevices.end(); ++it) {
		(*it)->reset(time);
	}
	getCPU().reset(time);
}

void MSXMotherBoard::powerUpMSX()
{
	if (powered) return;

	powered = true;
	// TODO: If our "powered" field is always equal to the power setting,
	//       there is not really a point in keeping it.
	assert(powerSetting.getValue() == powered);
	powerSetting.setValue(true);
	// TODO: We could make the power LED a device, so we don't have to handle
	//       it separately here.
	EventDistributor::instance().distributeEvent(
		new LedEvent(LedEvent::POWER, true));

	const EmuTime& time = Scheduler::instance().getCurrentTime();
	getCPUInterface().reset();
	for (Devices::iterator it = availableDevices.begin();
	     it != availableDevices.end(); ++it) {
		(*it)->powerUp(time);
	}
	getCPU().reset(time);
	getMixer().unmute();
}

void MSXMotherBoard::powerDownMSX()
{
	if (!powered) return;

	powered = false;
	// TODO: This assertion fails in 1 case: when quitting with a running MSX.
	//       How do we want the Reactor to shutdown: immediately or after
	//       handling all pending commands/events/updates?
	//assert(powerSetting.getValue() == powered);
	powerSetting.setValue(false);
	EventDistributor::instance().distributeEvent(
		new LedEvent(LedEvent::POWER, false));

	getCPU().exitCPULoop();
	getMixer().mute();

	const EmuTime& time = Scheduler::instance().getCurrentTime();
	for (Devices::iterator it = availableDevices.begin();
	     it != availableDevices.end(); ++it) {
		(*it)->powerDown(time);
	}
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
			std::auto_ptr<MSXDevice> device(
				DeviceFactory::create(*this, sub, EmuTime::zero));
			if (device.get()) {
				addDevice(device);
			} else {
				output.printWarning("Deprecated device: \"" +
					name + "\", please upgrade your "
					"machine descriptions.");
			}
		}
	}
}

// SettingListener
void MSXMotherBoard::update(const Setting* setting)
{
	if (setting == &powerSetting) {
		if (powerSetting.getValue()) {
			powerUpMSX();
		} else {
			powerDownMSX();
		}
	} else {
		assert(false);
	}
}

// inner class ResetCmd:

MSXMotherBoard::ResetCmd::ResetCmd(MSXMotherBoard& parent_)
	: parent(parent_)
{
}

string MSXMotherBoard::ResetCmd::execute(const vector<string>& /*tokens*/)
{
	parent.resetMSX();
	return "";
}

string MSXMotherBoard::ResetCmd::help(const vector<string>& /*tokens*/) const
{
	return "Resets the MSX.\n";
}

} // namespace openmsx
